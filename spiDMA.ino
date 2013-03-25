//  teensy  SPI + DMA   TODO
//   CS,MOSI,MISO,CLK  pins 10-13
//   dma ch0 xmit (mem to SPI)     ch1 recv  (SPI to mem)
//  try  byte, 16-bit, and 32-bit dma transfers  ? SPI 16-bit
//  do exchange, read/write 0xff,  write/sink read
// don't need ISR for now, spin wait
// ref https://github.com/hughpyle/teensy-i2s  our mem2mem

#include <SPI.h>

#define CSpin 10
#define RSER_RFDF_DIRS (1<<16)
#define RSER_RFDF_RE (1<<17)
#define RSER_TFFF_DIRS (1<<24)
#define RSER_TFFF_RE (1<<25)


volatile int DMAdone=0;
#define DMA_CINT_CINT(n) ((uint8_t)(n & 3)<<0) // Clear Interrupt Request


void dma_ch0_isr(void)
{
  DMAdone=1;
  DMA_CINT = DMA_CINT_CINT(0); // use the Clear Intr. Request register
}

void dma_ch1_isr(void)
{
  DMAdone=1;
  DMA_CINT = DMA_CINT_CINT(1); // use the Clear Intr. Request register
}

void spidma_init() {
		// set size, and SPI address  for xmit and recv dma
		//  TODO enable DMA in SPI regs ...
		SPI0_RSER =  RSER_RFDF_DIRS | RSER_RFDF_RE | RSER_TFFF_DIRS | RSER_TFFF_RE;
    	DMA_TCD0_DADDR = &SPI0_PUSHR;
        DMA_TCD0_DOFF = 0;  // no increment
    	DMA_TCD1_SADDR = &SPI0_POPR;
        DMA_TCD1_SOFF = 0;  // no increment
        DMA_TCD0_ATTR = DMA_TCD_ATTR_SSIZE(0) | DMA_TCD_ATTR_DSIZE(0); //8 bit
        DMA_TCD1_ATTR = DMA_TCD_ATTR_SSIZE(0) | DMA_TCD_ATTR_DSIZE(0); //8 bit
        DMA_TCD0_SLAST = 0;
        DMA_TCD1_SLAST = 0;
        DMA_TCD0_CITER_ELINKNO = 1;
        DMA_TCD1_CITER_ELINKNO = 1;
        DMA_TCD0_DLASTSGA = 0;
        DMA_TCD1_DLASTSGA = 0;
        DMA_TCD0_BITER_ELINKNO = 1;
        DMA_TCD1_BITER_ELINKNO = 1;
        DMAdone=0;
//		NVIC_ENABLE_IRQ(IRQ_DMA_CH1);
//      DMA_TCD1_CSR = DMA_TCD_CSR_INTMAJOR; // interrupt on major loop completion
}

void myspi_init() {
	// set up SPI speed and dma   ? FIFO
	SPI.begin();
	SPI.setBitOrder(MSBFIRST);
	SPI.setDataMode(SPI_MODE0);
//	SPI.setClockDivider(SPI_CLOCK_DIV2);   // DIV4 default 4mhz
	pinMode(CSpin,OUTPUT);

	spidma_init();
}

void spidma_transfer(char *inbuf, char *outbuf, int bytes) {
	digitalWrite(CSpin,LOW);
    DMA_TCD0_SADDR = outbuf;
    DMA_TCD1_DADDR = inbuf;
    DMA_TCD0_NBYTES_MLNO = bytes;
    DMA_TCD1_NBYTES_MLNO = bytes;
    DMA_TCD0_SOFF = 1;  // increment
    DMA_TCD1_DOFF = 1;  // increment
    DMA_TCD1_CSR |= (1<<6) | (1<<3);  // active and clear SERQ
    DMA_TCD0_CSR |= (1<<6) | (1<<3);   // start transmit
	DMA_SERQ =0;  // enable channel
	DMA_SERQ =1;  // enable channel
	while (!(DMA_TCD0_CSR & DMA_TCD_CSR_DONE)) /* wait */ ;
	digitalWrite(CSpin,HIGH);
}

void spidma_write(char *outbuf, int bytes) {
	static int sink;
	digitalWrite(CSpin,LOW);
    DMA_TCD0_SADDR = outbuf;
    DMA_TCD1_DADDR = &sink;    // ignore bytes from SPI
    DMA_TCD0_NBYTES_MLNO = bytes;
    DMA_TCD1_NBYTES_MLNO = bytes;
    DMA_TCD0_SOFF = 1;  // increment
    DMA_TCD1_DOFF = 0;  // increment
    DMA_TCD1_CSR |=  DMA_TCD_CSR_START | (1<<3);  // start and clear SERQ
    DMA_TCD0_CSR |=  DMA_TCD_CSR_START | (1<<3);   // start transmit
	DMA_SERQ =0;  // enable channel
	DMA_SERQ =1;  // enable channel
	while (!(DMA_TCD1_CSR & DMA_TCD_CSR_DONE)) /* wait */ ;
	digitalWrite(CSpin,HIGH);
}

void spidma_read(char *inbuf, int bytes) {
	static int whatever = 0xffffffff;
	digitalWrite(CSpin,LOW);
    DMA_TCD0_SADDR = &whatever;
    DMA_TCD1_DADDR = inbuf;
    DMA_TCD0_NBYTES_MLNO = bytes;
    DMA_TCD1_NBYTES_MLNO = bytes;
    DMA_TCD0_SOFF = 0;  // increment
    DMA_TCD1_DOFF = 1;  // increment
    DMA_TCD1_CSR |=  DMA_TCD_CSR_START | (1<<3);  // start and clear SERQ
    DMA_TCD0_CSR |=  DMA_TCD_CSR_START | (1<<3);   // start transmit
	DMA_SERQ =0;  // enable channel
	DMA_SERQ =1;  // enable channel
	while (!(DMA_TCD1_CSR & DMA_TCD_CSR_DONE)) /* wait */ ;
	digitalWrite(CSpin,HIGH);
}


void setup() {
	Serial.begin(9600);
	while(!Serial.available()){
		Serial.println("hit a key");
		delay(1000);
	}
	Serial.read();
	Serial.println("ok");
	myspi_init();
	Serial.println(SPI0_CTAR0,HEX);
	Serial.println(SPI0_RSER,HEX);
		delay(2000);
}

void loop() {
	char buff[1000];
	unsigned int i,t1,t2;

	Serial.println("looping");
	for(i=0;i<sizeof(buff);i++) buff[i]=i;
	t1 = micros();
	spidma_write(buff,sizeof(buff));
	t2 = micros() - t1;
	Serial.println(t2);
	Serial.println(SPI0_CTAR0,HEX);
	Serial.println(SPI0_RSER,HEX);
	delay(2000);
}


