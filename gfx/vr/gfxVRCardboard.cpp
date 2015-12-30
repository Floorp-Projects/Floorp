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
#include "mozilla/Preferences.h"
#include "mozilla/Hal.h"

#include "gfxVRCardboard.h"

#include "nsServiceManagerUtils.h"
#include "nsIScreenManager.h"

using namespace mozilla::dom;
using namespace mozilla::gfx;
using namespace mozilla::gfx::impl;

HMDInfoCardboard::HMDInfoCardboard()
  : VRHMDInfo(VRHMDType::Cardboard, true)
{
  MOZ_ASSERT(sizeof(HMDInfoCardboard::DistortionVertex) == sizeof(VRDistortionVertex),
             "HMDInfoCardboard::DistortionVertex must match the size of VRDistortionVertex");

  MOZ_COUNT_CTOR_INHERITED(HMDInfoCardboard, VRHMDInfo);

  mDeviceInfo.mDeviceName.AssignLiteral("Phone Sensor (Cardboard) HMD");

  mDeviceInfo.mSupportedSensorBits = VRStateValidFlags::State_Orientation;

  mDeviceInfo.mRecommendedEyeFOV[VRDeviceInfo::Eye_Left] = gfx::VRFieldOfView(45.0, 45.0, 45.0, 45.0);
  mDeviceInfo.mRecommendedEyeFOV[VRDeviceInfo::Eye_Right] = gfx::VRFieldOfView(45.0, 45.0, 45.0, 45.0);

  mDeviceInfo.mMaximumEyeFOV[VRDeviceInfo::Eye_Left] = gfx::VRFieldOfView(45.0, 45.0, 45.0, 45.0);
  mDeviceInfo.mMaximumEyeFOV[VRDeviceInfo::Eye_Right] = gfx::VRFieldOfView(45.0, 45.0, 45.0, 45.0);

  SetFOV(mDeviceInfo.mRecommendedEyeFOV[VRDeviceInfo::Eye_Left], mDeviceInfo.mRecommendedEyeFOV[VRDeviceInfo::Eye_Right], 0.01, 10000.0);

  mDeviceInfo.mScreenRect.x = 0;
  mDeviceInfo.mScreenRect.y = 0;
  mDeviceInfo.mScreenRect.width = 1920;
  mDeviceInfo.mScreenRect.height = 1080;
  mDeviceInfo.mIsFakeScreen = true;
}


VRHMDSensorState
HMDInfoCardboard::GetSensorState(double timeOffset)
{
  // Actual sensor state is calculated on the main thread,
  // within VRDeviceProxyOrientationFallBack
  VRHMDSensorState result;
  result.Clear();
  return result;
}

void
HMDInfoCardboard::ZeroSensor()
{
  MOZ_ASSERT(0, "HMDInfoCardboard::ZeroSensor not implemented.  "
                "Should use VRDeviceProxyOrientationFallBack on main thread");
}


void
HMDInfoCardboard::NotifyVsync(const TimeStamp& aVsyncTimestamp)
{
  // Nothing to do here for Cardboard VR
}

bool
HMDInfoCardboard::KeepSensorTracking()
{
  // Nothing to do here for Cardboard VR
  return true;
}

bool
HMDInfoCardboard::SetFOV(const gfx::VRFieldOfView& aFOVLeft,
                         const gfx::VRFieldOfView& aFOVRight,
                         double zNear, double zFar)
{
  const float standardIPD = 0.064f;

  for (uint32_t eye = 0; eye < VRDeviceInfo::NumEyes; eye++) {
    mDeviceInfo.mEyeFOV[eye] = eye == VRDeviceInfo::Eye_Left ? aFOVLeft : aFOVRight;
    mDeviceInfo.mEyeTranslation[eye] = Point3D(standardIPD * (eye == VRDeviceInfo::Eye_Left ? -1.0 : 1.0), 0.0, 0.0);
    mDeviceInfo.mEyeProjectionMatrix[eye] = mDeviceInfo.mEyeFOV[eye].ConstructProjectionMatrix(zNear, zFar, true);

    mDistortionMesh[eye].mVertices.SetLength(4);
    mDistortionMesh[eye].mIndices.SetLength(6);

    HMDInfoCardboard::DistortionVertex *destv = reinterpret_cast<HMDInfoCardboard::DistortionVertex*>(mDistortionMesh[eye].mVertices.Elements());
    float xoffs = eye == VRDeviceInfo::Eye_Left ? 0.0f : 1.0f;
    float txoffs = eye == VRDeviceInfo::Eye_Left ? 0.0f : 0.5f;
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
  mDeviceInfo.mEyeResolution.width = 1920 / 2;
  mDeviceInfo.mEyeResolution.height = 1080;

  if (PR_GetEnv("FAKE_CARDBOARD_SCREEN")) {
    // for testing, make the eye resolution 2x of the screen
    mDeviceInfo.mEyeResolution.width *= 2;
    mDeviceInfo.mEyeResolution.height *= 2;
  }

  mConfiguration.hmdType = mDeviceInfo.mType;
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

/*static*/ already_AddRefed<VRHMDManagerCardboard>
VRHMDManagerCardboard::Create()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!gfxPrefs::VREnabled() || !gfxPrefs::VRCardboardEnabled())
  {
    return nullptr;
  }

  RefPtr<VRHMDManagerCardboard> manager = new VRHMDManagerCardboard();
  return manager.forget();
}

bool
VRHMDManagerCardboard::Init()
{
  if (mCardboardInitialized) {
    return true;
  }

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
  if (!mCardboardInitialized) {
    return;
  }

  for (size_t i = 0; i < mCardboardHMDs.Length(); ++i) {
    aHMDResult.AppendElement(mCardboardHMDs[i]);
  }
}
