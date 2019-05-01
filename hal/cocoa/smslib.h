/*
 * smslib.h
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

#import <Foundation/Foundation.h>

#define SMSLIB_VERSION "1.8"

#pragma mark Structure definitions

// Structure for specifying a 3-axis acceleration. 0.0 means "zero gravities",
// 1.0 means "one gravity".
typedef struct sms_acceleration {
  float x;  // Right-left acceleration (positive is rightwards)
  float y;  // Front-rear acceleration (positive is rearwards)
  float z;  // Up-down acceleration (positive is upwards)
} sms_acceleration;

// Structure for specifying a calibration.
typedef struct sms_calibration {
  float zeros[3];  // Zero points for three axes (X, Y, Z)
  float onegs[3];  // One gravity values for three axes
} sms_calibration;

#pragma mark Return value definitions

// These are the return values for accelStartup(), giving the
// various stages where the most successful attempt at accessing
// the accelerometer failed. The higher the value, the further along the
// software progressed before failing. The options are:
//	- Didn't match model name
#define SMS_FAIL_MODEL (-7)
//	- Failure getting dictionary matching desired services
#define SMS_FAIL_DICTIONARY (-6)
//	- Failure getting list of services
#define SMS_FAIL_LIST_SERVICES (-5)
//	- Failure if list of services is empty. The process generally fails
//		here if run on a machine without a Sudden Motion Sensor.
#define SMS_FAIL_NO_SERVICES (-4)
//	- Failure if error opening device.
#define SMS_FAIL_OPENING (-3)
//	- Failure if opened, but didn't get a connection
#define SMS_FAIL_CONNECTION (-2)
//	- Failure if couldn't access connction using given function and size. This
//		is where the process would probably fail with a change in Apple's API.
//		Driver problems often also cause failures here.
#define SMS_FAIL_ACCESS (-1)
//	- Success!
#define SMS_SUCCESS (0)

#pragma mark Function declarations

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
int smsStartup(id logObject, SEL logSelector);

// This starts up the library in debug mode, ignoring the actual hardware.
// Returned data is in the form of 1Hz sine waves, with the X, Y and Z
// axes 120 degrees out of phase; "calibrated" data has range +/- (1.0/5);
// "uncalibrated" data has range +/- (256/5). X and Y axes centered on 0.0,
// Z axes centered on 1 (calibrated) or 256 (uncalibrated).
// Don't use smsGetBufferLength or smsGetBufferData. Always returns SMS_SUCCESS.
int smsDebugStartup(id logObject, SEL logSelector);

// Returns the current calibration values.
void smsGetCalibration(sms_calibration* calibrationRecord);

// Sets the calibration, but does NOT store it as a preference. If the argument
// is nil then the current calibration is set from the built-in value table.
void smsSetCalibration(sms_calibration* calibrationRecord);

// Stores the current calibration values as a stored preference.
void smsStoreCalibration(void);

// Loads the stored preference values into the current calibration.
// Returns YES if successful.
BOOL smsLoadCalibration(void);

// Deletes any stored calibration, and then takes the current calibration values
// from the built-in value table.
void smsDeleteCalibration(void);

// Fills in the accel record with calibrated acceleration data. Takes
// 1-2ms to return a value. Returns 0 if success, error number if failure.
int smsGetData(sms_acceleration* accel);

// Fills in the accel record with uncalibrated acceleration data.
// Returns 0 if success, error number if failure.
int smsGetUncalibratedData(sms_acceleration* accel);

// Returns the length of a raw block of data for the current type of sensor.
int smsGetBufferLength(void);

// Takes a pointer to accelGetRawLength() bytes; sets those bytes
// to return value from sensor. Make darn sure the buffer length is right!
void smsGetBufferData(char* buffer);

// This returns an NSString describing the current calibration in
// human-readable form. Also include a description of the machine.
NSString* smsGetCalibrationDescription(void);

// Shuts down the accelerometer.
void smsShutdown(void);
