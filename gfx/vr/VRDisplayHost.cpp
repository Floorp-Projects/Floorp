/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VRDisplayHost.h"
#include "gfxVR.h"

#if defined(XP_WIN)

#include <d3d11.h>
#include "gfxWindowsPlatform.h"
#include "../layers/d3d11/CompositorD3D11.h"
#include "mozilla/layers/TextureD3D11.h"

#endif

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::layers;

VRDisplayHost::VRDisplayHost(VRDeviceType aType)
  : mInputFrameID(0)
{
  MOZ_COUNT_CTOR(VRDisplayHost);
  mDisplayInfo.mType = aType;
  mDisplayInfo.mDisplayID = VRSystemManager::AllocateDisplayID();
  mDisplayInfo.mIsPresenting = false;
}

VRDisplayHost::~VRDisplayHost()
{
  MOZ_COUNT_DTOR(VRDisplayHost);
}

void
VRDisplayHost::AddLayer(VRLayerParent *aLayer)
{
  mLayers.AppendElement(aLayer);
  if (mLayers.Length() == 1) {
    StartPresentation();
  }
  mDisplayInfo.mIsPresenting = mLayers.Length() > 0;

  // Ensure that the content process receives the change immediately
  VRManager* vm = VRManager::Get();
  vm->RefreshVRDisplays();
}

void
VRDisplayHost::RemoveLayer(VRLayerParent *aLayer)
{
  mLayers.RemoveElement(aLayer);
  if (mLayers.Length() == 0) {
    StopPresentation();
  }
  mDisplayInfo.mIsPresenting = mLayers.Length() > 0;

  // Ensure that the content process receives the change immediately
  VRManager* vm = VRManager::Get();
  vm->RefreshVRDisplays();
}

#if defined(XP_WIN)

void
VRDisplayHost::SubmitFrame(VRLayerParent* aLayer, const int32_t& aInputFrameID,
  PTextureParent* aTexture, const gfx::Rect& aLeftEyeRect,
  const gfx::Rect& aRightEyeRect)
{
  // aInputFrameID is no longer controlled by content with the WebVR 1.1 API
  // update; however, we will later use this code to enable asynchronous
  // submission of multiple layers to be composited.  This will enable
  // us to build browser UX that remains responsive even when content does
  // not consistently submit frames.

  int32_t inputFrameID = aInputFrameID;
  if (inputFrameID == 0) {
    inputFrameID = mInputFrameID;
  }
  if (inputFrameID < 0) {
    // Sanity check to prevent invalid memory access on builds with assertions
    // disabled.
    inputFrameID = 0;
  }

  VRHMDSensorState sensorState = mLastSensorState[inputFrameID % kMaxLatencyFrames];
  // It is possible to get a cache miss on mLastSensorState if latency is
  // longer than kMaxLatencyFrames.  An optimization would be to find a frame
  // that is closer than the one selected with the modulus.
  // If we hit this; however, latency is already so high that the site is
  // un-viewable and a more accurate pose prediction is not likely to
  // compensate.

  TextureHost* th = TextureHost::AsTextureHost(aTexture);
  // WebVR doesn't use the compositor to compose the frame, so use
  // AutoLockTextureHostWithoutCompositor here.
  AutoLockTextureHostWithoutCompositor autoLock(th);
  if (autoLock.Failed()) {
    NS_WARNING("Failed to lock the VR layer texture");
    return;
  }

  CompositableTextureSourceRef source;
  if (!th->BindTextureSource(source)) {
    NS_WARNING("The TextureHost was successfully locked but can't provide a TextureSource");
    return;
  }
  MOZ_ASSERT(source);

  IntSize texSize = source->GetSize();

  TextureSourceD3D11* sourceD3D11 = source->AsSourceD3D11();
  if (!sourceD3D11) {
    NS_WARNING("WebVR support currently only implemented for D3D11");
    return;
  }

  SubmitFrame(sourceD3D11, texSize, sensorState, aLeftEyeRect, aRightEyeRect);
}

#else

void
VRDisplayHost::SubmitFrame(VRLayerParent* aLayer, const int32_t& aInputFrameID,
  PTextureParent* aTexture, const gfx::Rect& aLeftEyeRect,
  const gfx::Rect& aRightEyeRect)
{
  NS_WARNING("WebVR only supported in Windows.");
}

#endif

bool
VRDisplayHost::CheckClearDisplayInfoDirty()
{
  if (mDisplayInfo == mLastUpdateDisplayInfo) {
    return false;
  }
  mLastUpdateDisplayInfo = mDisplayInfo;
  return true;
}

VRControllerHost::VRControllerHost(VRDeviceType aType)
 : mVibrateIndex(0)
{
  MOZ_COUNT_CTOR(VRControllerHost);
  mControllerInfo.mType = aType;
  mControllerInfo.mControllerID = VRSystemManager::AllocateDisplayID();
}

VRControllerHost::~VRControllerHost()
{
  MOZ_COUNT_DTOR(VRControllerHost);
}

const VRControllerInfo&
VRControllerHost::GetControllerInfo() const
{
  return mControllerInfo;
}

void
VRControllerHost::SetButtonPressed(uint64_t aBit)
{
  mButtonPressed = aBit;
}

uint64_t
VRControllerHost::GetButtonPressed()
{
  return mButtonPressed;
}

void
VRControllerHost::SetButtonTouched(uint64_t aBit)
{
  mButtonTouched = aBit;
}

uint64_t
VRControllerHost::GetButtonTouched()
{
  return mButtonTouched;
}

void
VRControllerHost::SetPose(const dom::GamepadPoseState& aPose)
{
  mPose = aPose;
}

const dom::GamepadPoseState&
VRControllerHost::GetPose()
{
  return mPose;
}

dom::GamepadHand
VRControllerHost::GetHand()
{
  return mControllerInfo.mHand;
}

void
VRControllerHost::SetVibrateIndex(uint64_t aIndex)
{
  mVibrateIndex = aIndex;
}

uint64_t
VRControllerHost::GetVibrateIndex()
{
  return mVibrateIndex;
}
