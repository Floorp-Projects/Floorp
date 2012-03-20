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

#include "nsDeviceMotionSystem.h"
#include "nsIServiceManager.h"
#include "stdlib.h"

#include <sys/sysctl.h>
#include <sys/resource.h>
#include <sys/vm.h>

#import "smslib.h"
#define MEAN_GRAVITY 9.80665
#define DEFAULT_SENSOR_POLL 100

nsDeviceMotionSystem::nsDeviceMotionSystem()
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
  sms_acceleration accel;
  smsGetData(&accel);

  self->DeviceMotionChanged(nsIDeviceMotionData::TYPE_ACCELERATION,
			    accel.x * MEAN_GRAVITY,
			    accel.y * MEAN_GRAVITY,
			    accel.z * MEAN_GRAVITY);
}

void nsDeviceMotionSystem::Startup()
{
  smsStartup(nil, nil);
  smsLoadCalibration();

  mUpdateTimer = do_CreateInstance("@mozilla.org/timer;1");
  if (mUpdateTimer)
    mUpdateTimer->InitWithFuncCallback(UpdateHandler,
                                       this,
                                       DEFAULT_SENSOR_POLL,
                                       nsITimer::TYPE_REPEATING_SLACK);
}

void nsDeviceMotionSystem::Shutdown()
{
  if (mUpdateTimer) {
    mUpdateTimer->Cancel();
    mUpdateTimer = nsnull;
  }

  smsShutdown();
}

