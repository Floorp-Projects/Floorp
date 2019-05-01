/*
 * smslib.m
 *
 * SMSLib Sudden Motion Sensor Access Library
 * Copyright (c) 2010 Suitable Systems
 * All rights reserved.
 *
 * Developed by: Daniel Griscom
 *               Suitable Systems
 *               http://www.suitable.com
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal with the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * - Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimers.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimers in the
 * documentation and/or other materials provided with the distribution.
 *
 * - Neither the names of Suitable Systems nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this Software without specific prior written permission.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS WITH THE SOFTWARE.
 *
 * For more information about SMSLib, see
 *		<http://www.suitable.com/tools/smslib.html>
 * or contact
 *		Daniel Griscom
 *		Suitable Systems
 *		1 Centre Street, Suite 204
 *		Wakefield, MA 01880
 *		(781) 665-0053
 *
 */

#import <IOKit/IOKitLib.h>
#import <sys/sysctl.h>
#import <math.h>
#import "smslib.h"

#pragma mark Internal structures

// Represents a single axis of a type of sensor.
typedef struct axisStruct {
  int enabled;  // Non-zero if axis is valid in this sensor
  int index;    // Location in struct of first byte
  int size;     // Number of bytes
  float zerog;  // Value meaning "zero g"
  float oneg;   // Change in value meaning "increase of one g"
                // (can be negative if axis sensor reversed)
} axisStruct;

// Represents the configuration of a type of sensor.
typedef struct sensorSpec {
  const char* model;      // Prefix of model to be tested
  const char* name;       // Name of device to be read
  unsigned int function;  // Kernel function index
  int recordSize;         // Size of record to be sent/received
  axisStruct axes[3];     // Description of three axes (X, Y, Z)
} sensorSpec;

