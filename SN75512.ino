#ifdef SN75512   
// 2 x SN75512

#define VFDrefresh 1200    //msec, Multiplex time period. Greater value => slower multiplex frequency

#define DATABITS 24        //total length of the shift registers

//Fill this table with the bit positions of the shift registers!   
byte segmentEnablePins[] =  {16,17,18,19,20,21,22,23};  //segment enable bits (a,b,c,d,e,f,g,DP)  (You MUST define always 8 bits!!!)
byte digitEnablePins[] = {0,1,2,3,4,5,6,7,8};           //digit enable bits   (You may define any number of digits between 1 and 10 )

//SN75512 control pins
#define PIN_LE      13  // D7 Shift Register Latch Enable
#define PIN_CLK     12  // D6 Shift Register Clock
#define PIN_DATA    14  // D5 Shift Register Data
#define PIN_STROBE  5   // D1 Shift Register Strobe (1=display off     0=display on)

#define PIN_HEAT_A -1   //VFD heater signalA  (if not used, set to -1)
#define PIN_HEAT_B -1   //VFD heater signalB  (if not used, set to -1)

#define PIN_LE_BIT     1<<PIN_LE    
#define PIN_CLK_BIT    1<<PIN_CLK    
#define PIN_DATA_BIT   1<<PIN_DATA  
#define PIN_STROBE_BIT 1<<PIN_STROBE    
#define PIN_HEAT_A_BIT 1<<PIN_HEAT_A
#define PIN_HEAT_B_BIT 1<<PIN_HEAT_B

//------------------abcdefgDP----------------   definition of different characters
byte charDefinition[] = {
                   B11111100,   //0: abcdef
                   B01100000,   //1: bc 
                   B11011010,   //2: abdeg
                   B11110010,   //3: abcdg
                   B01100110,   //4: bcfg
                   B10110110,   //5: acdfg
                   B10111110,   //6: acdefg
                   B11100000,   //7: abc
                   B11111110,   //8: abcdefg
                   B11110110,   //9: abcdfg
                   B00000000,   // : BLANK   (10)
                   B00000010,   //-: minus (11)
                   B00000001,   // decimal point (12)
                   B11101110,   // A  abcefg  (13)
                   B11001110,   // P  abefg (14)
                   B10011100,   // C  adef (15)
                   B11000110    //grad  abfg  (16)
};

#define MAXCHARS sizeof(charDefinition)
#define MAXSEGMENTS sizeof(segmentEnablePins)
int maxDigits =  sizeof(digitEnablePins);

uint32_t charTable[MAXCHARS];              //generated pin table from segmentDefinitions
uint32_t segmentEnableBits[MAXSEGMENTS];   //bitmaps, generated from EnablePins tables
uint32_t digitEnableBits[10];
boolean useHeater = false;                 //Is heater driver signal used?
//-----------------------------------------------------------------------------------------

