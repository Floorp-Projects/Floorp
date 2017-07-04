/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VRDisplayHost.h"
#include "gfxVR.h"
#include "ipc/VRLayerParent.h"
#include "mozilla/layers/TextureHost.h"
#include "mozilla/dom/GamepadBinding.h" // For GamepadMappingType

#if defined(XP_WIN)

#include <d3d11.h>
#include "gfxWindowsPlatform.h"
#include "../layers/d3d11/CompositorD3D11.h"
#include "mozilla/layers/TextureD3D11.h"

#elif defined(XP_MACOSX)

#include "mozilla/gfx/MacIOSurface.h"

#endif

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::layers;

VRDisplayHost::VRDisplayHost(VRDeviceType aType)
 : mFrameStarted(false)
{
  MOZ_COUNT_CTOR(VRDisplayHost);
  mDisplayInfo.mType = aType;
  mDisplayInfo.mDisplayID = VRSystemManager::AllocateDisplayID();
  mDisplayInfo.mPresentingGroups = 0;
  mDisplayInfo.mGroupMask = kVRGroupContent;
  mDisplayInfo.mFrameId = 0;
}

VRDisplayHost::~VRDisplayHost()
{
  MOZ_COUNT_DTOR(VRDisplayHost);
}

void
VRDisplayHost::SetGroupMask(uint32_t aGroupMask)
{
  mDisplayInfo.mGroupMask = aGroupMask;
}

bool
VRDisplayHost::GetIsConnected()
{
  return mDisplayInfo.mIsConnected;
}

void
VRDisplayHost::AddLayer(VRLayerParent *aLayer)
{
  mLayers.AppendElement(aLayer);
  mDisplayInfo.mPresentingGroups |= aLayer->GetGroup();
  if (mLayers.Length() == 1) {
    StartPresentation();
  }

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
  mDisplayInfo.mPresentingGroups = 0;
  for (auto layer : mLayers) {
    mDisplayInfo.mPresentingGroups |= layer->GetGroup();
  }

  // Ensure that the content process receives the change immediately
  VRManager* vm = VRManager::Get();
  vm->RefreshVRDisplays();
}

void
VRDisplayHost::StartFrame()
{
  mLastFrameStart = TimeStamp::Now();
  ++mDisplayInfo.mFrameId;
  mDisplayInfo.mLastSensorState[mDisplayInfo.mFrameId % kVRMaxLatencyFrames] = GetSensorState();
  mFrameStarted = true;
}

void
VRDisplayHost::NotifyVSync()
{
  /**
   * We will trigger a new frame immediately after a successful frame texture
   * submission.  If content fails to call VRDisplay.submitFrame after
   * kVRDisplayRAFMaxDuration milliseconds has elapsed since the last
   * VRDisplay.requestAnimationFrame, we act as a "watchdog" and kick-off
   * a new VRDisplay.requestAnimationFrame to avoid a render loop stall and
   * to give content a chance to recover.
   *
   * If the lower level VR platform API's are rejecting submitted frames,
   * such as when the Oculus "Health and Safety Warning" is displayed,
   * we will not kick off the next frame immediately after VRDisplay.submitFrame
   * as it would result in an unthrottled render loop that would free run at
   * potentially extreme frame rates.  To ensure that content has a chance to
   * resume its presentation when the frames are accepted once again, we rely
   * on this "watchdog" to act as a VR refresh driver cycling at a rate defined
   * by kVRDisplayRAFMaxDuration.
   *
   * kVRDisplayRAFMaxDuration is the number of milliseconds since last frame
   * start before triggering a new frame.  When content is failing to submit
   * frames on time or the lower level VR platform API's are rejecting frames,
   * kVRDisplayRAFMaxDuration determines the rate at which RAF callbacks
   * will be called.
   *
   * This number must be larger than the slowest expected frame time during
   * normal VR presentation, but small enough not to break content that
   * makes assumptions of reasonably minimal VSync rate.
   *
   * The slowest expected refresh rate for a VR display currently is an
   * Oculus CV1 when ASW (Asynchronous Space Warp) is enabled, at 45hz.
   * A kVRDisplayRAFMaxDuration value of 50 milliseconds results in a 20hz
   * rate, which avoids inadvertent triggering of the watchdog during
   * Oculus ASW even if every second frame is dropped.
   */
  const double kVRDisplayRAFMaxDuration = 50;

  bool bShouldStartFrame = false;

  if (mDisplayInfo.mPresentingGroups == 0) {
    // If this display isn't presenting, refresh the sensors and trigger
    // VRDisplay.requestAnimationFrame at the normal 2d display refresh rate.
    bShouldStartFrame = true;
  } else {
    // If content fails to call VRDisplay.submitFrame, we must eventually
    // time-out and trigger a new frame.
    if (mLastFrameStart.IsNull()) {
      bShouldStartFrame = true;
    } else {
      TimeDuration duration = TimeStamp::Now() - mLastFrameStart;
      if (duration.ToMilliseconds() > kVRDisplayRAFMaxDuration) {
        bShouldStartFrame = true;
      }
    }
  }

  if (bShouldStartFrame) {
    VRManager *vm = VRManager::Get();
    MOZ_ASSERT(vm);
    vm->NotifyVRVsync(mDisplayInfo.mDisplayID);
  }
}