// Configuration of all known types of sensors. The configurations are
// tried in order until one succeeds in returning data.
// All default values are set here, but each axis' zerog and oneg values
// may be changed to saved (calibrated) values.
//
// These values came from SeisMaCalibrate calibration reports. In general I've
// found the following:
//	- All Intel-based SMSs have 250 counts per g, centered on 0, but the signs
//		are different (and in one case two axes are swapped)
//	- PowerBooks and iBooks all have sensors centered on 0, and reading
//		50-53 steps per gravity (but with differing polarities!)
//	- PowerBooks and iBooks of the same model all have the same axis polarities
//	- PowerBook and iBook access methods are model- and OS version-specific
//
// So, the sequence of tests is:
//	- Try model-specific access methods. Note that the test is for a match to the
//		beginning of the model name, e.g. the record with model name "MacBook"
//		matches computer models "MacBookPro1,2" and "MacBook1,1" (and ""
//		matches any model).
//	- If no model-specific record's access fails, then try each model-independent
//		access method in order, stopping when one works.
static const sensorSpec sensors[] = {
    // ****** Model-dependent methods ******
    // The PowerBook5,6 is one of the G4 models that seems to lose
    // SMS access until the next reboot.
    {"PowerBook5,6",
     "IOI2CMotionSensor",
     21,
     60,
     {{1, 0, 1, 0, 51.5}, {1, 1, 1, 0, -51.5}, {1, 2, 1, 0, -51.5}}},
    // The PowerBook5,7 is one of the G4 models that seems to lose
    // SMS access until the next reboot.
    {"PowerBook5,7",
     "IOI2CMotionSensor",
     21,
     60,
     {{1, 0, 1, 0, 51.5}, {1, 1, 1, 0, 51.5}, {1, 2, 1, 0, 51.5}}},
    // Access seems to be reliable on the PowerBook5,8
    {"PowerBook5,8",
     "PMUMotionSensor",
     21,
     60,
     {{1, 0, 1, 0, -51.5}, {1, 1, 1, 0, 51.5}, {1, 2, 1, 0, -51.5}}},
    // Access seems to be reliable on the PowerBook5,9
    {"PowerBook5,9",
     "PMUMotionSensor",
     21,
     60,
     {{1, 0, 1, 0, 51.5}, {1, 1, 1, 0, -51.5}, {1, 2, 1, 0, -51.5}}},
    // The PowerBook6,7 is one of the G4 models that seems to lose
    // SMS access until the next reboot.
    {"PowerBook6,7",
     "IOI2CMotionSensor",
     21,
     60,
     {{1, 0, 1, 0, 51.5}, {1, 1, 1, 0, 51.5}, {1, 2, 1, 0, 51.5}}},
    // The PowerBook6,8 is one of the G4 models that seems to lose
    // SMS access until the next reboot.
    {"PowerBook6,8",
     "IOI2CMotionSensor",
     21,
     60,
     {{1, 0, 1, 0, 51.5}, {1, 1, 1, 0, 51.5}, {1, 2, 1, 0, 51.5}}},
    // MacBook Pro Core 2 Duo 17". Note the reversed Y and Z axes.
    {"MacBookPro2,1",
     "SMCMotionSensor",
     5,
     40,
     {{1, 0, 2, 0, 251}, {1, 2, 2, 0, -251}, {1, 4, 2, 0, -251}}},
    // MacBook Pro Core 2 Duo 15" AND 17" with LED backlight, introduced June '07.
    // NOTE! The 17" machines have the signs of their X and Y axes reversed
    // from this calibration, but there's no clear way to discriminate between
    // the two machines.
    {"MacBookPro3,1",
     "SMCMotionSensor",
     5,
     40,
     {{1, 0, 2, 0, -251}, {1, 2, 2, 0, 251}, {1, 4, 2, 0, -251}}},
    // ... specs?
    {"MacBook5,2",
     "SMCMotionSensor",
     5,
     40,
     {{1, 0, 2, 0, -251}, {1, 2, 2, 0, 251}, {1, 4, 2, 0, -251}}},
    // ... specs?
    {"MacBookPro5,1",
     "SMCMotionSensor",
     5,
     40,
     {{1, 0, 2, 0, -251}, {1, 2, 2, 0, -251}, {1, 4, 2, 0, 251}}},
    // ... specs?
    {"MacBookPro5,2",
     "SMCMotionSensor",
     5,
     40,
     {{1, 0, 2, 0, -251}, {1, 2, 2, 0, -251}, {1, 4, 2, 0, 251}}},
    // This is speculative, based on a single user's report. Looks like the X and Y axes
    // are swapped. This is true for no other known Appple laptop.
    {"MacBookPro5,3",
     "SMCMotionSensor",
     5,
     40,
     {{1, 2, 2, 0, -251}, {1, 0, 2, 0, -251}, {1, 4, 2, 0, -251}}},
    // ... specs?
    {"MacBookPro5,4",
     "SMCMotionSensor",
     5,
     40,
     {{1, 0, 2, 0, -251}, {1, 2, 2, 0, -251}, {1, 4, 2, 0, 251}}},
    // ****** Model-independent methods ******
    // Seen once with PowerBook6,8 under system 10.3.9; I suspect
    // other G4-based 10.3.* systems might use this
    {"", "IOI2CMotionSensor", 24, 60, {{1, 0, 1, 0, 51.5}, {1, 1, 1, 0, 51.5}, {1, 2, 1, 0, 51.5}}},
    // PowerBook5,6 , PowerBook5,7 , PowerBook6,7 , PowerBook6,8
    // under OS X 10.4.*
    {"", "IOI2CMotionSensor", 21, 60, {{1, 0, 1, 0, 51.5}, {1, 1, 1, 0, 51.5}, {1, 2, 1, 0, 51.5}}},
    // PowerBook5,8 , PowerBook5,9 under OS X 10.4.*
    {"",
     "PMUMotionSensor",
     21,
     60,
     {// Each has two out of three gains negative, but it's different
      // for the different models. So, this will be right in two out
      // of three axis for either model.
      {1, 0, 1, 0, -51.5},
      {1, 1, 1, -6, -51.5},
      {1, 2, 1, 0, -51.5}}},
    // All MacBook, MacBookPro models. Hardware (at least on early MacBookPro 15")
    // is Kionix KXM52-1050 three-axis accelerometer chip. Data is at
    // http://kionix.com/Product-Index/product-index.htm. Specific MB and MBP models
    // that use this are:
    //		MacBook1,1
    //		MacBook2,1
    //		MacBook3,1
    //		MacBook4,1
    //		MacBook5,1
    //		MacBook6,1
    //		MacBookAir1,1
    //		MacBookPro1,1
    //		MacBookPro1,2
    //		MacBookPro4,1
    //		MacBookPro5,5
    {"", "SMCMotionSensor", 5, 40, {{1, 0, 2, 0, 251}, {1, 2, 2, 0, 251}, {1, 4, 2, 0, 251}}}};

