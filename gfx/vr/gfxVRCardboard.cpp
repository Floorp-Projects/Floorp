/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <math.h>

#include "prlink.h"
#include "prmem.h"
#include "prenv.h"
#include "gfxPrefs.h"
#include "nsString.h"
#include "mozilla/dom/ScreenOrientation.h"
#include "mozilla/Preferences.h"
#include "mozilla/Hal.h"

#include "gfxVRCardboard.h"

#include "nsServiceManagerUtils.h"
#include "nsIScreenManager.h"

#ifdef ANDROID
#include <android/log.h>
#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "GeckoVR" , ## args)
#else
#define LOG(...) do { } while(0)
#endif

// 1/sqrt(2) (aka sqrt(2)/2)
#ifndef M_SQRT1_2
# define M_SQRT1_2	0.70710678118654752440
#endif

using namespace mozilla::dom;
using namespace mozilla::gfx;
using namespace mozilla::gfx::impl;

namespace {
// some utility functions

// This remaps axes in the given matrix to a new configuration based on the
// screen orientation.  Similar to what Android SensorManager.remapCoordinateSystem
// does, except only for a fixed number of transforms that we need.
Matrix4x4
RemapMatrixForOrientation(ScreenOrientationInternal screenConfig, const Matrix4x4& aMatrix)
{
  Matrix4x4 out;
  const float *in = &aMatrix._11;
  float *o = &out._11;

  if (screenConfig == eScreenOrientation_LandscapePrimary) {
    // remap X,Y -> Y,-X
    o[0] = -in[1]; o[1] = in[0]; o[2] = in[2];
    o[4] = -in[5]; o[5] = in[4]; o[6] = in[6];
    o[8] = -in[9]; o[9] = in[8]; o[10] = in[10];
  } else if (screenConfig == eScreenOrientation_LandscapeSecondary) {
    // remap X,Y -> -Y,X
    o[0] = in[1]; o[1] = -in[0]; o[2] = in[2];
    o[4] = in[5]; o[5] = -in[4]; o[6] = in[6];
    o[8] = in[9]; o[9] = -in[8]; o[10] = in[10];
  } else if (screenConfig == eScreenOrientation_PortraitPrimary) {
    out = aMatrix;
  } else if (screenConfig == eScreenOrientation_PortraitSecondary) {
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

HMDInfoCardboard::HMDInfoCardboard()
  : VRHMDInfo(VRHMDType::Cardboard)
  , mStartCount(0)
  , mOrient(eScreenOrientation_PortraitPrimary)
{
  MOZ_ASSERT(sizeof(HMDInfoCardboard::DistortionVertex) == sizeof(VRDistortionVertex),
             "HMDInfoCardboard::DistortionVertex must match the size of VRDistortionVertex");

  MOZ_COUNT_CTOR_INHERITED(HMDInfoCardboard, VRHMDInfo);

  mDeviceName.AssignLiteral("Phone Sensor (Cardboard) HMD");

  mSupportedSensorBits = State_Orientation;

  mRecommendedEyeFOV[Eye_Left] = VRFieldOfView(45.0, 45.0, 45.0, 45.0);
  mRecommendedEyeFOV[Eye_Right] = VRFieldOfView(45.0, 45.0, 45.0, 45.0);

  mMaximumEyeFOV[Eye_Left] = VRFieldOfView(45.0, 45.0, 45.0, 45.0);
  mMaximumEyeFOV[Eye_Right] = VRFieldOfView(45.0, 45.0, 45.0, 45.0);

  SetFOV(mRecommendedEyeFOV[Eye_Left], mRecommendedEyeFOV[Eye_Right], 0.01, 10000.0);

#if 1
  int32_t xcoord = 0;
  if (PR_GetEnv("FAKE_CARDBOARD_SCREEN")) {
      const char *env = PR_GetEnv("FAKE_CARDBOARD_SCREEN");
      nsresult err;
      xcoord = nsCString(env).ToInteger(&err);
      if (err != NS_OK) xcoord = 0;
  }
  mScreen = VRHMDManager::MakeFakeScreen(xcoord, 0, 1920, 1080);
#endif

}

bool
HMDInfoCardboard::StartSensorTracking()
{
  LOG("HMDInfoCardboard::StartSensorTracking %d\n", mStartCount);
  if (mStartCount == 0) {
    // it's never been started before; initialize observers and
    // initial state.

    mozilla::hal::ScreenConfiguration sconfig;
    mozilla::hal::GetCurrentScreenConfiguration(&sconfig);
    this->Notify(sconfig);

    mozilla::hal::RegisterSensorObserver(mozilla::hal::SENSOR_GAME_ROTATION_VECTOR, this);
    mozilla::hal::RegisterScreenConfigurationObserver(this);

    mLastSensorState.Clear();
  }

  mStartCount++;
  return true;
}

// Android sends us events that have a 90-degree rotation about 
// the x axis compared to what we want (phone flat vs. phone held in front of the eyes).
// Correct for this by applying a transform to undo this rotation.
void
HMDInfoCardboard::Notify(const mozilla::hal::ScreenConfiguration& config)
{
  mOrient = config.orientation();

  if (mOrient == eScreenOrientation_LandscapePrimary) {
    mScreenTransform = Quaternion(-0.5f, 0.5f, 0.5f, 0.5f);
  } else if (mOrient == eScreenOrientation_LandscapeSecondary) {
    mScreenTransform = Quaternion(-0.5f, -0.5f, -0.5f, 0.5f);
  } else if (mOrient == eScreenOrientation_PortraitPrimary) {
    mScreenTransform = Quaternion((float) -M_SQRT1_2, 0.f, 0.f, (float) M_SQRT1_2);
  } else if (mOrient == eScreenOrientation_PortraitSecondary) {
    // Currently, PortraitSecondary event doesn't be triggered.
    mScreenTransform = Quaternion((float) M_SQRT1_2, 0.f, 0.f, (float) M_SQRT1_2);
  }
}

void
HMDInfoCardboard::Notify(const mozilla::hal::SensorData& data)
{
  if (data.sensor() != mozilla::hal::SENSOR_GAME_ROTATION_VECTOR)
    return;

  const nsTArray<float>& sensorValues = data.values();

  // This is super chatty
  //LOG("HMDInfoCardboard::Notify %f %f %f %f\n", sensorValues[0], sensorValues[1], sensorValues[2], sensorValues[3]);

  mSavedLastSensor.Set(sensorValues[0], sensorValues[1], sensorValues[2], sensorValues[3]);
  mSavedLastSensorTime = data.timestamp();
  mNeedsSensorCompute = true;
}

void
HMDInfoCardboard::ComputeStateFromLastSensor()
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

  VRHMDSensorState& state = mLastSensorState;

  state.flags |= State_Orientation;
  state.orientation[0] = q.x;
  state.orientation[1] = q.y;
  state.orientation[2] = q.z;
  state.orientation[3] = q.w;

  state.timestamp = mSavedLastSensorTime / 1000000.0;

  mNeedsSensorCompute = false;
}

VRHMDSensorState
HMDInfoCardboard::GetSensorState(double timeOffset)
{
  ComputeStateFromLastSensor();
  return mLastSensorState;
}

void
HMDInfoCardboard::StopSensorTracking()
{
  LOG("HMDInfoCardboard::StopSensorTracking, count %d\n", mStartCount);
  if (--mStartCount == 0) {
    mozilla::hal::UnregisterScreenConfigurationObserver(this);
    mozilla::hal::UnregisterSensorObserver(mozilla::hal::SENSOR_GAME_ROTATION_VECTOR, this);
  }
}

void
HMDInfoCardboard::ZeroSensor()
{
  mSensorZeroInverse = mSavedLastSensor;
  mSensorZeroInverse.Invert();
}

bool
HMDInfoCardboard::SetFOV(const VRFieldOfView& aFOVLeft,
                         const VRFieldOfView& aFOVRight,
                         double zNear, double zFar)
{
  const float standardIPD = 0.064f;

  for (uint32_t eye = 0; eye < NumEyes; eye++) {
    mEyeFOV[eye] = eye == Eye_Left ? aFOVLeft : aFOVRight;
    mEyeTranslation[eye] = Point3D(standardIPD * (eye == Eye_Left ? -1.0 : 1.0), 0.0, 0.0);
    mEyeProjectionMatrix[eye] = mEyeFOV[eye].ConstructProjectionMatrix(zNear, zFar, true);

    mDistortionMesh[eye].mVertices.SetLength(4);
    mDistortionMesh[eye].mIndices.SetLength(6);

    HMDInfoCardboard::DistortionVertex *destv = reinterpret_cast<HMDInfoCardboard::DistortionVertex*>(mDistortionMesh[eye].mVertices.Elements());
    float xoffs = eye == Eye_Left ? 0.0f : 1.0f;
    float txoffs = eye == Eye_Left ? 0.0f : 0.5f;
    destv[0].pos[0] = -1.0 + xoffs;
    destv[0].pos[1] = -1.0;
    destv[0].texR[0] = destv[0].texG[0] = destv[0].texB[0] = 0.0 + txoffs;
    destv[0].texR[1] = destv[0].texG[1] = destv[0].texB[1] = 1.0;
    destv[0].padding[0] = 1.0; // vignette factor

    destv[1].pos[0] = 0.0 + xoffs;
    destv[1].pos[1] = -1.0;
    destv[1].texR[0] = destv[1].texG[0] = destv[1].texB[0] = 0.5 + txoffs;
    destv[1].texR[1] = destv[1].texG[1] = destv[1].texB[1] = 1.0;
    destv[1].padding[0] = 1.0; // vignette factor

    destv[2].pos[0] = 0.0 + xoffs;
    destv[2].pos[1] = 1.0;
    destv[2].texR[0] = destv[2].texG[0] = destv[2].texB[0] = 0.5 + txoffs;
    destv[2].texR[1] = destv[2].texG[1] = destv[2].texB[1] = 0.0;
    destv[2].padding[0] = 1.0; // vignette factor

    destv[3].pos[0] = -1.0 + xoffs;
    destv[3].pos[1] = 1.0;
    destv[3].texR[0] = destv[3].texG[0] = destv[3].texB[0] = 0.0 + txoffs;
    destv[3].texR[1] = destv[3].texG[1] = destv[3].texB[1] = 0.0;
    destv[3].padding[0] = 1.0; // vignette factor

    uint16_t *iv = mDistortionMesh[eye].mIndices.Elements();
    iv[0] = 0; iv[1] = 1; iv[2] = 2;
    iv[3] = 2; iv[4] = 3; iv[5] = 0;
  }

  // XXX find out the default screen size and use that
  mEyeResolution.width = 1920 / 2;
  mEyeResolution.height = 1080;

  if (PR_GetEnv("FAKE_CARDBOARD_SCREEN")) {
    // for testing, make the eye resolution 2x of the screen
    mEyeResolution.width *= 2;
    mEyeResolution.height *= 2;
  }

  mConfiguration.hmdType = mType;
  mConfiguration.value = 0;
  mConfiguration.fov[0] = aFOVLeft;
  mConfiguration.fov[1] = aFOVRight;

  return true;
}

void
HMDInfoCardboard::FillDistortionConstants(uint32_t whichEye,
                                          const IntSize& textureSize, const IntRect& eyeViewport,
                                          const Size& destViewport, const Rect& destRect,
                                          VRDistortionConstants& values)
{
  // these modify the texture coordinates; texcoord * zw + xy
  values.eyeToSourceScaleAndOffset[0] = 0.0;
  values.eyeToSourceScaleAndOffset[1] = 0.0;
  if (PR_GetEnv("FAKE_CARDBOARD_SCREEN")) {
    values.eyeToSourceScaleAndOffset[2] = 2.0;
    values.eyeToSourceScaleAndOffset[3] = 2.0;
  } else {
    values.eyeToSourceScaleAndOffset[2] = 1.0;
    values.eyeToSourceScaleAndOffset[3] = 1.0;
  }

  // Our mesh positions are in the [-1..1] clip space; we give appropriate offset
  // and scaling for the right viewport.  (In the 0..2 space for sanity)

  // this is the destRect in clip space
  float x0 = destRect.x / destViewport.width * 2.0 - 1.0;
  float x1 = (destRect.x + destRect.width) / destViewport.width * 2.0 - 1.0;

  float y0 = destRect.y / destViewport.height * 2.0 - 1.0;
  float y1 = (destRect.y + destRect.height) / destViewport.height * 2.0 - 1.0;

  // offset
  values.destinationScaleAndOffset[0] = (x0+x1) / 2.0;
  values.destinationScaleAndOffset[1] = (y0+y1) / 2.0;
  // scale
  values.destinationScaleAndOffset[2] = destRect.width / destViewport.width;
  values.destinationScaleAndOffset[3] = destRect.height / destViewport.height;
}

void
HMDInfoCardboard::Destroy()
{
}



bool
VRHMDManagerCardboard::PlatformInit()
{
  return gfxPrefs::VREnabled() && gfxPrefs::VRCardboardEnabled();
}

bool
VRHMDManagerCardboard::Init()
{
  if (mCardboardInitialized)
    return true;

  RefPtr<HMDInfoCardboard> hmd = new HMDInfoCardboard();
  mCardboardHMDs.AppendElement(hmd);

  mCardboardInitialized = true;
  return true;
}

void
VRHMDManagerCardboard::Destroy()
{
  if (!mCardboardInitialized)
    return;

  for (size_t i = 0; i < mCardboardHMDs.Length(); ++i) {
    mCardboardHMDs[i]->Destroy();
  }

  mCardboardHMDs.Clear();
  mCardboardInitialized = false;
}

void
VRHMDManagerCardboard::GetHMDs(nsTArray<RefPtr<VRHMDInfo>>& aHMDResult)
{
  Init();
  for (size_t i = 0; i < mCardboardHMDs.Length(); ++i) {
    aHMDResult.AppendElement(mCardboardHMDs[i]);
  }
}
