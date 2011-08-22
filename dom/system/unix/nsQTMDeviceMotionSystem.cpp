/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Oleg Romashin <romaxa@gmail.com>
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

#include "mozilla/dom/ContentChild.h"
#include "nsQTMDeviceMotionSystem.h"
#include <QObject>
#include <QtSensors/QAccelerometer>
#include <QtSensors/QRotationSensor>

#include "nsXULAppAPI.h"

using namespace mozilla;
using namespace QtMobility;

bool
MozQAccelerometerSensorFilter::filter(QAccelerometerReading* reading)
{
    mSystem.DeviceMotionChanged(nsIDeviceMotionData::TYPE_ACCELERATION,
                                -reading->x(),
                                -reading->y(),
                                -reading->z());
    return true;
}

bool
MozQRotationSensorFilter::filter(QRotationReading* reading)
{
    // QRotationReading class:
    // - the rotation around z axis (alpha) is given as z in QRotationReading;
    // - the rotation around x axis (beta) is given as x in QRotationReading;
    // - the rotation around y axis (gamma) is given as y in QRotationReading;
    // See: http://doc.qt.nokia.com/qtmobility-1.0/qrotationreading.html
    mSystem.DeviceMotionChanged(nsIDeviceMotionData::TYPE_ORIENTATION,
                                reading->z(),
                                reading->x(),
                                reading->y());
    return true;
}

nsDeviceMotionSystem::nsDeviceMotionSystem()
{
    if (XRE_GetProcessType() == GeckoProcessType_Default) {
        mAccelerometer = new QAccelerometer();
        mAccelerometerFilter = new MozQAccelerometerSensorFilter(*this);
        mAccelerometer->addFilter(mAccelerometerFilter);
        mRotation = new QRotationSensor();
        mRotationFilter = new MozQRotationSensorFilter(*this);
        mRotation->addFilter(mRotationFilter);
    }
}

nsDeviceMotionSystem::~nsDeviceMotionSystem()
{
    if (XRE_GetProcessType() == GeckoProcessType_Default) {
        mAccelerometer->removeFilter(mAccelerometerFilter);
        mAccelerometer->stop();
        mRotation->removeFilter(mRotationFilter);
        mRotation->stop();
        delete mAccelerometer;
        delete mAccelerometerFilter;
        delete mRotation;
        delete mRotationFilter;
    }
}

void nsDeviceMotionSystem::Startup()
{
    if (XRE_GetProcessType() == GeckoProcessType_Default) {
        mAccelerometer->start();
        if (!mAccelerometer->isActive()) {
            NS_WARNING("AccelerometerSensor didn't start!");
        }
        mRotation->start();
        if (!mRotation->isActive()) {
            NS_WARNING("RotationSensor didn't start!");
        }
    }
    else
        mozilla::dom::ContentChild::GetSingleton()->
            SendAddDeviceMotionListener();
}

void nsDeviceMotionSystem::Shutdown()
{
    if (XRE_GetProcessType() == GeckoProcessType_Default) {
        mAccelerometer->stop();
        mRotation->stop();
    }
    else
        mozilla::dom::ContentChild::GetSingleton()->
            SendRemoveDeviceMotionListener();
}