#define SENSOR_COUNT (sizeof(sensors) / sizeof(sensorSpec))

#pragma mark Internal prototypes

static int getData(sms_acceleration* accel, int calibrated, id logObject, SEL logSelector);
static float getAxis(int which, int calibrated);
static int signExtend(int value, int size);
static NSString* getModelName(void);
static NSString* getOSVersion(void);
static BOOL loadCalibration(void);
static void storeCalibration(void);
static void defaultCalibration(void);
static void deleteCalibration(void);
static int prefIntRead(NSString* prefName, BOOL* success);
static void prefIntWrite(NSString* prefName, int prefValue);
static float prefFloatRead(NSString* prefName, BOOL* success);
static void prefFloatWrite(NSString* prefName, float prefValue);
static void prefDelete(NSString* prefName);
static void prefSynchronize(void);
// static long getMicroseconds(void);
float fakeData(NSTimeInterval time);

#pragma mark Static variables

static int debugging = NO;          // True if debugging (synthetic data)
static io_connect_t connection;     // Connection for reading accel values
static int running = NO;            // True if we successfully started
static unsigned int sensorNum = 0;  // The current index into sensors[]
static const char* serviceName;     // The name of the current service
static char *iRecord, *oRecord;     // Pointers to read/write records for sensor
static int recordSize;              // Size of read/write records
static unsigned int function;       // Which kernel function should be used
static float zeros[3];              // X, Y and Z zero calibration values
static float onegs[3];              // X, Y and Z one-g calibration values

#pragma mark Defines

// Pattern for building axis letter from axis number
#define INT_TO_AXIS(a) (a == 0 ? @"X" : a == 1 ? @"Y" : @"Z")
// Name of configuration for given axis' zero (axis specified by integer)
#define ZERO_NAME(a) [NSString stringWithFormat:@"%@-Axis-Zero", INT_TO_AXIS(a)]
// Name of configuration for given axis' oneg (axis specified by integer)
#define ONEG_NAME(a) [NSString stringWithFormat:@"%@-Axis-One-g", INT_TO_AXIS(a)]
// Name of "Is calibrated" preference
#define CALIBRATED_NAME (@"Calibrated")
// Application domain for SeisMac library
#define APP_ID ((CFStringRef) @"com.suitable.SeisMacLib")

// These #defines make the accelStartup code a LOT easier to read.
#undef LOG
#define LOG(message)                                            \
  if (logObject) {                                              \
    [logObject performSelector:logSelector withObject:message]; \
  }
#define LOG_ARG(format, var1)                                                                    \
  if (logObject) {                                                                               \
    [logObject performSelector:logSelector withObject:[NSString stringWithFormat:format, var1]]; \
  }
#define LOG_2ARG(format, var1, var2)                                            \
  if (logObject) {                                                              \
    [logObject performSelector:logSelector                                      \
                    withObject:[NSString stringWithFormat:format, var1, var2]]; \
  }
#define LOG_3ARG(format, var1, var2, var3)                                            \
  if (logObject) {                                                                    \
    [logObject performSelector:logSelector                                            \
                    withObject:[NSString stringWithFormat:format, var1, var2, var3]]; \
  }

#pragma mark Function definitions

