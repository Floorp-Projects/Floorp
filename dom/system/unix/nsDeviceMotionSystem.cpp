/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Michael Ventnor <m.ventnor@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <unistd.h>

#include "mozilla/Util.h"

#include "nsDeviceMotionSystem.h"
#include "nsIServiceManager.h"

#define DEFAULT_SENSOR_POLL 100

using namespace mozilla;

typedef struct {
  const char* mPosition;
  const char* mCalibrate;
  nsAccelerometerSystemDriver mToken;
} AccelerometerData;

static const AccelerometerData gAccelerometers[] = {
  // MacBook
  {"/sys/devices/platform/applesmc.768/position",
   "/sys/devices/platform/applesmc.768/calibrate",
   eAppleSensor},
  // Thinkpad
  {"/sys/devices/platform/hdaps/position",
   "/sys/devices/platform/hdaps/calibrate",
   eIBMSensor},
  // Maemo Fremantle
  {"/sys/class/i2c-adapter/i2c-3/3-001d/coord",
   NULL,
   eMaemoSensor},
  // HP Pavilion dv7
  {"/sys/devices/platform/lis3lv02d/position",
   "/sys/devices/platform/lis3lv02d/calibrate",
   eHPdv7Sensor},
};

nsDeviceMotionSystem::nsDeviceMotionSystem() :
  mPositionFile(NULL),
  mCalibrateFile(NULL),
  mType(eNoSensor)
{
}

nsDeviceMotionSystem::~nsDeviceMotionSystem()
{
}

void
nsDeviceMotionSystem::UpdateHandler(nsITimer *aTimer, void *aClosure)
{
  nsDeviceMotionSystem *self = reinterpret_cast<nsDeviceMotionSystem *>(aClosure);
  if (!self) {
    NS_ERROR("no self");
    return;
  }

  float xf, yf, zf;

  switch (self->mType) {
    case eAppleSensor:
    {
      int x, y, z, calibrate_x, calibrate_y;
      fflush(self->mCalibrateFile);
      rewind(self->mCalibrateFile);

      fflush(self->mPositionFile);
      rewind(self->mPositionFile);

      if (fscanf(self->mCalibrateFile, "(%d, %d)", &calibrate_x, &calibrate_y) <= 0)
        return;

      if (fscanf(self->mPositionFile, "(%d, %d, %d)", &x, &y, &z) <= 0)
        return;

      // In applesmc:
      // - the x calibration value is negated
      // - a negative z actually means a correct-way-up computer
      // - dividing by 255 gives us the intended value of -1 to 1
      xf = ((float)(x + calibrate_x)) / 255.0;
      yf = ((float)(y - calibrate_y)) / 255.0;
      zf = ((float)z) / -255.0;
      break;
    }
    case eIBMSensor:
    {
      int x, y, calibrate_x, calibrate_y;
      fflush(self->mCalibrateFile);
      rewind(self->mCalibrateFile);

      fflush(self->mPositionFile);
      rewind(self->mPositionFile);

      if (fscanf(self->mCalibrateFile, "(%d, %d)", &calibrate_x, &calibrate_y) <= 0)
        return;

      if (fscanf(self->mPositionFile, "(%d, %d)", &x, &y) <= 0)
        return;

      xf = ((float)(x - calibrate_x)) / 180.0;
      yf = ((float)(y - calibrate_y)) / 180.0;
      zf = 1.0f;
      break;
    }
    case eMaemoSensor:
    {
      int x, y, z;
      fflush(self->mPositionFile);
      rewind(self->mPositionFile);

      if (fscanf(self->mPositionFile, "%d %d %d", &x, &y, &z) <= 0)
        return;

      xf = ((float)x) / -1000.0;
      yf = ((float)y) / -1000.0;
      zf = ((float)z) / -1000.0;
      break;
    }
    case eHPdv7Sensor:
    {
      int x, y, z, calibrate_x, calibrate_y, calibrate_z;
      fflush(self->mCalibrateFile);
      rewind(self->mCalibrateFile);

      fflush(self->mPositionFile);
      rewind(self->mPositionFile);

      if (fscanf(self->mCalibrateFile, "(%d,%d,%d)", &calibrate_x, &calibrate_y, &calibrate_z) <= 0)
        return;

      if (fscanf(self->mPositionFile, "(%d,%d,%d)", &x, &y, &z) <= 0)
        return;

      // Example data:
      //
      // Calibration (-4,0,51)
      // flat on the table (-5,-2,50)
      // Tilted on its left side (-60,0,-4)
      // Tilted on its right side (51,1,-1)
      // upside down (-2,3,-60)
      //
      // So assuming the calibration data shows the acceleration
      // (1G) measured with the notebook laying flat on the table
      // it would mean that our best bet, is to ignore the z-axis
      // calibration...  We are still way off, but it's hard to
      // see how to get better data without doing calibration with
      // user intervention (like: Turn your notbook slowly around
      // so every edge and the top and bottom points up in turn)

      xf = ((float)(x - calibrate_x)) / 60.0;
      yf = ((float)(y - calibrate_y)) / 60.0;
      zf = ((float)(z)) / 60.0;
      break;
    }

    case eNoSensor:
    default:
      return;
  }

  self->DeviceMotionChanged(nsIDeviceMotionData::TYPE_ACCELERATION, xf, yf, zf );
}

void nsDeviceMotionSystem::Startup()
{
  // Accelerometers in Linux are used by reading a file (yay UNIX!), which is
  // in a slightly different location depending on the driver.
  for (unsigned int i = 0; i < ArrayLength(gAccelerometers); i++) {
    if (!(mPositionFile = fopen(gAccelerometers[i].mPosition, "r")))
      continue;

    mType = gAccelerometers[i].mToken;
    if (gAccelerometers[i].mCalibrate) {
      mCalibrateFile = fopen(gAccelerometers[i].mCalibrate, "r");
      if (!mCalibrateFile) {
        fclose(mPositionFile);
        mPositionFile = nsnull;
        return;
      }
    }

    break;
  }

  if (mType == eNoSensor)
    return;

  mUpdateTimer = do_CreateInstance("@mozilla.org/timer;1");
  if (mUpdateTimer)
    mUpdateTimer->InitWithFuncCallback(UpdateHandler,
                                       this,
                                       DEFAULT_SENSOR_POLL,
                                       nsITimer::TYPE_REPEATING_SLACK);
}

void nsDeviceMotionSystem::Shutdown()
{
  if (mPositionFile) {
    fclose(mPositionFile);
    mPositionFile = nsnull;
  }

  // Fun fact: writing to the calibrate file causes the
  // driver to re-calibrate the accelerometer
  if (mCalibrateFile) {
    fclose(mCalibrateFile);
    mCalibrateFile = nsnull;
  }

  mType = eNoSensor;

  if (mUpdateTimer) {
    mUpdateTimer->Cancel();
    mUpdateTimer = nsnull;
  }
}