void ICACHE_RAM_ATTR writeDisplay(){        
static volatile uint32_t val = 0;
static volatile byte pos = 0;
static volatile int brightCounter[] = {0,9,2,8,4,7,6,5,3,1};
static volatile boolean heatState = false;
//https://sub.nanona.fi/esp8266/timing-and-ticks.html
//One tick is 1us / 80 = 0.0125us = 12.5ns on 80MHz
//One tick is 1us / 80 = 0.0125us = 6.25ns on 160MHz
// 1 NOP is 1 tick

  //noInterrupts();
  
  if (useHeater) {
    heatState = !heatState;
    if (heatState) {
      WRITE_PERI_REG( PIN_OUT_CLEAR, PIN_HEAT_A_BIT );
      WRITE_PERI_REG( PIN_OUT_SET,   PIN_HEAT_B_BIT );
    }
    else {
      WRITE_PERI_REG( PIN_OUT_SET,   PIN_HEAT_A_BIT );
      WRITE_PERI_REG( PIN_OUT_CLEAR, PIN_HEAT_B_BIT );
    }
  }
  
  if (EEPROMsaving) {  //stop refresh, while EEPROM write is in progress!
    timer1_write(VFDrefresh);
    return;  
  }

  if (brightCounter[pos] % MAXBRIGHTNESS < (displayON ?  prm.dayBright : prm.nightBright))
    WRITE_PERI_REG( PIN_OUT_CLEAR, PIN_STROBE_BIT );   //ON
  else 
    WRITE_PERI_REG( PIN_OUT_SET, PIN_STROBE_BIT );    //OFF
    
  for (int t=0; t<4;t++) asm volatile ("nop");
  
  brightCounter[pos]++;  
  if (brightCounter[pos]>MAXBRIGHTNESS) brightCounter[pos] = 1;
  
  val = (digitEnableBits[pos] | charTable[digit[pos]]);  //the full bitmap to send to MAX chip
  if (digitDP[pos]) val = val | charTable[12];    //Decimal Point
  for (int i=0; i<DATABITS; i++)  {
    if ((val & uint32_t(1 << (DATABITS -1 - i))) ) {
      WRITE_PERI_REG( PIN_OUT_SET, PIN_DATA_BIT );
          for (int t=0; t<4;t++) asm volatile ("nop");   
    }
    else {
      WRITE_PERI_REG( PIN_OUT_CLEAR, PIN_DATA_BIT );
          for (int t=0; t<8;t++) asm volatile ("nop");   
    }
    WRITE_PERI_REG( PIN_OUT_SET, PIN_CLK_BIT );
    for (int t=0; t<8;t++) asm volatile ("nop");  
    WRITE_PERI_REG( PIN_OUT_CLEAR, PIN_CLK_BIT );
    for (int t=0; t<4;t++) asm volatile ("nop");   
    } //end for      
 
  WRITE_PERI_REG( PIN_OUT_SET, PIN_LE_BIT );
  for (int t=0; t<4;t++) asm volatile ("nop");   
  WRITE_PERI_REG( PIN_OUT_CLEAR, PIN_LE_BIT );

  pos++; if (pos >= maxDigits) pos = 0; 

  timer1_write(VFDrefresh);
  //interrupts();
}

void generateBitTable() {
uint32_t out;

  DPRINTLN("--- Generating segment pins bitmap ---");
  for (int i=0;i<MAXSEGMENTS;i++) {
    segmentEnableBits[i] = uint32_t(1<<segmentEnablePins[i]);
    //DPRINT(i); DPRINT(": "); DPRINTLN(segmentEnableBits[i],BIN);
  }
  
  DPRINTLN("--- Generating digit pins bitmap ---");
  for (int i=0;i<maxDigits;i++) {
    digitEnableBits[i] = uint32_t(1 << digitEnablePins[i]);
    //DPRINT(i); DPRINT(": "); DPRINTLN( digitEnableBits[i],BIN);
  }

DPRINTLN("---- Generated Character / Pins table -----");
  for (int i=0;i<MAXCHARS;i++) {
    out = 0;
    //DPRINT(i); DPRINT(":  ");
    //DPRINT(charDefinition[i],BIN);  //DPRINT(" = ");
    for (int j=0;j<=7;j++)   //from a to g
      if ((charDefinition[i] & 1<<(7-j)) != 0) {
        out = out | segmentEnableBits[j]; //DPRINT("1"); 
        }
    //else        DPRINT("0");
    //DPRINT("  >> ");  
    
    charTable[i] = out;
    //DPRINTLN(charTable[i],BIN);
  }  //end for
}


void setup_pins() {
#if defined(ESP8266) 
#else
  #error "Board is not supported!"  
#endif
  
  DPRINTLN("Setup pins...");
  pinMode(PIN_LE,  OUTPUT);
  pinMode(PIN_STROBE,  OUTPUT); // a priori inutile avec le PWM
  pinMode(PIN_DATA,OUTPUT);
  pinMode(PIN_CLK, OUTPUT);
  digitalWrite(PIN_STROBE,LOW);  //brightness
  if ((PIN_HEAT_A >=0) && (PIN_HEAT_B>=0)) {
    useHeater = true;
    pinMode(PIN_HEAT_A, OUTPUT);
    digitalWrite(PIN_HEAT_A,HIGH);
    pinMode(PIN_HEAT_B, OUTPUT);
    digitalWrite(PIN_HEAT_B,LOW);
  }
  
  generateBitTable();
  digitsOnly = false;
    
  timer1_attachInterrupt(writeDisplay);
  timer1_enable(TIM_DIV16, TIM_EDGE, TIM_SINGLE);
  timer1_write(VFDrefresh); 
}  

void writeDisplaySingle() {}
#endif
