//--------------------------------------------------------------------------------------
// Internal temperature measurement of Attiny84 based TinySensor
// harizanov.com
// GNU GPL V3
//--------------------------------------------------------------------------------------

#ifdef F_CPU  
#undef F_CPU
#endif  
#define F_CPU 1000000 // 1 MHz  

#include <avr/sleep.h>

#ifndef cbi
#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#endif
#ifndef sbi
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))
#endif

#include <JeeLib.h> // https://github.com/jcw/jeelib
#include "pins_arduino.h"

ISR(WDT_vect) { Sleepy::watchdogEvent(); } // interrupt handler for JeeLabs Sleepy power saving

#define myNodeID 8      // RF12 node ID in the range 1-30
#define network 210      // RF12 Network group
#define freq RF12_868MHZ // Frequency of RFM12B module

#define LEDpin PB0


//########################################################################################################################
//Data Structure to be sent
//########################################################################################################################

 typedef struct {
  	  int state;	// State
  	  int supplyV;	// Supply voltage
 } Payload;

static Payload temptx;

static void setPrescaler (uint8_t mode) {
    cli();
    CLKPR = bit(CLKPCE);
    CLKPR = mode;
    sei();
}
 
 ISR(PCINT0_vect) {
  temptx.state=bitRead(PINA,1);
 }

void setup() {

  setPrescaler(1);    // div 1, i.e. 4 MHz
  pinMode(LEDpin,OUTPUT);
  digitalWrite(LEDpin,LOW);
  delay(1000);
  digitalWrite(LEDpin,HIGH);

 
  pinMode(1,INPUT);
  sbi(GIMSK,PCIE0); // Turn on Pin Change interrupt
  sbi(PCMSK0,PCINT1); // Which pins are affected by the interrupt
  
  rf12_initialize(myNodeID,freq,network); // Initialize RFM12 with settings defined above 
  // Adjust low battery voltage to 2.2V
  rf12_control(0xC040);
  rf12_sleep(0);                          // Put the RFM12 to sleep
//  Serial.begin(9600);
  PRR = bit(PRTIM1); // only keep timer 0 going
}

void loop() {
  pinMode(LEDpin,OUTPUT);
  digitalWrite(LEDpin,LOW);  
  setPrescaler(3);  // div 8, i.e. 1 MHz
  bitClear(PRR, PRADC); // power up the ADC
  ADCSRA |= bit(ADEN); // enable the ADC  
  Sleepy::loseSomeTime(16); // Allow 10ms for the sensor to be ready

//  temptx.state=1;
  temptx.supplyV = readVcc(); // Get supply voltage

  ADCSRA &= ~ bit(ADEN); // disable the ADC
  bitSet(PRR, PRADC); // power down the ADC

  digitalWrite(LEDpin,HIGH);  

  rfwrite(); // Send data via RF 

  /*
  for(int j = 0; j < 1; j++) {    // Sleep for 5 minutes
    Sleepy::loseSomeTime(60000); //JeeLabs power save function: enter low power mode for 60 seconds (valid range 16-65000 ms)
  }
  */
  

  set_sleep_mode(SLEEP_MODE_PWR_DOWN); // Set sleep mode
  sleep_mode(); // System sleeps here
  
}

//--------------------------------------------------------------------------------------------------
// Send payload data via RF
//--------------------------------------------------------------------------------------------------
static void rfwrite(){
  setPrescaler(1);    // div 1, i.e. 4 MHz
   bitClear(PRR, PRUSI); // enable USI h/w
   rf12_sleep(-1);     //wake up RF module
   while (!rf12_canSend())
     rf12_recvDone();
   rf12_sendStart(0, &temptx, sizeof temptx); 
   rf12_sendWait(2);    //wait for RF to finish sending while in standby mode
   rf12_sleep(0);    //put RF module to sleep
   bitSet(PRR, PRUSI); // disable USI h/w
   setPrescaler(4); // div 4, i.e. 1 MHz
}
//--------------------------------------------------------------------------------------------------
// Read current supply voltage
//--------------------------------------------------------------------------------------------------

 long readVcc() {
   long result;
   // Read 1.1V reference against Vcc
   ADMUX = _BV(MUX5) | _BV(MUX0);
   delay(2); // Wait for Vref to settle
   ADCSRA |= _BV(ADSC); // Convert
   while (bit_is_set(ADCSRA,ADSC));
   result = ADCL;
   result |= ADCH<<8;
   result = 1126400L / result; // Back-calculate Vcc in mV
   return result;
}




