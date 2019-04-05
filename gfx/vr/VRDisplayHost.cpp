/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VRDisplayHost.h"
#include "gfxPrefs.h"
#include "gfxVR.h"
#include "ipc/VRLayerParent.h"
#include "mozilla/layers/CompositorThread.h"  // for CompositorThreadHolder
#include "mozilla/layers/TextureHost.h"
#include "mozilla/dom/GamepadBinding.h"  // For GamepadMappingType
#include "VRThread.h"

#if defined(XP_WIN)

#  include <d3d11.h>
#  include "gfxWindowsPlatform.h"
#  include "../layers/d3d11/CompositorD3D11.h"
#  include "mozilla/gfx/DeviceManagerDx.h"
#  include "mozilla/layers/TextureD3D11.h"

#elif defined(XP_MACOSX)

#  include "mozilla/gfx/MacIOSurface.h"

#endif

#if defined(MOZ_WIDGET_ANDROID)
#  include "mozilla/layers/CompositorThread.h"
// Max frame duration on Android before the watchdog submits a new one.
// Probably we can get rid of this when we enforce that SubmitFrame can only be
// called in a VRDisplay loop.
#  define ANDROID_MAX_FRAME_DURATION 4000
#endif  // defined(MOZ_WIDGET_ANDROID)

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::layers;

VRDisplayHost::AutoRestoreRenderState::AutoRestoreRenderState(
    VRDisplayHost* aDisplay)
    : mDisplay(aDisplay), mSuccess(true) {
#if defined(XP_WIN)
  ID3D11DeviceContext1* context = mDisplay->GetD3DDeviceContext();
  ID3DDeviceContextState* state = mDisplay->GetD3DDeviceContextState();
  if (!context || !state) {
    mSuccess = false;
    return;
  }
  context->SwapDeviceContextState(state,
                                  getter_AddRefs(mPrevDeviceContextState));
#endif
}

VRDisplayHost::AutoRestoreRenderState::~AutoRestoreRenderState() {
#if defined(XP_WIN)
  ID3D11DeviceContext1* context = mDisplay->GetD3DDeviceContext();
  if (context && mSuccess) {
    context->SwapDeviceContextState(mPrevDeviceContextState, nullptr);
  }
#endif
}

bool VRDisplayHost::AutoRestoreRenderState::IsSuccess() { return mSuccess; }

VRDisplayHost::VRDisplayHost(VRDeviceType aType)
    : mDisplayInfo{},
      mLastUpdateDisplayInfo{},
      mCurrentSubmitTaskMonitor("CurrentSubmitTaskMonitor"),
      mCurrentSubmitTask(nullptr),
      mFrameStarted(false) {
  MOZ_COUNT_CTOR(VRDisplayHost);
  mDisplayInfo.mType = aType;
  mDisplayInfo.mDisplayID = VRSystemManager::AllocateDisplayID();
  mDisplayInfo.mPresentingGroups = 0;
  mDisplayInfo.mGroupMask = kVRGroupContent;
  mDisplayInfo.mFrameId = 0;
  mDisplayInfo.mDisplayState.presentingGeneration = 0;
  mDisplayInfo.mDisplayState.displayName[0] = '\0';

#if defined(MOZ_WIDGET_ANDROID)
  mLastSubmittedFrameId = 0;
  mLastStartedFrame = 0;
#endif  // defined(MOZ_WIDGET_ANDROID)
}

VRDisplayHost::~VRDisplayHost() {
  CancelCurrentSubmitTask();

  if (mSubmitThread) {
    mSubmitThread->Shutdown();
    mSubmitThread = nullptr;
  }
  MOZ_COUNT_DTOR(VRDisplayHost);
}

void VRDisplayHost::ShutdownSubmitThread() {
  if (mSubmitThread) {
    mSubmitThread->Shutdown();
    mSubmitThread = nullptr;
  }
}