// This starts up the accelerometer code, trying each possible sensor
// specification. Note that for logging purposes it
// takes an object and a selector; the object's selector is then invoked
// with a single NSString as argument giving progress messages. Example
// logging method:
//		- (void)logMessage: (NSString *)theString
// which would be used in accelStartup's invocation thusly:
//		result = accelStartup(self, @selector(logMessage:));
// If the object is nil, then no logging is done. Sets calibation from built-in
// value table. Returns ACCEL_SUCCESS for success, and other (negative)
// values for various failures (returns value indicating result of
// most successful trial).
int smsStartup(id logObject, SEL logSelector) {
  io_iterator_t iterator;
  io_object_t device;
  kern_return_t result;
  sms_acceleration accel;
  int failure_result = SMS_FAIL_MODEL;

  running = NO;
  debugging = NO;

  NSString* modelName = getModelName();

  LOG_ARG(@"Machine model: %@\n", modelName);
  LOG_ARG(@"OS X version: %@\n", getOSVersion());
  LOG_ARG(@"Accelerometer library version: %s\n", SMSLIB_VERSION);

  for (sensorNum = 0; sensorNum < SENSOR_COUNT; sensorNum++) {
    // Set up all specs for this type of sensor
    serviceName = sensors[sensorNum].name;
    recordSize = sensors[sensorNum].recordSize;
    function = sensors[sensorNum].function;

    LOG_3ARG(@"Trying service \"%s\" with selector %d and %d byte record:\n", serviceName, function,
             recordSize);

    NSString* targetName = [NSString stringWithCString:sensors[sensorNum].model
                                              encoding:NSMacOSRomanStringEncoding];
    LOG_ARG(@"    Comparing model name to target \"%@\": ", targetName);
    if ([targetName length] == 0 || [modelName hasPrefix:targetName]) {
      LOG(@"success.\n");
    } else {
      LOG(@"failure.\n");
      // Don't need to increment failure_result.
      continue;
    }

    LOG(@"    Fetching dictionary for service: ");
    CFMutableDictionaryRef dict = IOServiceMatching(serviceName);

    if (dict) {
      LOG(@"success.\n");
    } else {
      LOG(@"failure.\n");
      if (failure_result < SMS_FAIL_DICTIONARY) {
        failure_result = SMS_FAIL_DICTIONARY;
      }
      continue;
    }

    LOG(@"    Getting list of matching services: ");
    result = IOServiceGetMatchingServices(kIOMasterPortDefault, dict, &iterator);

    if (result == KERN_SUCCESS) {
      LOG(@"success.\n");
    } else {
      LOG_ARG(@"failure, with return value 0x%x.\n", result);
      if (failure_result < SMS_FAIL_LIST_SERVICES) {
        failure_result = SMS_FAIL_LIST_SERVICES;
      }
      continue;
    }

    LOG(@"    Getting first device in list: ");
    device = IOIteratorNext(iterator);

    if (device == 0) {
      LOG(@"failure.\n");
      if (failure_result < SMS_FAIL_NO_SERVICES) {
        failure_result = SMS_FAIL_NO_SERVICES;
      }
      continue;
    } else {
      LOG(@"success.\n");
      LOG(@"    Opening device: ");
    }

    result = IOServiceOpen(device, mach_task_self(), 0, &connection);

    if (result != KERN_SUCCESS) {
      LOG_ARG(@"failure, with return value 0x%x.\n", result);
      IOObjectRelease(device);
      if (failure_result < SMS_FAIL_OPENING) {
        failure_result = SMS_FAIL_OPENING;
      }
      continue;
    } else if (connection == 0) {
      LOG_ARG(@"'success', but didn't get a connection (return value was: 0x%x).\n", result);
      IOObjectRelease(device);
      if (failure_result < SMS_FAIL_CONNECTION) {
        failure_result = SMS_FAIL_CONNECTION;
      }
      continue;
    } else {
      IOObjectRelease(device);
      LOG(@"success.\n");
    }
    LOG(@"    Testing device.\n");

    defaultCalibration();

    iRecord = (char*)malloc(recordSize);
    oRecord = (char*)malloc(recordSize);

    running = YES;
    result = getData(&accel, true, logObject, logSelector);
    running = NO;

    if (result) {
      LOG_ARG(@"    Failure testing device, with result 0x%x.\n", result);
      free(iRecord);
      iRecord = 0;
      free(oRecord);
      oRecord = 0;
      if (failure_result < SMS_FAIL_ACCESS) {
        failure_result = SMS_FAIL_ACCESS;
      }
      continue;
    } else {
      LOG(@"    Success testing device!\n");
      running = YES;
      return SMS_SUCCESS;
    }
  }
  return failure_result;
}

