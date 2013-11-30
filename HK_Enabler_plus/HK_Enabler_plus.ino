// JCDesigns HK Enabler Arduino Version 1.0
// Uses Sparkfun Arduino Pro Micro (Or equivalent)https://www.sparkfun.com/products/11098
// Uses TH3122.4 chip Sourced from http://www.ebay.com/itm/2x-Melexis-TH3122-4-IBUS-I-Bus-K-Bus-Transceiver-/221296386461?pt=LH_DefaultDomain_0&hash=item33864ae19d
// Uses PC Board Sourced From OSH Park (3 boards for $5 sq in)
// connections for 10DOF(axis) IMU L3G4200D+ADXL345+HMC5883L+BMP085 (3V-5V compatible)sensor module http://www.ebay.com/itm/141096879704?ssPageName=STRK:MEWNX:IT&_trksid=p3984.m1497.l2649
// IMU Also here https://core-electronics.com.au/store/index.php/10dof-imu-module-with-l3g4200d-adxl345-hmc5883l-bmp085-gy-80.html 
// Holes For attching https://www.sparkfun.com/products/9394
// Send Patterns from the I-Bus/K-Bus for Harman Kardon Radio Emulation 
// based on https://github.com/blalor/iPod_IBus_adapter
// Idea From http://www.northamericanmotoring.com/forums/electrical/155161-getting-more-out-of-the-r53-mfsw.html
// With  help from http://www.gbmini.net/wp/2004/04/running_harmon_kardon_with_no_factory_head_unit/



// SEN/STA of TH3122 connected to digital pin 2 (interrupt 1)
// if SEN/STA is low, there is traffic on the bus
// forcing SEN/STA low clears line for transmission
// buttons to change sound mode can be added at digital pins
// if not interfacing steering wheel buttons to radio, they can be mapped to digital pins to control things

#include <EEPROM.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <SoftwareSerial.h>
#include <Wire.h>


///////////////////////////////////////////////////////////////////////////
// Pin/Button connections...
//PCB has connections for 4 buttons at D2, D4, D5, and D6
#define DSP_mode    14
#define SENSTA      4 //if SEN/STA is jumpered to Ground, D4 is open  
#define Volume_plus 15
#define Fader_front 16
#define Fader_rear  10

/*
 * LEDs:
 *   red: missed poll from radio
 *   yellow: processing incoming IBus data
 *   green: sending IBus data
 *   
 *   yellow + green: contention/collision when sending
 */
#define LED_ERR      5 // red
#define LED_IBUS_RX  6 // yellow
#define LED_IBUS_TX  7 // green

//i2C connections for IMU
#define i2C_SDA 2 
#define i2C_SCL 3

SoftwareSerial mySerial(9, 8); // RX, TX for LCD screen


/////////////////////////////////////////////////////////////

#define MAX_EXPECTED_LEN 64

#define TX_BUF_LEN 80
#define RX_BUF_LEN (MAX_EXPECTED_LEN + 2)

// addresses of IBus devices
#define RAD_ADDR  0x68
#define DSP_ADDR  0x6A
#define SW_ADDR   0x50

// static offsets into the packet
#define PKT_SRC  0
#define PKT_LEN  1
#define PKT_DEST 2
#define PKT_CMD  3

boolean Radio_turned_on; 
boolean Radio_ready;
int packet_delay = 30;

// buffer for building outgoing packets
uint8_t tx_buf[TX_BUF_LEN];

// buffer for processing incoming packets; same size as serial buffer
uint8_t rx_buf[RX_BUF_LEN];

/*
 * time-keeping variables
 */

// trigger time to turn off LED
unsigned long ledOffTime; // 500ms interval

// timeout duration before giving up on a read
unsigned long readTimeout; // variable


void setup() { 
  pinMode (SENSTA, INPUT);
  pinMode (DSP_mode,INPUT_PULLUP);
  pinMode (Fader_front, INPUT_PULLUP);
  pinMode (Fader_rear, INPUT_PULLUP);
  pinMode (Volume_plus, INPUT_PULLUP);  
  
  Radio_turned_on = false; 
  Radio_ready = false;
  
  
  // set up serial for IBus; 9600,8,E,1 I set these up directly as they didn't seem to work right using Arduino call out
    Serial1.begin(9600);
    UCSR1C |= (1 << UPM11);
    UCSR1C &= ~(1 << UPM10);
    UCSR1C |= (0 << USBS1);
    
  //start recieving Serial Data 
    Serial.begin(9600);
   
  //LCD Serial
   mySerial.begin(9600); 
   
   
} 

void loop() {  
      
    HK_Enabler();

}  




