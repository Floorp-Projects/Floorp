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

VRDisplayHost::VRDisplayHost(VRDisplayType aType)
  : mInputFrameID(0)
{
  MOZ_COUNT_CTOR(VRDisplayHost);
  mDisplayInfo.mType = aType;
  mDisplayInfo.mDisplayID = VRDisplayManager::AllocateDisplayID();
  mDisplayInfo.mIsPresenting = false;

  for (int i = 0; i < kMaxLatencyFrames; i++) {
    mLastSensorState[i].Clear();
  }
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
}

void
VRDisplayHost::RemoveLayer(VRLayerParent *aLayer)
{
  mLayers.RemoveElement(aLayer);
  if (mLayers.Length() == 0) {
    StopPresentation();
  }
  mDisplayInfo.mIsPresenting = mLayers.Length() > 0;
}

#if defined(XP_WIN)

void
VRDisplayHost::SubmitFrame(VRLayerParent* aLayer, const int32_t& aInputFrameID,
  PTextureParent* aTexture, const gfx::Rect& aLeftEyeRect,
  const gfx::Rect& aRightEyeRect)
{
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