// This starts up the library in debug mode, ignoring the actual hardware.
// Returned data is in the form of 1Hz sine waves, with the X, Y and Z
// axes 120 degrees out of phase; "calibrated" data has range +/- (1.0/5);
// "uncalibrated" data has range +/- (256/5). X and Y axes centered on 0.0,
// Z axes centered on 1 (calibrated) or 256 (uncalibrated).
// Don't use smsGetBufferLength or smsGetBufferData. Always returns SMS_SUCCESS.
int smsDebugStartup(id logObject, SEL logSelector) {
  LOG(@"Starting up in debug mode\n");
  debugging = YES;
  return SMS_SUCCESS;
}

// Returns the current calibration values.
void smsGetCalibration(sms_calibration* calibrationRecord) {
  int x;

  for (x = 0; x < 3; x++) {
    calibrationRecord->zeros[x] = (debugging ? 0 : zeros[x]);
    calibrationRecord->onegs[x] = (debugging ? 256 : onegs[x]);
  }
}

// Sets the calibration, but does NOT store it as a preference. If the argument
// is nil then the current calibration is set from the built-in value table.
void smsSetCalibration(sms_calibration* calibrationRecord) {
  int x;

  if (!debugging) {
    if (calibrationRecord) {
      for (x = 0; x < 3; x++) {
        zeros[x] = calibrationRecord->zeros[x];
        onegs[x] = calibrationRecord->onegs[x];
      }
    } else {
      defaultCalibration();
    }
  }
}

// Stores the current calibration values as a stored preference.
void smsStoreCalibration(void) {
  if (!debugging) storeCalibration();
}

// Loads the stored preference values into the current calibration.
// Returns YES if successful.
BOOL smsLoadCalibration(void) {
  if (debugging) {
    return YES;
  } else if (loadCalibration()) {
    return YES;
  } else {
    defaultCalibration();
    return NO;
  }
}

// Deletes any stored calibration, and then takes the current calibration values
// from the built-in value table.
void smsDeleteCalibration(void) {
  if (!debugging) {
    deleteCalibration();
    defaultCalibration();
  }
}

// Fills in the accel record with calibrated acceleration data. Takes
// 1-2ms to return a value. Returns 0 if success, error number if failure.
int smsGetData(sms_acceleration* accel) {
  NSTimeInterval time;
  if (debugging) {
    usleep(1500);  // Usually takes 1-2 milliseconds
    time = [NSDate timeIntervalSinceReferenceDate];
    accel->x = fakeData(time) / 5;
    accel->y = fakeData(time - 1) / 5;
    accel->z = fakeData(time - 2) / 5 + 1.0;
    return true;
  } else {
    return getData(accel, true, nil, nil);
  }
}

// Fills in the accel record with uncalibrated acceleration data.
// Returns 0 if success, error number if failure.
int smsGetUncalibratedData(sms_acceleration* accel) {
  NSTimeInterval time;
  if (debugging) {
    usleep(1500);  // Usually takes 1-2 milliseconds
    time = [NSDate timeIntervalSinceReferenceDate];
    accel->x = fakeData(time) * 256 / 5;
    accel->y = fakeData(time - 1) * 256 / 5;
    accel->z = fakeData(time - 2) * 256 / 5 + 256;
    return true;
  } else {
    return getData(accel, false, nil, nil);
  }
}