/* **********************REFERENCE*************************************

  //HK Radio Definitions 
Key_on[] = {0x80,0x04,0xBF,0x11,0x01,0x2B}; 
Key_off[] = {0x80,0x04,0xBF,0x11,0x00,0x2A}; 
Radio_on[] = {0x68,0x04,0xFF,0x02,0x04,0x95}; 
DSP_present[] = {0x6A,0x04,0x68,0x02,0x01,0x05}; 
Initialize_begin[] = {0x68,0x04,0x6A,0x36,0xAF,0x9F}; 
Initialize_end[] = {0x68,0x04,0x6A,0x36,0xA1,0x91}; 
Bass_0[] = {0x68,0x04,0x6A,0x36,0x60,0x50}; 
Treble_0[] = {0x68,0x04,0x6A,0x36,0xC0,0xF0}; 
Fade_rear4[] = {0x68,0x04,0x6A,0x36,0x94,0xA4}; 
Balance_center[] = {0x68,0x04,0x6A,0x36,0x40,0x70}; 
 Radio_spatial[] = {0x68,0x04,0x6A,0x36,0xE4,0xD4};
Radio_miniHK[] = {0x68,0x04,0x6A,0x36,0xE3,0xD3};
Radio_electronic[] = {0x68,0x04,0x6A,0x36,0xE5,0xD5};
Radio_instrumental[] = {0x68,0x04,0x6A,0x36,0xE6,0xD6};
Radio_festival[] = {0x68,0x04,0x6A,0x36,0xE7,0xD7};
Mode_spatial[] = {0x68,0x04,0x6A,0x34,0x09,0x3B}; 
Mode_miniHK[] = {0x68,0x04,0x6A,0x34,0x08,0x3A}; 
Mode_electronic[] = {0x68,0x04,0x6A,0x34,0x0A,0x3C}; 
Mode_instrumental[] = {0x68,0x04,0x6A,0x34,0x0B,0x3D}; 
Mode_festival[] = {0x68,0x04,0x6A,0x34,0x0C,0x3E}; 
Driver_mode_off[] = {0x68,0x05,0x6A,0x34,0x90,0x00,0xA3}; 
Driver_mode_on[] = {0x68,0x05,0x6A,0x34,0x91,0x00,0xA3}; 
Volume_increase15[] = {0x68,0x04,0x6A,0x32,0xF1,0xC5}; 
Volume_increase3[] = {0x68,0x04,0x6A,0x32,0x31,0x05};
Volume_increase6[] = {0x68,0x04,0x6A,0x32,0x61,0x55};
Volume_increase1[] = {0x68,0x04,0x6A,0x32,0x01,0x55}; // check this

//Steering Wheel Controls
SW_Volume_up_press[] = {0x50,0x04,0x68,0x32,0x11,0x1F}; //Volume up = top right button 
SW_Volume_up_release[] = {0x50,0x04,0x68,0x32,0x31,0x3F}; //<+> release
SW_Volume_down_press[] = {0x50,0x04,0x68,0x32,0x10,0x1E}; // Volume down = bottom right button
SW_Volume_down_release[] = {0x50,0x04,0x68,0x32,0x30,0x3E}; // <-> release
SW_Mode_press[] = {0x50,0x04,0x68,0x3B,0x02,0x05}; // Mode press = middle right button 
SW_Mode_release[] = {0x50,0x04,0x68,0x3B,0x22,0x25}; // Mode release 

SW_Next_press[] = {0x50,0x04,0x68,0x3B,0x01,0x06}; // Next track press = top left button 
SW_Next_release[] = {0x50,0x04,0x68,0x3B,0x21,0x26}; // Next track release 
SW_Previous_press[] = {0x50,0x04,0x68,0x3B,0x08,0x0F}; // Previous track press = bottom left button 
SW_Previous_release[] = {0x50,0x04,0x68,0x3B,0x28,0x2F}; // Previous track release 
SW_LM_press[] = {0x50,0x04,0x68,0x3B,0x80,0x87}; // Left middle button press 
SW_LM_release[] = {0x50,0x04,0x68,0x3B,0xA0,0xA7}; // Left middle button release

 With Help From http://www.northamericanmotoring.com/forums/electrical/155161-getting-more-out-of-the-r53-mfsw.html
 With  help from http://www.gbmini.net/wp/2004/04/running_harmon_kardon_with_no_factory_head_unit/
 
 TECHNICAL INFO:
 Harmon Kardon / Radio communications in 2003 MINI Cooper S.
 ===============================================
 When power is applied, the radio sends a broadcast message â€“ presumably indicates that it is turned on.
 68 04 FF 02 04 95 Radio->all …
 Next the DSP sends a message to the radio â€“ presumably indicating that it is present.
 6A 04 68 02 01 05 DSP->Radio …
 
 So now the radio knows it needs to operate in “Harmon Kardon” mode. In this mode the front speaker outputs are fixed at a medium volume level, with no bass/treble/fade/balance applied. Instead the audio control settings are transmitted to the amplifier and are applied to the incoming signal there. The radio sends an initial sequence of settings:
 68 04 6A 36 AF 9F Radio->DSP “initialize begin?”
 68 04 6A 36 60 50 Radio->DSP “bass=center”
 68 04 6A 36 C0 F0 Radio->DSP “treble=center”
 68 04 6A 36 80 B0 Radio->DSP “fade=center”
 68 04 6A 36 40 70 Radio->DSP “balance=center”
 68 04 6A 36 E4 D4 Radio->DSP ????
 68 04 6A 34 09 3B Radio->DSP “mode=spatial”
 68 05 6A 34 90 00 A3 Radio->DSP “driver mode off”
 68 04 6A 36 A1 91 Radio->DSP “initialize end?”
 The values for bass, treble, fade and balance are the current settings â€“ not necessarily “center”. The “36 AF” message at the beginning perhaps sets some flags; this message stops the amplifier putting out any sound, because the radio sends it when it is turned off. It probably also sets all the audio controls to default, and sets the volume to zero. The “36 A1″ at the end presumably allows the amplifier to start running.
 
 The radio now sends a sequence to set the volume. This is set with one or more messages that increase the volume by between 1 and 15 steps:
 68 04 6A 32 F1 C5 Radio->DSP “volume+15″
 68 04 6A 32 F1 C5 Radio->DSP “volume+15″
 68 04 6A 32 31 05 Radio->DSP “volume+3″
 So the volume is now at “33″; the maximum volume that can be sent by the radio is “63″.
 
 The radio / amplifier system is now operating.
 
 The volume can be adjusted up/down. The radio sends messages to increase or decrease the volume by 1-15 steps; it must keep track of how many steps are needed:
 68 04 6A 32 10 24 Radio->DSP “volume-1″
 68 04 6A 32 40 74 Radio->DSP “volume-4″
 68 04 6A 32 10 24 Radio->DSP “volume-1″
 68 04 6A 32 30 04 Radio->DSP “volume-3″
 68 04 6A 32 11 25 Radio->DSP “volume+1″
 68 04 6A 32 61 55 Radio->DSP “volume+6″
 68 04 6A 32 11 25 Radio->DSP “volume+1″
 68 04 6A 32 51 65 Radio->DSP “volume+5″
 After this sequence, the volume would be at “36″.
 
 The audio controls can also be adjusted; bass/treble/fade/balance are all adjusted using the “36 xy” code where the “x” part indicates the audio function and the “y” part indicates the setting:
 36 60 “bass=center”
 36 6y “bass=boost”; y=2/4/6/8/A/C
 36 7y “bass=cut”; y=2/4/6/8/A/C
 36 C0 “treble=center”
 36 Cy “treble=boost”; y=2/4/6/8/A/C
 36 Dy “treble=cut”; y=2/4/6/8/A/C
 36 80 “fade=center”
 36 8y “fade=front?”; y=1/2/3/4/5/6/8/A/F
 36 9y “fade=rear?”; y=1/2/3/4/5/6/8/A/F
 36 40 “balance=center”
 36 4y “balance=left?”; y=1/2/3/4/5/6/8/A/F
 36 5y “balance=right?”; y=1/2/3/4/5/6/8/A/F
 I would guess that all the audio controls can vary to “F”=15, so that it might be possible to get more bass/treble boost by sending a “36 6F” or “36 CF” code.
 
 The amplifier mode can also be adjusted using the “34 0y” code where the “y” part selects the mode:
 34 08 “MINI H/K”
 34 09 “SPATIAL”
 34 0A “ELECTRONIC”
 34 0B “INSTRUMENTAL”
 34 0C “FESTIVAL”
 Other values of “y” seem to have no effect.
 
 The “driver mode” can also be adjusted on or off:
 34 90 “DRIVER MODE OFF”
 34 91 “DRIVER MODE ON”
 Some modes do not allow the driver mode to be turned on; the radio always sends a “34 90″ code when the amplifier mode is changed.
 Turning driver mode on when it’s not allowed seems to have no effect.
 
 If the radio is turned off, a “final” message is sent to the amplifier:
 68 04 6A 36 AF 9F Radio->DSP “initialize begin?”
 This silences the amplifier even if it is not powered off! Strange, since the radio also sends a voltage to control the amplifier.

 ---------------------------------------------------------------------------------
 Steering Wheel ------------------------------------------------------------------    
 ---------------------------------------------------------------------------------

MINI with the 2 spoke steering wheel 

50 04 68 32 11 1F Volume up = top right button 
50 04 68 32 31 3F <+> release
50 04 68 32 10 1E Volume down = bottom right button
50 04 68 32 30 3E <-> release

50 04 68 3B 01 06 Next track press = top left button 
50 04 68 3B 21 26 Next track release 

50 04 68 3B 08 0F Previous track press = bottom left button 
50 04 68 3B 28 2F Previous track release 
 
50 04 68 3B 02 05 Mode press = middle right button 
50 04 68 3B 22 25 Mode release 

50 04 68 3B 80 87 Left middle button press 
50 04 68 3B A0 A7 Left middle button release

 */