void
VRDisplayHost::SubmitFrame(VRLayerParent* aLayer, PTextureParent* aTexture,
                           const gfx::Rect& aLeftEyeRect,
                           const gfx::Rect& aRightEyeRect)
{
  if ((mDisplayInfo.mGroupMask & aLayer->GetGroup()) == 0) {
    // Suppress layers hidden by the group mask
    return;
  }

  // Ensure that we only accept the first SubmitFrame call per RAF cycle.
  if (!mFrameStarted) {
    return;
  }
  mFrameStarted = false;

#if defined(XP_WIN)

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
    NS_WARNING("VRDisplayHost::SubmitFrame failed to get a TextureSourceD3D11");
    return;
  }

  if (!SubmitFrame(sourceD3D11, texSize, aLeftEyeRect, aRightEyeRect)) {
    return;
  }

#elif defined(XP_MACOSX)

  TextureHost* th = TextureHost::AsTextureHost(aTexture);

  MacIOSurface* surf = th->GetMacIOSurface();
  if (!surf) {
    NS_WARNING("VRDisplayHost::SubmitFrame failed to get a MacIOSurface");
    return;
  }

  IntSize texSize = gfx::IntSize(surf->GetDevicePixelWidth(),
                                 surf->GetDevicePixelHeight());

  if (!SubmitFrame(surf, texSize, aLeftEyeRect, aRightEyeRect)) {
    return;
  }

#else

  NS_WARNING("WebVR is not supported on this platform.");
  return;
#endif

#if defined(XP_WIN) || defined(XP_MACOSX)

  /**
   * Trigger the next VSync immediately after we are successfully
   * submitting frames.  As SubmitFrame is responsible for throttling
   * the render loop, if we don't successfully call it, we shouldn't trigger
   * NotifyVRVsync immediately, as it will run unbounded.
   * If NotifyVRVsync is not called here due to SubmitFrame failing, the
   * fallback "watchdog" code in VRDisplayHost::NotifyVSync() will cause
   * frames to continue at a lower refresh rate until frame submission
   * succeeds again.
   */
  VRManager *vm = VRManager::Get();
  MOZ_ASSERT(vm);
  vm->NotifyVRVsync(mDisplayInfo.mDisplayID);
#endif
}

bool
VRDisplayHost::CheckClearDisplayInfoDirty()
{
  if (mDisplayInfo == mLastUpdateDisplayInfo) {
    return false;
  }
  mLastUpdateDisplayInfo = mDisplayInfo;
  return true;
}

VRControllerHost::VRControllerHost(VRDeviceType aType, dom::GamepadHand aHand,
                                   uint32_t aDisplayID)
 : mVibrateIndex(0)
{
  MOZ_COUNT_CTOR(VRControllerHost);
  mControllerInfo.mType = aType;
  mControllerInfo.mHand = aHand;
  mControllerInfo.mMappingType = dom::GamepadMappingType::_empty;
  mControllerInfo.mDisplayID = aDisplayID;
  mControllerInfo.mControllerID = VRSystemManager::AllocateControllerID();
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