// Returns the length of a raw block of data for the current type of sensor.
int smsGetBufferLength(void) {
  if (debugging) {
    return 0;
  } else if (running) {
    return sensors[sensorNum].recordSize;
  } else {
    return 0;
  }
}

// Takes a pointer to accelGetRawLength() bytes; sets those bytes
// to return value from sensor. Make darn sure the buffer length is right!
void smsGetBufferData(char* buffer) {
  IOItemCount iSize = recordSize;
  IOByteCount oSize = recordSize;
  kern_return_t result;

  if (debugging || running == NO) {
    return;
  }

  memset(iRecord, 1, iSize);
  memset(buffer, 0, oSize);
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 1050
  const size_t InStructSize = recordSize;
  size_t OutStructSize = recordSize;
  result =
      IOConnectCallStructMethod(connection,
                                function,  // magic kernel function number
                                (const void*)iRecord, InStructSize, (void*)buffer, &OutStructSize);
#else   // __MAC_OS_X_VERSION_MIN_REQUIRED 1050
  result = IOConnectMethodStructureIStructureO(connection,
                                               function,  // magic kernel function number
                                               iSize, &oSize, iRecord, buffer);
#endif  // __MAC_OS_X_VERSION_MIN_REQUIRED 1050

  if (result != KERN_SUCCESS) {
    running = NO;
  }
}

// This returns an NSString describing the current calibration in
// human-readable form. Also include a description of the machine.
NSString* smsGetCalibrationDescription(void) {
  BOOL success;
  NSMutableString* s = [[NSMutableString alloc] init];

  if (debugging) {
    [s release];
    return @"Debugging!";
  }

  [s appendString:@"---- SeisMac Calibration Record ----\n \n"];
  [s appendFormat:@"Machine model: %@\n", getModelName()];
  [s appendFormat:@"OS X build: %@\n", getOSVersion()];
  [s appendFormat:@"SeisMacLib version %s, record %d\n \n", SMSLIB_VERSION, sensorNum];
  [s appendFormat:@"Using service \"%s\", function index %d, size %d\n \n", serviceName, function,
                  recordSize];
  if (prefIntRead(CALIBRATED_NAME, &success) && success) {
    [s appendString:@"Calibration values (from calibration):\n"];
  } else {
    [s appendString:@"Calibration values (from defaults):\n"];
  }
  [s appendFormat:@"    X-Axis-Zero  = %.2f\n", zeros[0]];
  [s appendFormat:@"    X-Axis-One-g = %.2f\n", onegs[0]];
  [s appendFormat:@"    Y-Axis-Zero  = %.2f\n", zeros[1]];
  [s appendFormat:@"    Y-Axis-One-g = %.2f\n", onegs[1]];
  [s appendFormat:@"    Z-Axis-Zero  = %.2f\n", zeros[2]];
  [s appendFormat:@"    Z-Axis-One-g = %.2f\n \n", onegs[2]];
  [s appendString:@"---- End Record ----\n"];
  return s;
}

// Shuts down the accelerometer.
void smsShutdown(void) {
  if (!debugging) {
    running = NO;
    if (iRecord) free(iRecord);
    if (oRecord) free(oRecord);
    IOServiceClose(connection);
  }
}

#pragma mark Internal functions

// Loads the current calibration from the stored preferences.
// Returns true iff successful.
BOOL loadCalibration(void) {
  BOOL thisSuccess, allSuccess;
  int x;

  prefSynchronize();

  if (prefIntRead(CALIBRATED_NAME, &thisSuccess) && thisSuccess) {
    // Calibrated. Set all values from saved values.
    allSuccess = YES;
    for (x = 0; x < 3; x++) {
      zeros[x] = prefFloatRead(ZERO_NAME(x), &thisSuccess);
      allSuccess &= thisSuccess;
      onegs[x] = prefFloatRead(ONEG_NAME(x), &thisSuccess);
      allSuccess &= thisSuccess;
    }
    return allSuccess;
  }

  return NO;
}

