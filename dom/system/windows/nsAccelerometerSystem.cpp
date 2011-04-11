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
 * Doug Turner <dougt@dougt.org>
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jesper Kristensen <mail@jesperkristensen.dk>
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

#include "nsAccelerometerSystem.h"
#include "nsIServiceManager.h"
#include "windows.h"

////////////////////////////
// ThinkPad
////////////////////////////

typedef struct {
  int status; // Current internal state
  unsigned short x; // raw value
  unsigned short y; // raw value
  unsigned short xx; // avg. of 40ms
  unsigned short yy; // avg. of 40ms
  char temp; // raw value (could be deg celsius?)
  unsigned short x0; // Used for "auto-center"
  unsigned short y0; // Used for "auto-center"
} ThinkPadAccelerometerData;

typedef void (__stdcall *ShockproofGetAccelerometerData)(ThinkPadAccelerometerData*);

ShockproofGetAccelerometerData gShockproofGetAccelerometerData = nsnull;

class ThinkPadSensor : public Sensor
{
public:
  ThinkPadSensor();
  ~ThinkPadSensor();
  PRBool Startup();
  void Shutdown();
  void GetValues(double *x, double *y, double *z);
private:
  HMODULE mLibrary;
};

ThinkPadSensor::ThinkPadSensor()
{
}

ThinkPadSensor::~ThinkPadSensor()
{
}

PRBool
ThinkPadSensor::Startup()
{
  mLibrary = LoadLibraryW(L"sensor.dll");
  if (!mLibrary)
    return PR_FALSE;

  gShockproofGetAccelerometerData = (ShockproofGetAccelerometerData)
    GetProcAddress(mLibrary, "ShockproofGetAccelerometerData");
  if (!gShockproofGetAccelerometerData) {
    FreeLibrary(mLibrary);
    mLibrary = nsnull;
    return PR_FALSE;
  }
  return PR_TRUE;
}

void
ThinkPadSensor::Shutdown()
{
  NS_ASSERTION(mLibrary, "Shutdown called when mLibrary is null?");
  FreeLibrary(mLibrary);
  mLibrary = nsnull;
  gShockproofGetAccelerometerData = nsnull;
}

void
ThinkPadSensor::GetValues(double *x, double *y, double *z)
{
  ThinkPadAccelerometerData accelData;

  gShockproofGetAccelerometerData(&accelData);

  // accelData.x and accelData.y is the acceleration measured from the accelerometer.
  // x and y is switched from what we use, and the accelerometer does not support z axis.
  // Balance point (526, 528) and 90 degree tilt (144) determined experimentally.
  *x = ((double)(accelData.y - 526)) / 144;
  *y = ((double)(accelData.x - 528)) / 144;
  *z = 1.0;
}

nsAccelerometerSystem::nsAccelerometerSystem(){}
nsAccelerometerSystem::~nsAccelerometerSystem(){}

void
nsAccelerometerSystem::UpdateHandler(nsITimer *aTimer, void *aClosure)
{
  nsAccelerometerSystem *self = reinterpret_cast<nsAccelerometerSystem *>(aClosure);
  if (!self || !self->mSensor) {
    NS_ERROR("no self or sensor");
    return;
  }
  double x, y, z;
  self->mSensor->GetValues(&x, &y, &z);
  self->AccelerationChanged(x, y, z);
}

void nsAccelerometerSystem::Startup()
{
  NS_ASSERTION(!mSensor, "mSensor should be null.  Startup called twice?");

  PRBool started = PR_FALSE;

  mSensor = new ThinkPadSensor();
  if (mSensor)
    started = mSensor->Startup();

  if (!started)
    return;

  mUpdateTimer = do_CreateInstance("@mozilla.org/timer;1");
  if (mUpdateTimer)
    mUpdateTimer->InitWithFuncCallback(UpdateHandler,
                                       this,
                                       mUpdateInterval,
                                       nsITimer::TYPE_REPEATING_SLACK);
}

void nsAccelerometerSystem::Shutdown()
{
  if (mUpdateTimer) {
    mUpdateTimer->Cancel();
    mUpdateTimer = nsnull;
  }

  if (mSensor)
    mSensor->Shutdown();
}

