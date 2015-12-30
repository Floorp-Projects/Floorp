/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <math.h>
#include "VRDeviceProxyOrientationFallBack.h"
#include "mozilla/dom/ScreenOrientation.h" // for ScreenOrientationInternal
#include "mozilla/Hal.h"

using namespace mozilla;
using namespace mozilla::gfx;

// 1/sqrt(2) (aka sqrt(2)/2)
#ifndef M_SQRT1_2
# define M_SQRT1_2	0.70710678118654752440
#endif

#ifdef ANDROID
#include <android/log.h>
#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "GeckoVR" , ## args)
#else
#define LOG(...) do { } while(0)
#endif

namespace {
// some utility functions

// This remaps axes in the given matrix to a new configuration based on the
// screen orientation.  Similar to what Android SensorManager.remapCoordinateSystem
// does, except only for a fixed number of transforms that we need.
Matrix4x4
RemapMatrixForOrientation(dom::ScreenOrientationInternal screenConfig, const Matrix4x4& aMatrix)
{
  Matrix4x4 out;
  const float *in = &aMatrix._11;
  float *o = &out._11;

  if (screenConfig == dom::eScreenOrientation_LandscapePrimary) {
    // remap X,Y -> Y,-X
    o[0] = -in[1]; o[1] = in[0]; o[2] = in[2];
    o[4] = -in[5]; o[5] = in[4]; o[6] = in[6];
    o[8] = -in[9]; o[9] = in[8]; o[10] = in[10];
  } else if (screenConfig == dom::eScreenOrientation_LandscapeSecondary) {
    // remap X,Y -> -Y,X
    o[0] = in[1]; o[1] = -in[0]; o[2] = in[2];
    o[4] = in[5]; o[5] = -in[4]; o[6] = in[6];
    o[8] = in[9]; o[9] = -in[8]; o[10] = in[10];
  } else if (screenConfig == dom::eScreenOrientation_PortraitPrimary) {
    out = aMatrix;
  } else if (screenConfig == dom::eScreenOrientation_PortraitSecondary) {
    // remap X,Y -> X,-Z
    o[0] = in[0]; o[1] = in[2]; o[2] = -in[1];
    o[4] = in[4]; o[5] = in[6]; o[6] = -in[5];
    o[8] = in[8]; o[9] = in[10]; o[10] = -in[9];
  } else {
    MOZ_ASSERT(0, "gfxVRCardboard::RemapMatrixForOrientation invalid screenConfig");
  }

  return out;
}

} // namespace

namespace mozilla {
namespace gfx {

VRDeviceProxyOrientationFallBack::VRDeviceProxyOrientationFallBack(const VRDeviceUpdate& aDeviceUpdate)
  : VRDeviceProxy(aDeviceUpdate)
  , mOrient(dom::eScreenOrientation_PortraitPrimary)
  , mTracking(false)
{
  MOZ_COUNT_CTOR_INHERITED(VRDeviceProxyOrientationFallBack, VRDeviceProxy);
}

VRDeviceProxyOrientationFallBack::~VRDeviceProxyOrientationFallBack()
{
  StopSensorTracking();
  MOZ_COUNT_DTOR_INHERITED(VRDeviceProxyOrientationFallBack, VRDeviceProxy);
}

void
VRDeviceProxyOrientationFallBack::StartSensorTracking()
{
  if (!mTracking) {
    // it's never been started before; initialize observers and
    // initial state.

    hal::ScreenConfiguration sconfig;
    hal::GetCurrentScreenConfiguration(&sconfig);
    this->Notify(sconfig);

    hal::RegisterSensorObserver(hal::SENSOR_GAME_ROTATION_VECTOR, this);
    hal::RegisterScreenConfigurationObserver(this);

    mSensorState.Clear();

    mTracking = true;
  }
}

void
VRDeviceProxyOrientationFallBack::StopSensorTracking()
{
  if (mTracking) {
    hal::UnregisterScreenConfigurationObserver(this);
    hal::UnregisterSensorObserver(hal::SENSOR_GAME_ROTATION_VECTOR, this);
    mTracking = false;
  }
}

// Android sends us events that have a 90-degree rotation about 
// the x axis compared to what we want (phone flat vs. phone held in front of the eyes).
// Correct for this by applying a transform to undo this rotation.
void
VRDeviceProxyOrientationFallBack::Notify(const hal::ScreenConfiguration& config)
{
  mOrient = config.orientation();

  if (mOrient == dom::eScreenOrientation_LandscapePrimary) {
    mScreenTransform = Quaternion(-0.5f, 0.5f, 0.5f, 0.5f);
  } else if (mOrient == dom::eScreenOrientation_LandscapeSecondary) {
    mScreenTransform = Quaternion(-0.5f, -0.5f, -0.5f, 0.5f);
  } else if (mOrient == dom::eScreenOrientation_PortraitPrimary) {
    mScreenTransform = Quaternion((float) -M_SQRT1_2, 0.f, 0.f, (float) M_SQRT1_2);
  } else if (mOrient == dom::eScreenOrientation_PortraitSecondary) {
    // Currently, PortraitSecondary event doesn't be triggered.
    mScreenTransform = Quaternion((float) M_SQRT1_2, 0.f, 0.f, (float) M_SQRT1_2);
  }
}

void
VRDeviceProxyOrientationFallBack::Notify(const hal::SensorData& data)
{
  if (data.sensor() != hal::SENSOR_GAME_ROTATION_VECTOR)
    return;

  const nsTArray<float>& sensorValues = data.values();

  // This is super chatty
  //LOG("HMDInfoCardboard::Notify %f %f %f %f\n", sensorValues[0], sensorValues[1], sensorValues[2], sensorValues[3]);

  mSavedLastSensor.Set(sensorValues[0], sensorValues[1], sensorValues[2], sensorValues[3]);
  mSavedLastSensorTime = data.timestamp();
  mNeedsSensorCompute = true;
}

void
VRDeviceProxyOrientationFallBack::ZeroSensor()
{
  mSensorZeroInverse = mSavedLastSensor;
  mSensorZeroInverse.Invert();
}

void
VRDeviceProxyOrientationFallBack::ComputeStateFromLastSensor()
{
  if (!mNeedsSensorCompute)
    return;

  // apply the zero orientation
  Quaternion q = mSensorZeroInverse * mSavedLastSensor;

  // make a matrix from the quat
  Matrix4x4 qm;
  qm.SetRotationFromQuaternion(q);

  // remap the coordinate space, based on the orientation
  Matrix4x4 qmRemapped = RemapMatrixForOrientation(mOrient, qm);

  // turn it back into a quat
  q.SetFromRotationMatrix(qmRemapped);

  // apply adjustment based on what's been done to the screen and the original zero
  // position of the base coordinate space
  q = mScreenTransform * q;

  mSensorState.flags |= VRStateValidFlags::State_Orientation;
  mSensorState.orientation[0] = q.x;
  mSensorState.orientation[1] = q.y;
  mSensorState.orientation[2] = q.z;
  mSensorState.orientation[3] = q.w;

  mSensorState.timestamp = mSavedLastSensorTime / 1000000.0;

  mNeedsSensorCompute = false;
}

VRHMDSensorState
VRDeviceProxyOrientationFallBack::GetSensorState(double timeOffset)
{
  StartSensorTracking();
  ComputeStateFromLastSensor();
  return mSensorState;
}

} // namespace gfx
} // namespace mozilla

