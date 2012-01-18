/* Interacts with a RFXCOM pulse or Photosensor module.
Will write a file to the SD card if sensor readings spike (or, fall in this case)

RTC module code based upon http://www.combustory.com/wiki/index.php/RTC1307_-_Real_Time_Clock
*/

#include "Wire.h"
#include "Statistic.h"
#include <SD.h>
#define RTC_I2C_ADDRESS 0x68 // RTC I2C address

// Number of measurements for averaging
const int AVERAGE_LENGTH = 100;

const int SAMPLE_TIME = 50;
const int DELAY_TIME = 25;
 
int photocellPin = 7;     // the LDR and cap are connected to pin2
int photocellReading;     // the digital reading
int ledPin = 13;    // you can just use the 'built in' LED
int pinup_delay = 0;

// for the circular buffer
int lastReadings[AVERAGE_LENGTH];
float average = 0;
float stddev = 0;
int counter = 0;

// For reading time
byte second, minute, hour, dayOfWeek, dayOfMonth, month, year;

//File pointer for loggin
File file;
char name[16];
char contents[64];

//stats
Statistic stats;
 
void setup(void) {
  // We'll send debugging information via the Serial monitor
  Wire.begin();
  Serial.begin(9600);
  Serial.println("[init] Starting sensor");
  
  // For clearing the capacitor
  pinMode(2, OUTPUT);
  pinMode(3, OUTPUT);  
  pinMode(13, OUTPUT);
  
  Serial.println("[init] Starting stat engine");
  
  //stats stuff
  stats.clear();

  Serial.print("[init] Taking initial measurements..."); 
  // Initialise the array 
  for(int i=0;i<AVERAGE_LENGTH;i++) {
    stats.add(RCtime(photocellPin));
  }
  Serial.println("done");  
 
  // SD card stuff
  Serial.println("[init] Starting SD card");
  delay(2000);
  pinMode(10, OUTPUT);
  int suc = SD.begin(8);
  if (!suc){
    Serial.println("[init] SD FAILED");
    Serial.println(suc);
    while(1);
  } else {
    Serial.println("[init] SD success"); 
  }
  
  
}
 
void loop(void) {
  // read the resistor using the RCtime technique
  photocellReading = RCtime(photocellPin);
  
  // Calculate rolling average
  ++counter %= AVERAGE_LENGTH;

  stddev = stats.pop_stdev();
  average = stats.average();
  float ubound = average + (stddev*1.96);
  float lbound = average - (stddev*1.96);

  // Glorious debug
  
  //Serial.print("lb = "); 
  //Serial.print(lbound);
  //Serial.print(" Av = "); 
  //Serial.print(average);
  //Serial.print(" ub = "); 
  //Serial.print(ubound);
  //Serial.print(" std = ");
  //Serial.print(stddev);
  //Serial.print(" R: ");
  // Serial.println(photocellReading); 
  
  if ((photocellReading < lbound) && pinup_delay == 0) {
    getDate();
    Serial.println("[SENSOR] Start of zero - or flash");
    Serial.print("[SENSOR] Reading = ");
    Serial.println(photocellReading);     // the raw analog reading
    setNameAndContent();
    writeLog();
    digitalWrite(13, HIGH);
    pinup_delay = DELAY_TIME;
  } else {

    //Serial.println();
    digitalWrite(13, LOW);
    if (pinup_delay > 0) {
      pinup_delay--;
    } 
    if (pinup_delay == 1){
      Serial.println("[SENSOR] End of zero - or flash");
    }
    if (pinup_delay == 0) {
      // if it not a reading, add it
      stats.add(photocellReading);
    }
  }
 
  delay(SAMPLE_TIME);
}

//// Calculates the new mean based on the last 20 measurements 
//int calcStats() {
//  
//  // average
//  float oav = average;
//  average = 0;
//  stddev = 0;
//  for(int i=0;i<AVERAGE_LENGTH;i++) {
//    average += lastReadings[i];
//    stddev += sq(lastReadings[i] - oav);
//  }
//  average /= AVERAGE_LENGTH;
//  stddev = sqrt((stddev/oav));
//}
 
// Uses a digital pin to measure a resistor (like an FSR or photocell!)
// We do this by having the resistor feed current into a capacitor and
// counting how long it takes to get to Vcc/2 (for most arduinos, thats 2.5V)
int RCtime(int RCpin) {
  int reading = 0;  // start with 0
 
  // set the pin to an output and pull to LOW (ground)
  //pinMode(RCpin, OUTPUT);
  //digitalWrite(RCpin, LOW);
  
  // Reset the cap
  digitalWrite(2, LOW);
  digitalWrite(2, LOW);
  
  delay(1);
 
  // Now set the pin to an input and...
  //pinMode(RCpin, INPUT);
  
  // Count how long to rise
  pinMode(3, INPUT);
  digitalWrite(2, HIGH);
  
  while (digitalRead(7) == HIGH) { 
    reading++;      // increment to keep track of time 
 
    if (reading == 30000) {
      // if we got this far, the resistance is so high
      // its likely that nothing is connected! 
      break;           // leave the loop
    }
  }
  pinMode(3, OUTPUT);
  // OK either we maxed out at 30000 or hopefully got a reading, return the count
 
  return reading;
}

// convert normal decimal numbers into binary coded decimal
byte decToBcd(byte val) {
  return ( (val/10*16) + (val%10) );
}

// convert binary coded decimal into normal decimal
byte bcdtoDec(byte val) {
  return ( (val/16*10) + (val%16) );
}

// Gets the current datetime from the RTC
void getDate() {
  
  // Reset the RTC register pointer
  Wire.beginTransmission(RTC_I2C_ADDRESS);
  Wire.write((byte)0x00);
  Wire.endTransmission();
  
  // begin read
  Wire.requestFrom(RTC_I2C_ADDRESS, 7);
  
  // A few of these need masks because certain bits are control bits
  second     = bcdtoDec(Wire.read() & 0x7f);
  minute     = bcdtoDec(Wire.read());
  hour       = bcdtoDec(Wire.
  read() & 0x3f);
  dayOfWeek  = bcdtoDec(Wire.read());
  dayOfMonth = bcdtoDec(Wire.read());
  month      = bcdtoDec(Wire.read());
  year       = bcdtoDec(Wire.read());
  
}

//// This outputs the date with no newline
//void outputDate() {
//  Serial.print(hour, DEC);
//  Serial.print(":");
//  Serial.print(minute, DEC);
//  Serial.print(":");
//  Serial.print(second, DEC);
//  Serial.print("  ");
//  Serial.print(month, DEC);
//  Serial.print("/");
//  Serial.print(dayOfMonth, DEC);
//  Serial.print("/");
//  Serial.print(year, DEC);
//}

void setNameAndContent() {
  sprintf(name, "%iI%iI%i.TXT\0", dayOfMonth, month, year);
  sprintf(contents, "%i:%i:%i %i/%i/%i\0", hour, minute, second, dayOfMonth, month, year);
}

void writeLog() {
  file = SD.open(name, O_CREAT | O_APPEND | O_WRITE);
  Serial.println(name);
  Serial.println(file);
  file.println(contents);
  file.close();
}