// Stores the current calibration into the stored preferences.
static void storeCalibration(void) {
  int x;
  prefIntWrite(CALIBRATED_NAME, 1);
  for (x = 0; x < 3; x++) {
    prefFloatWrite(ZERO_NAME(x), zeros[x]);
    prefFloatWrite(ONEG_NAME(x), onegs[x]);
  }
  prefSynchronize();
}

// Sets the calibration to its default values.
void defaultCalibration(void) {
  int x;
  for (x = 0; x < 3; x++) {
    zeros[x] = sensors[sensorNum].axes[x].zerog;
    onegs[x] = sensors[sensorNum].axes[x].oneg;
  }
}

// Deletes the stored preferences.
static void deleteCalibration(void) {
  int x;

  prefDelete(CALIBRATED_NAME);
  for (x = 0; x < 3; x++) {
    prefDelete(ZERO_NAME(x));
    prefDelete(ONEG_NAME(x));
  }
  prefSynchronize();
}

// Read a named floating point value from the stored preferences. Sets
// the success boolean based on, you guessed it, whether it succeeds.
static float prefFloatRead(NSString* prefName, BOOL* success) {
  float result = 0.0f;

  CFPropertyListRef ref = CFPreferencesCopyAppValue((CFStringRef)prefName, APP_ID);
  // If there isn't such a preference, fail
  if (ref == NULL) {
    *success = NO;
    return result;
  }
  CFTypeID typeID = CFGetTypeID(ref);
  // Is it a number?
  if (typeID == CFNumberGetTypeID()) {
    // Is it a floating point number?
    if (CFNumberIsFloatType((CFNumberRef)ref)) {
      // Yup: grab it.
      *success = CFNumberGetValue((__CFNumber*)ref, kCFNumberFloat32Type, &result);
    } else {
      // Nope: grab as an integer, and convert to a float.
      long num;
      if (CFNumberGetValue((CFNumberRef)ref, kCFNumberLongType, &num)) {
        result = num;
        *success = YES;
      } else {
        *success = NO;
      }
    }
    // Or is it a string (e.g. set by the command line "defaults" command)?
  } else if (typeID == CFStringGetTypeID()) {
    result = (float)CFStringGetDoubleValue((CFStringRef)ref);
    *success = YES;
  } else {
    // Can't convert to a number: fail.
    *success = NO;
  }
  CFRelease(ref);
  return result;
}

// Writes a named floating point value to the stored preferences.
static void prefFloatWrite(NSString* prefName, float prefValue) {
  CFNumberRef cfFloat = CFNumberCreate(kCFAllocatorDefault, kCFNumberFloatType, &prefValue);
  CFPreferencesSetAppValue((CFStringRef)prefName, cfFloat, APP_ID);
  CFRelease(cfFloat);
}

// Reads a named integer value from the stored preferences.
static int prefIntRead(NSString* prefName, BOOL* success) {
  Boolean internalSuccess;
  CFIndex result = CFPreferencesGetAppIntegerValue((CFStringRef)prefName, APP_ID, &internalSuccess);
  *success = internalSuccess;

  return result;
}

// Writes a named integer value to the stored preferences.
static void prefIntWrite(NSString* prefName, int prefValue) {
  CFPreferencesSetAppValue((CFStringRef)prefName, (CFNumberRef)[NSNumber numberWithInt:prefValue],
                           APP_ID);
}

// Deletes the named preference values.
static void prefDelete(NSString* prefName) {
  CFPreferencesSetAppValue((CFStringRef)prefName, NULL, APP_ID);
}

// Synchronizes the local preferences with the stored preferences.
static void prefSynchronize(void) { CFPreferencesAppSynchronize(APP_ID); }