#if defined(XP_WIN)
bool VRDisplayHost::CreateD3DObjects() {
  if (!mDevice) {
    RefPtr<ID3D11Device> device = gfx::DeviceManagerDx::Get()->GetVRDevice();
    if (!device) {
      NS_WARNING("VRDisplayHost::CreateD3DObjects failed to get a D3D11Device");
      return false;
    }
    if (FAILED(device->QueryInterface(__uuidof(ID3D11Device1),
                                      getter_AddRefs(mDevice)))) {
      NS_WARNING(
          "VRDisplayHost::CreateD3DObjects failed to get a D3D11Device1");
      return false;
    }
  }
  if (!mContext) {
    mDevice->GetImmediateContext1(getter_AddRefs(mContext));
    if (!mContext) {
      NS_WARNING(
          "VRDisplayHost::CreateD3DObjects failed to get an immediate context");
      return false;
    }
  }
  if (!mDeviceContextState) {
    D3D_FEATURE_LEVEL featureLevels[]{D3D_FEATURE_LEVEL_11_1,
                                      D3D_FEATURE_LEVEL_11_0};
    mDevice->CreateDeviceContextState(0, featureLevels, 2, D3D11_SDK_VERSION,
                                      __uuidof(ID3D11Device1), nullptr,
                                      getter_AddRefs(mDeviceContextState));
  }
  if (!mDeviceContextState) {
    NS_WARNING(
        "VRDisplayHost::CreateD3DObjects failed to get a "
        "D3D11DeviceContextState");
    return false;
  }
  return true;
}

ID3D11Device1* VRDisplayHost::GetD3DDevice() { return mDevice; }

ID3D11DeviceContext1* VRDisplayHost::GetD3DDeviceContext() { return mContext; }

ID3DDeviceContextState* VRDisplayHost::GetD3DDeviceContextState() {
  return mDeviceContextState;
}

#endif  // defined(XP_WIN)

void VRDisplayHost::SetGroupMask(uint32_t aGroupMask) {
  mDisplayInfo.mGroupMask = aGroupMask;
}

bool VRDisplayHost::GetIsConnected() {
  return mDisplayInfo.mDisplayState.isConnected;
}

void VRDisplayHost::AddLayer(VRLayerParent* aLayer) {
  mLayers.AppendElement(aLayer);
  mDisplayInfo.mPresentingGroups |= aLayer->GetGroup();
  if (mLayers.Length() == 1) {
    StartPresentation();
  }

  // Ensure that the content process receives the change immediately
  VRManager* vm = VRManager::Get();
  vm->RefreshVRDisplays();
}

void VRDisplayHost::RemoveLayer(VRLayerParent* aLayer) {
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

void VRDisplayHost::StartFrame() {
  AUTO_PROFILER_TRACING("VR", "GetSensorState", OTHER);

  TimeStamp now = TimeStamp::Now();
#if defined(MOZ_WIDGET_ANDROID)
  const TimeStamp lastFrameStart =
      mLastFrameStart[mDisplayInfo.mFrameId % kVRMaxLatencyFrames];
  const bool isPresenting = mLastUpdateDisplayInfo.GetPresentingGroups() != 0;
  double duration =
      lastFrameStart.IsNull() ? 0.0 : (now - lastFrameStart).ToMilliseconds();
  /**
   * Do not start more VR frames until the last submitted frame is already
   * processed.
   */
  if (isPresenting && mLastStartedFrame > 0 &&
      mDisplayInfo.mDisplayState.lastSubmittedFrameId < mLastStartedFrame &&
      duration < (double)ANDROID_MAX_FRAME_DURATION) {
    return;
  }
#endif  // !defined(MOZ_WIDGET_ANDROID)

  ++mDisplayInfo.mFrameId;
  size_t bufferIndex = mDisplayInfo.mFrameId % kVRMaxLatencyFrames;
  mDisplayInfo.mLastSensorState[bufferIndex] = GetSensorState();
  mLastFrameStart[bufferIndex] = now;
  mFrameStarted = true;
#if defined(MOZ_WIDGET_ANDROID)
  mLastStartedFrame = mDisplayInfo.mFrameId;
#endif  // !defined(MOZ_WIDGET_ANDROID)
}

void VRDisplayHost::NotifyVSync() {
  /**
   * If this display isn't presenting, refresh the sensors and trigger
   * VRDisplay.requestAnimationFrame at the normal 2d display refresh rate.
   */
  if (mDisplayInfo.mPresentingGroups == 0) {
    VRManager* vm = VRManager::Get();
    MOZ_ASSERT(vm);
    vm->NotifyVRVsync(mDisplayInfo.mDisplayID);
  }
}

void VRDisplayHost::CheckWatchDog() {
  /**
   * We will trigger a new frame immediately after a successful frame texture
   * submission.  If content fails to call VRDisplay.submitFrame after
   * dom.vr.display.rafMaxDuration milliseconds has elapsed since the last
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
   * by dom.vr.display.rafMaxDuration.
   *
   * This number must be larger than the slowest expected frame time during
   * normal VR presentation, but small enough not to break content that
   * makes assumptions of reasonably minimal VSync rate.
   *
   * The slowest expected refresh rate for a VR display currently is an
   * Oculus CV1 when ASW (Asynchronous Space Warp) is enabled, at 45hz.
   * A dom.vr.display.rafMaxDuration value of 50 milliseconds results in a 20hz
   * rate, which avoids inadvertent triggering of the watchdog during
   * Oculus ASW even if every second frame is dropped.
   */
  bool bShouldStartFrame = false;

  // If content fails to call VRDisplay.submitFrame, we must eventually
  // time-out and trigger a new frame.
  TimeStamp lastFrameStart =
      mLastFrameStart[mDisplayInfo.mFrameId % kVRMaxLatencyFrames];
  if (lastFrameStart.IsNull()) {
    bShouldStartFrame = true;
  } else {
    TimeDuration duration = TimeStamp::Now() - lastFrameStart;
    if (duration.ToMilliseconds() > gfxPrefs::VRDisplayRafMaxDuration()) {
      bShouldStartFrame = true;
    }
  }

  if (bShouldStartFrame) {
    VRManager* vm = VRManager::Get();
    MOZ_ASSERT(vm);
    vm->NotifyVRVsync(mDisplayInfo.mDisplayID);
  }
}

void VRDisplayHost::Run1msTasks(double aDeltaTime) {
  // To override in children
}

void VRDisplayHost::Run10msTasks() { CheckWatchDog(); }

void VRDisplayHost::Run100msTasks() {
  // to override in children
}

void VRDisplayHost::SubmitFrameInternal(
    const layers::SurfaceDescriptor& aTexture, uint64_t aFrameId,
    const gfx::Rect& aLeftEyeRect, const gfx::Rect& aRightEyeRect) {
#if !defined(MOZ_WIDGET_ANDROID)
  MOZ_ASSERT(mSubmitThread->GetThread() == NS_GetCurrentThread());
#endif  // !defined(MOZ_WIDGET_ANDROID)
  AUTO_PROFILER_TRACING("VR", "SubmitFrameAtVRDisplayHost", OTHER);

  {  // scope lock
    MonitorAutoLock lock(mCurrentSubmitTaskMonitor);

    if (!SubmitFrame(aTexture, aFrameId, aLeftEyeRect, aRightEyeRect)) {
      mCurrentSubmitTask = nullptr;
      return;
    }
    mCurrentSubmitTask = nullptr;
  }

#if defined(XP_WIN) || defined(XP_MACOSX) || defined(MOZ_WIDGET_ANDROID)

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
  VRManager* vm = VRManager::Get();
  MessageLoop* loop = CompositorThreadHolder::Loop();

  loop->PostTask(NewRunnableMethod<const uint32_t>(
      "gfx::VRManager::NotifyVRVsync", vm, &VRManager::NotifyVRVsync,
      mDisplayInfo.mDisplayID));
#endif
}

void VRDisplayHost::SubmitFrame(VRLayerParent* aLayer,
                                const layers::SurfaceDescriptor& aTexture,
                                uint64_t aFrameId,
                                const gfx::Rect& aLeftEyeRect,
                                const gfx::Rect& aRightEyeRect) {
  MonitorAutoLock lock(mCurrentSubmitTaskMonitor);
  if ((mDisplayInfo.mGroupMask & aLayer->GetGroup()) == 0) {
    // Suppress layers hidden by the group mask
    return;
  }

  // Ensure that we only accept the first SubmitFrame call per RAF cycle.
  if (!mFrameStarted || aFrameId != mDisplayInfo.mFrameId) {
    return;
  }

#if defined(MOZ_WIDGET_ANDROID)
  /**
   * Do not queue more submit frames until the last submitted frame is already
   * processed and the new WebGL texture is ready.
   */
  if (mLastSubmittedFrameId > 0 &&
      mLastSubmittedFrameId !=
          mDisplayInfo.mDisplayState.lastSubmittedFrameId) {
    mLastStartedFrame = 0;
    return;
  }

  mLastSubmittedFrameId = aFrameId;
#endif  // !defined(MOZ_WIDGET_ANDROID)

  mFrameStarted = false;

  RefPtr<CancelableRunnable> task = NewCancelableRunnableMethod<
      StoreCopyPassByConstLRef<layers::SurfaceDescriptor>, uint64_t,
      StoreCopyPassByConstLRef<gfx::Rect>, StoreCopyPassByConstLRef<gfx::Rect>>(
      "gfx::VRDisplayHost::SubmitFrameInternal", this,
      &VRDisplayHost::SubmitFrameInternal, aTexture, aFrameId, aLeftEyeRect,
      aRightEyeRect);

  if (!mCurrentSubmitTask) {
    mCurrentSubmitTask = task;
#if !defined(MOZ_WIDGET_ANDROID)
    if (!mSubmitThread) {
      mSubmitThread = new VRThread(NS_LITERAL_CSTRING("VR_SubmitFrame"));
    }
    mSubmitThread->Start();
    mSubmitThread->PostTask(task.forget());
#else
    CompositorThreadHolder::Loop()->PostTask(task.forget());
#endif  // defined(MOZ_WIDGET_ANDROID)
  }
}

void VRDisplayHost::CancelCurrentSubmitTask() {
  MonitorAutoLock lock(mCurrentSubmitTaskMonitor);
  if (mCurrentSubmitTask) {
    mCurrentSubmitTask->Cancel();
    mCurrentSubmitTask = nullptr;
  }
}

bool VRDisplayHost::CheckClearDisplayInfoDirty() {
  if (mDisplayInfo == mLastUpdateDisplayInfo) {
    return false;
  }
  mLastUpdateDisplayInfo = mDisplayInfo;
  return true;
}

void VRDisplayHost::StartVRNavigation() {}

void VRDisplayHost::StopVRNavigation(const TimeDuration& aTimeout) {}

VRControllerHost::VRControllerHost(VRDeviceType aType, dom::GamepadHand aHand,
                                   uint32_t aDisplayID)
    : mControllerInfo{}, mVibrateIndex(0) {
  MOZ_COUNT_CTOR(VRControllerHost);
  mControllerInfo.mType = aType;
  mControllerInfo.mControllerState.hand = aHand;
  mControllerInfo.mMappingType = dom::GamepadMappingType::_empty;
  mControllerInfo.mDisplayID = aDisplayID;
  mControllerInfo.mControllerID = VRSystemManager::AllocateControllerID();
}

VRControllerHost::~VRControllerHost() { MOZ_COUNT_DTOR(VRControllerHost); }

const VRControllerInfo& VRControllerHost::GetControllerInfo() const {
  return mControllerInfo;
}

void VRControllerHost::SetButtonPressed(uint64_t aBit) {
  mControllerInfo.mControllerState.buttonPressed = aBit;
}

uint64_t VRControllerHost::GetButtonPressed() {
  return mControllerInfo.mControllerState.buttonPressed;
}

void VRControllerHost::SetButtonTouched(uint64_t aBit) {
  mControllerInfo.mControllerState.buttonTouched = aBit;
}

uint64_t VRControllerHost::GetButtonTouched() {
  return mControllerInfo.mControllerState.buttonTouched;
}

void VRControllerHost::SetPose(const dom::GamepadPoseState& aPose) {
  mPose = aPose;
}

const dom::GamepadPoseState& VRControllerHost::GetPose() { return mPose; }

dom::GamepadHand VRControllerHost::GetHand() {
  return mControllerInfo.mControllerState.hand;
}

void VRControllerHost::SetVibrateIndex(uint64_t aIndex) {
  mVibrateIndex = aIndex;
}

uint64_t VRControllerHost::GetVibrateIndex() { return mVibrateIndex; }