// Internal version of accelGetData, with logging
int getData(sms_acceleration* accel, int calibrated, id logObject, SEL logSelector) {
  IOItemCount iSize = recordSize;
  IOByteCount oSize = recordSize;
  kern_return_t result;

  if (running == NO) {
    return -1;
  }

  memset(iRecord, 1, iSize);
  memset(oRecord, 0, oSize);

  LOG_2ARG(@"    Querying device (%u, %d): ", sensors[sensorNum].function,
           sensors[sensorNum].recordSize);

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 1050
  const size_t InStructSize = recordSize;
  size_t OutStructSize = recordSize;
  result =
      IOConnectCallStructMethod(connection,
                                function,  // magic kernel function number
                                (const void*)iRecord, InStructSize, (void*)oRecord, &OutStructSize);
#else   // __MAC_OS_X_VERSION_MIN_REQUIRED 1050
  result = IOConnectMethodStructureIStructureO(connection,
                                               function,  // magic kernel function number
                                               iSize, &oSize, iRecord, oRecord);
#endif  // __MAC_OS_X_VERSION_MIN_REQUIRED 1050

  if (result != KERN_SUCCESS) {
    LOG(@"failed.\n");
    running = NO;
    return result;
  } else {
    LOG(@"succeeded.\n");

    accel->x = getAxis(0, calibrated);
    accel->y = getAxis(1, calibrated);
    accel->z = getAxis(2, calibrated);
    return 0;
  }
}

// Given the returned record, extracts the value of the given axis. If
// calibrated, then zero G is 0.0, and one G is 1.0.
float getAxis(int which, int calibrated) {
  // Get various values (to make code cleaner)
  int indx = sensors[sensorNum].axes[which].index;
  int size = sensors[sensorNum].axes[which].size;
  float zerog = zeros[which];
  float oneg = onegs[which];
  // Storage for value to be returned
  int value = 0;

  // Although the values in the returned record should have the proper
  // endianness, we still have to get it into the proper end of value.
#if (BYTE_ORDER == BIG_ENDIAN)
  // On PowerPC processors
  memcpy(((char*)&value) + (sizeof(int) - size), &oRecord[indx], size);
#endif
#if (BYTE_ORDER == LITTLE_ENDIAN)
  // On Intel processors
  memcpy(&value, &oRecord[indx], size);
#endif

  value = signExtend(value, size);

  if (calibrated) {
    // Scale and shift for zero.
    return ((float)(value - zerog)) / oneg;
  } else {
    return value;
  }
}

// Extends the sign, given the length of the value.
int signExtend(int value, int size) {
  // Extend sign
  switch (size) {
    case 1:
      if (value & 0x00000080) value |= 0xffffff00;
      break;
    case 2:
      if (value & 0x00008000) value |= 0xffff0000;
      break;
    case 3:
      if (value & 0x00800000) value |= 0xff000000;
      break;
  }
  return value;
}

// Returns the model name of the computer (e.g. "MacBookPro1,1")
NSString* getModelName(void) {
  char model[32];
  size_t len = sizeof(model);
  int name[2] = {CTL_HW, HW_MODEL};
  NSString* result;

  if (sysctl(name, 2, &model, &len, NULL, 0) == 0) {
    result = [NSString stringWithFormat:@"%s", model];
  } else {
    result = @"";
  }

  return result;
}

// Returns the current OS X version and build (e.g. "10.4.7 (build 8J2135a)")
NSString* getOSVersion(void) {
  NSDictionary* dict = [NSDictionary
      dictionaryWithContentsOfFile:@"/System/Library/CoreServices/SystemVersion.plist"];
  NSString* versionString = [dict objectForKey:@"ProductVersion"];
  NSString* buildString = [dict objectForKey:@"ProductBuildVersion"];
  NSString* wholeString = [NSString stringWithFormat:@"%@ (build %@)", versionString, buildString];
  return wholeString;
}

// Returns time within the current second in microseconds.
// long getMicroseconds() {
//	struct timeval t;
//	gettimeofday(&t, 0);
//	return t.tv_usec;
//}

// Returns fake data given the time. Range is +/-1.
float fakeData(NSTimeInterval time) {
  long secs = lround(floor(time));
  int secsMod3 = secs % 3;
  double angle = time * 10 * M_PI * 2;
  double mag = exp(-(time - (secs - secsMod3)) * 2);
  return sin(angle) * mag;
}
