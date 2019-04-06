/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <math.h>

#include "prlink.h"
#include "prenv.h"
#include "gfxPrefs.h"
#include "mozilla/Preferences.h"

#include "mozilla/gfx/Quaternion.h"

#ifdef XP_WIN
#  include "CompositorD3D11.h"
#  include "TextureD3D11.h"
static const char* kShmemName = "moz.gecko.vr_ext.0.0.1";
#elif defined(XP_MACOSX)
#  include "mozilla/gfx/MacIOSurface.h"
#  include <sys/mman.h>
#  include <sys/stat.h> /* For mode constants */
#  include <fcntl.h>    /* For O_* constants */
#  include <errno.h>
static const char* kShmemName = "/moz.gecko.vr_ext.0.0.1";
#elif defined(MOZ_WIDGET_ANDROID)
#  include <string.h>
#  include <pthread.h>
#  include "GeckoVRManager.h"
#endif  // defined(MOZ_WIDGET_ANDROID)

#include "gfxVRExternal.h"
#include "gfxVRMutex.h"
#include "VRManagerParent.h"
#include "VRManager.h"
#include "VRThread.h"

#include "nsServiceManagerUtils.h"
#include "nsIScreenManager.h"

#include "mozilla/dom/GamepadEventTypes.h"
#include "mozilla/dom/GamepadBinding.h"
#include "mozilla/Telemetry.h"

#ifndef M_PI
#  define M_PI 3.14159265358979323846
#endif

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::gfx::impl;
using namespace mozilla::layers;
using namespace mozilla::dom;

VRDisplayExternal::VRDisplayExternal(const VRDisplayState& aDisplayState)
    : VRDisplayHost(VRDeviceType::External),
      mHapticPulseRemaining{},
      mBrowserState{},
      mLastSensorState{} {
  MOZ_COUNT_CTOR_INHERITED(VRDisplayExternal, VRDisplayHost);
  mDisplayInfo.mDisplayState = aDisplayState;

  // default to an identity quaternion
  mLastSensorState.pose.orientation[3] = 1.0f;
}

VRDisplayExternal::~VRDisplayExternal() {
  Destroy();
  MOZ_COUNT_DTOR_INHERITED(VRDisplayExternal, VRDisplayHost);
}

void VRDisplayExternal::Destroy() {
  StopAllHaptics();
  StopPresentation();
}

void VRDisplayExternal::ZeroSensor() {}

void VRDisplayExternal::Run1msTasks(double aDeltaTime) {
  VRDisplayHost::Run1msTasks(aDeltaTime);
  UpdateHaptics(aDeltaTime);
}

void VRDisplayExternal::Run10msTasks() {
  VRDisplayHost::Run10msTasks();
  ExpireNavigationTransition();
  PullState();
  PushState();

  // 1ms tasks will always be run before
  // the 10ms tasks, so no need to include
  // them here as well.
}

void VRDisplayExternal::ExpireNavigationTransition() {
  if (!mVRNavigationTransitionEnd.IsNull() &&
      TimeStamp::Now() > mVRNavigationTransitionEnd) {
    mBrowserState.navigationTransitionActive = false;
  }
}

VRHMDSensorState& VRDisplayExternal::GetSensorState() {
  return mLastSensorState;
}

void VRDisplayExternal::StartPresentation() {
  if (mBrowserState.presentationActive) {
    return;
  }
  mTelemetry.Clear();
  mTelemetry.mPresentationStart = TimeStamp::Now();

  // Indicate that we are ready to start immersive mode
  mBrowserState.presentationActive = true;
  mBrowserState.layerState[0].type = VRLayerType::LayerType_Stereo_Immersive;
  PushState();

  mDisplayInfo.mDisplayState.lastSubmittedFrameId = 0;
  if (mDisplayInfo.mDisplayState.reportsDroppedFrames) {
    mTelemetry.mLastDroppedFrameCount =
        mDisplayInfo.mDisplayState.droppedFrameCount;
  }

#if defined(MOZ_WIDGET_ANDROID)
  mLastSubmittedFrameId = 0;
  mLastStartedFrame = 0;
#endif
}

void VRDisplayExternal::StopPresentation() {
  if (!mBrowserState.presentationActive) {
    return;
  }

  // Indicate that we have stopped immersive mode
  mBrowserState.presentationActive = false;
  memset(mBrowserState.layerState, 0,
         sizeof(VRLayerState) * mozilla::ArrayLength(mBrowserState.layerState));

  PushState(true);

  Telemetry::HistogramID timeSpentID = Telemetry::HistogramCount;
  Telemetry::HistogramID droppedFramesID = Telemetry::HistogramCount;
  int viewIn = 0;

  if (mDisplayInfo.mDisplayState.eightCC ==
      GFX_VR_EIGHTCC('O', 'c', 'u', 'l', 'u', 's', ' ', 'D')) {
    // Oculus Desktop API
    timeSpentID = Telemetry::WEBVR_TIME_SPENT_VIEWING_IN_OCULUS;
    droppedFramesID = Telemetry::WEBVR_DROPPED_FRAMES_IN_OCULUS;
    viewIn = 1;
  } else if (mDisplayInfo.mDisplayState.eightCC ==
             GFX_VR_EIGHTCC('O', 'p', 'e', 'n', 'V', 'R', ' ', ' ')) {
    // OpenVR API
    timeSpentID = Telemetry::WEBVR_TIME_SPENT_VIEWING_IN_OPENVR;
    droppedFramesID = Telemetry::WEBVR_DROPPED_FRAMES_IN_OPENVR;
    viewIn = 2;
  }

  if (viewIn) {
    const TimeDuration duration =
        TimeStamp::Now() - mTelemetry.mPresentationStart;
    Telemetry::Accumulate(Telemetry::WEBVR_USERS_VIEW_IN, viewIn);
    Telemetry::Accumulate(timeSpentID, duration.ToMilliseconds());
    const uint32_t droppedFramesPerSec =
        (mDisplayInfo.mDisplayState.droppedFrameCount -
         mTelemetry.mLastDroppedFrameCount) /
        duration.ToSeconds();
    Telemetry::Accumulate(droppedFramesID, droppedFramesPerSec);
  }
}

void VRDisplayExternal::StartVRNavigation() {
  mBrowserState.navigationTransitionActive = true;
  mVRNavigationTransitionEnd = TimeStamp();
  PushState();
}

void VRDisplayExternal::StopVRNavigation(const TimeDuration& aTimeout) {
  if (aTimeout.ToMilliseconds() <= 0) {
    mBrowserState.navigationTransitionActive = false;
    mVRNavigationTransitionEnd = TimeStamp();
    PushState();
  }
  mVRNavigationTransitionEnd = TimeStamp::Now() + aTimeout;
}

bool VRDisplayExternal::PopulateLayerTexture(
    const layers::SurfaceDescriptor& aTexture, VRLayerTextureType* aTextureType,
    VRLayerTextureHandle* aTextureHandle) {
  switch (aTexture.type()) {
#if defined(XP_WIN)
    case SurfaceDescriptor::TSurfaceDescriptorD3D10: {
      const SurfaceDescriptorD3D10& surf =
          aTexture.get_SurfaceDescriptorD3D10();
      *aTextureType =
          VRLayerTextureType::LayerTextureType_D3D10SurfaceDescriptor;
      *aTextureHandle = (void*)surf.handle();
      return true;
    }
#elif defined(XP_MACOSX)
    case SurfaceDescriptor::TSurfaceDescriptorMacIOSurface: {
      // MacIOSurface ptr can't be fetched or used at different threads.
      // Both of fetching and using this MacIOSurface are at the VRService
      // thread.
      const auto& desc = aTexture.get_SurfaceDescriptorMacIOSurface();
      *aTextureType = VRLayerTextureType::LayerTextureType_MacIOSurface;
      *aTextureHandle = desc.surfaceId();
      return true;
    }
#elif defined(MOZ_WIDGET_ANDROID)
    case SurfaceDescriptor::TSurfaceTextureDescriptor: {
      const SurfaceTextureDescriptor& desc =
          aTexture.get_SurfaceTextureDescriptor();
      java::GeckoSurfaceTexture::LocalRef surfaceTexture =
          java::GeckoSurfaceTexture::Lookup(desc.handle());
      if (!surfaceTexture) {
        NS_WARNING("VRDisplayHost::SubmitFrame failed to get a SurfaceTexture");
        return false;
      }
      *aTextureType = VRLayerTextureType::LayerTextureType_GeckoSurfaceTexture;
      *aTextureHandle = desc.handle();
      return true;
    }
#endif
    default: {
      MOZ_ASSERT(false);
      return false;
    }
  }
}

bool VRDisplayExternal::SubmitFrame(const layers::SurfaceDescriptor& aTexture,
                                    uint64_t aFrameId,
                                    const gfx::Rect& aLeftEyeRect,
                                    const gfx::Rect& aRightEyeRect) {
  MOZ_ASSERT(mBrowserState.layerState[0].type ==
             VRLayerType::LayerType_Stereo_Immersive);
  VRLayer_Stereo_Immersive& layer =
      mBrowserState.layerState[0].layer_stereo_immersive;
  if (!PopulateLayerTexture(aTexture, &layer.textureType,
                            &layer.textureHandle)) {
    return false;
  }
  layer.frameId = aFrameId;
  layer.inputFrameId =
      mDisplayInfo.mLastSensorState[mDisplayInfo.mFrameId % kVRMaxLatencyFrames]
          .inputFrameID;

  layer.leftEyeRect.x = aLeftEyeRect.x;
  layer.leftEyeRect.y = aLeftEyeRect.y;
  layer.leftEyeRect.width = aLeftEyeRect.width;
  layer.leftEyeRect.height = aLeftEyeRect.height;
  layer.rightEyeRect.x = aRightEyeRect.x;
  layer.rightEyeRect.y = aRightEyeRect.y;
  layer.rightEyeRect.width = aRightEyeRect.width;
  layer.rightEyeRect.height = aRightEyeRect.height;

  PushState(true);

#if defined(MOZ_WIDGET_ANDROID)
  PullState([&]() {
    return (mDisplayInfo.mDisplayState.lastSubmittedFrameId >= aFrameId) ||
           mDisplayInfo.mDisplayState.suppressFrames ||
           !mDisplayInfo.mDisplayState.isConnected;
  });

  if (mDisplayInfo.mDisplayState.suppressFrames ||
      !mDisplayInfo.mDisplayState.isConnected) {
    // External implementation wants to supress frames, service has shut down or
    // hardware has been disconnected.
    return false;
  }
#else
  while (mDisplayInfo.mDisplayState.lastSubmittedFrameId < aFrameId) {
    if (PullState()) {
      if (mDisplayInfo.mDisplayState.suppressFrames ||
          !mDisplayInfo.mDisplayState.isConnected) {
        // External implementation wants to supress frames, service has shut
        // down or hardware has been disconnected.
        return false;
      }
    }
#  ifdef XP_WIN
    Sleep(0);
#  else
    sleep(0);
#  endif
  }
#endif  // defined(MOZ_WIDGET_ANDROID)

  return mDisplayInfo.mDisplayState.lastSubmittedFrameSuccessful;
}

void VRDisplayExternal::VibrateHaptic(uint32_t aControllerIdx,
                                      uint32_t aHapticIndex, double aIntensity,
                                      double aDuration,
                                      const VRManagerPromise& aPromise) {
  TimeStamp now = TimeStamp::Now();
  size_t bestSlotIndex = 0;
  // Default to an empty slot, or the slot holding the oldest haptic pulse
  for (size_t i = 0; i < mozilla::ArrayLength(mBrowserState.hapticState); i++) {
    const VRHapticState& state = mBrowserState.hapticState[i];
    if (state.inputFrameID == 0) {
      // Unused slot, use it
      bestSlotIndex = i;
      break;
    }
    if (mHapticPulseRemaining[i] < mHapticPulseRemaining[bestSlotIndex]) {
      // If no empty slots are available, fall back to overriding
      // the pulse which is ending soonest.
      bestSlotIndex = i;
    }
  }
  // Override the last pulse on the same actuator if present.
  for (size_t i = 0; i < mozilla::ArrayLength(mBrowserState.hapticState); i++) {
    const VRHapticState& state = mBrowserState.hapticState[i];
    if (state.inputFrameID == 0) {
      // This is an empty slot -- no match
      continue;
    }
    if (state.controllerIndex == aControllerIdx &&
        state.hapticIndex == aHapticIndex) {
      // Found pulse on same actuator -- let's override it.
      bestSlotIndex = i;
    }
  }
  ClearHapticSlot(bestSlotIndex);

  // Populate the selected slot with new haptic state
  size_t bufferIndex = mDisplayInfo.mFrameId % kVRMaxLatencyFrames;
  VRHapticState& bestSlot = mBrowserState.hapticState[bestSlotIndex];
  bestSlot.inputFrameID =
      mDisplayInfo.mLastSensorState[bufferIndex].inputFrameID;
  bestSlot.controllerIndex = aControllerIdx;
  bestSlot.hapticIndex = aHapticIndex;
  bestSlot.pulseStart = (now - mLastFrameStart[bufferIndex]).ToSeconds();
  bestSlot.pulseDuration = aDuration;
  bestSlot.pulseIntensity = aIntensity;
  // Convert from seconds to ms
  mHapticPulseRemaining[bestSlotIndex] = aDuration * 1000.0f;
  MOZ_ASSERT(bestSlotIndex <= mHapticPromises.Length());
  if (bestSlotIndex == mHapticPromises.Length()) {
    mHapticPromises.AppendElement(
        UniquePtr<VRManagerPromise>(new VRManagerPromise(aPromise)));
  } else {
    mHapticPromises[bestSlotIndex] =
        UniquePtr<VRManagerPromise>(new VRManagerPromise(aPromise));
  }
  PushState();
}

void VRDisplayExternal::ClearHapticSlot(size_t aSlot) {
  MOZ_ASSERT(aSlot < mozilla::ArrayLength(mBrowserState.hapticState));
  memset(&mBrowserState.hapticState[aSlot], 0, sizeof(VRHapticState));
  mHapticPulseRemaining[aSlot] = 0.0f;
  if (aSlot < mHapticPromises.Length() && mHapticPromises[aSlot]) {
    VRManager* vm = VRManager::Get();
    vm->NotifyVibrateHapticCompleted(*mHapticPromises[aSlot]);
    mHapticPromises[aSlot] = nullptr;
  }
}

void VRDisplayExternal::UpdateHaptics(double aDeltaTime) {
  bool bNeedPush = false;
  // Check for any haptic pulses that have ended and clear them
  for (size_t i = 0; i < mozilla::ArrayLength(mBrowserState.hapticState); i++) {
    const VRHapticState& state = mBrowserState.hapticState[i];
    if (state.inputFrameID == 0) {
      // Nothing in this slot
      continue;
    }
    mHapticPulseRemaining[i] -= aDeltaTime;
    if (mHapticPulseRemaining[i] <= 0.0f) {
      // The pulse has finished
      ClearHapticSlot(i);
      bNeedPush = true;
    }
  }
  if (bNeedPush) {
    PushState();
  }
}

void VRDisplayExternal::StopVibrateHaptic(uint32_t aControllerIdx) {
  for (size_t i = 0; i < mozilla::ArrayLength(mBrowserState.hapticState); i++) {
    VRHapticState& state = mBrowserState.hapticState[i];
    if (state.controllerIndex == aControllerIdx) {
      memset(&state, 0, sizeof(VRHapticState));
    }
  }
  PushState();
}

void VRDisplayExternal::StopAllHaptics() {
  for (size_t i = 0; i < mozilla::ArrayLength(mBrowserState.hapticState); i++) {
    ClearHapticSlot(i);
  }
  PushState();
}

void VRDisplayExternal::PushState(bool aNotifyCond) {
  VRManager* vm = VRManager::Get();
  VRSystemManagerExternal* manager = vm->GetExternalManager();
  manager->PushState(&mBrowserState, aNotifyCond);
}

#if defined(MOZ_WIDGET_ANDROID)
bool VRDisplayExternal::PullState(const std::function<bool()>& aWaitCondition) {
  VRManager* vm = VRManager::Get();
  VRSystemManagerExternal* manager = vm->GetExternalManager();
  return manager->PullState(&mDisplayInfo.mDisplayState, &mLastSensorState,
                            mDisplayInfo.mControllerState, aWaitCondition);
}
#else
bool VRDisplayExternal::PullState() {
  VRManager* vm = VRManager::Get();
  VRSystemManagerExternal* manager = vm->GetExternalManager();
  nsTArray<RefPtr<gfx::VRDisplayHost>> displays;
  manager->GetHMDs(displays);

  // When VR process crashes, it happenes VRDisplayHost is destroyed
  // but its mSubmitThread is still running. We need add this
  // to check if we still need to access its shmem.
  if (!displays.Length()) {
    return false;
  }

  return manager->PullState(&mDisplayInfo.mDisplayState, &mLastSensorState,
                            mDisplayInfo.mControllerState);
}
#endif

VRSystemManagerExternal::VRSystemManagerExternal(
    VRExternalShmem* aAPIShmem /* = nullptr*/)
    : mExternalShmem(aAPIShmem)
#if !defined(MOZ_WIDGET_ANDROID)
#  if defined(XP_WIN)
      ,
      mMutex(NULL)
#  endif  // defined(XP_WIN)
      ,
      mSameProcess(aAPIShmem != nullptr)
#endif  // !defined(MOZ_WIDGET_ANDROID)
{
#if defined(XP_MACOSX)
  mShmemFD = 0;
#elif defined(XP_WIN)
  mShmemFile = NULL;
#elif defined(MOZ_WIDGET_ANDROID)
  mExternalStructFailed = false;
  mEnumerationCompleted = false;
#endif
  mDoShutdown = false;

#if defined(XP_WIN)
  mMutex = CreateMutex(NULL,   // default security descriptor
                       false,  // mutex not owned
                       TEXT("mozilla::vr::ShmemMutex"));  // object name

  if (mMutex == NULL) {
    nsAutoCString msg;
    msg.AppendPrintf("VRSystemManagerExternal CreateMutex error \"%lu\".",
                     GetLastError());
    NS_WARNING(msg.get());
    MOZ_ASSERT(false);
    return;
  }
  // At xpcshell extension tests, it creates multiple VRSystemManagerExternal
  // instances in plug-contrainer.exe. It causes GetLastError() return
  // `ERROR_ALREADY_EXISTS`. However, even though `ERROR_ALREADY_EXISTS`, it
  // still returns the same mutex handle.
  //
  // https://docs.microsoft.com/en-us/windows/desktop/api/synchapi/nf-synchapi-createmutexa
  MOZ_ASSERT(GetLastError() == 0 || GetLastError() == ERROR_ALREADY_EXISTS);
#endif  // defined(XP_WIN)
}

VRSystemManagerExternal::~VRSystemManagerExternal() {
  CloseShmem();
#if defined(XP_WIN)
  if (mMutex) {
    CloseHandle(mMutex);
    mMutex = NULL;
  }
#endif
}

void VRSystemManagerExternal::OpenShmem() {
  if (mExternalShmem) {
    return;
#if defined(MOZ_WIDGET_ANDROID)
  } else if (mExternalStructFailed) {
    return;
#endif  // defined(MOZ_WIDGET_ANDROID)
  }

#if defined(XP_MACOSX)
  if (mShmemFD == 0) {
    mShmemFD =
        shm_open(kShmemName, O_RDWR, S_IRUSR | S_IWUSR | S_IROTH | S_IWOTH);
  }
  if (mShmemFD <= 0) {
    mShmemFD = 0;
    return;
  }

  struct stat sb;
  fstat(mShmemFD, &sb);
  off_t length = sb.st_size;
  if (length < (off_t)sizeof(VRExternalShmem)) {
    // TODO - Implement logging
    CloseShmem();
    return;
  }

  mExternalShmem = (VRExternalShmem*)mmap(NULL, length, PROT_READ | PROT_WRITE,
                                          MAP_SHARED, mShmemFD, 0);
  if (mExternalShmem == MAP_FAILED) {
    // TODO - Implement logging
    mExternalShmem = NULL;
    CloseShmem();
    return;
  }

#elif defined(XP_WIN)
  if (mShmemFile == NULL) {
    if (gfxPrefs::VRProcessEnabled()) {
      mShmemFile =
          CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0,
                             sizeof(VRExternalShmem), kShmemName);
      MOZ_ASSERT(GetLastError() == 0 || GetLastError() == ERROR_ALREADY_EXISTS);
      MOZ_ASSERT(mShmemFile);
    } else {
      mShmemFile = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, kShmemName);
    }

    if (mShmemFile == NULL) {
      // TODO - Implement logging
      CloseShmem();
      return;
    }
  }
  LARGE_INTEGER length;
  length.QuadPart = sizeof(VRExternalShmem);
  mExternalShmem = (VRExternalShmem*)MapViewOfFile(
      mShmemFile,           // handle to map object
      FILE_MAP_ALL_ACCESS,  // read/write permission
      0, 0, length.QuadPart);

  if (mExternalShmem == NULL) {
    // TODO - Implement logging
    CloseShmem();
    return;
  }
#elif defined(MOZ_WIDGET_ANDROID)
  mExternalShmem =
      (VRExternalShmem*)mozilla::GeckoVRManager::GetExternalContext();
  if (!mExternalShmem) {
    return;
  }
  int32_t version = -1;
  int32_t size = 0;
  if (pthread_mutex_lock((pthread_mutex_t*)&(mExternalShmem->systemMutex)) ==
      0) {
    version = mExternalShmem->version;
    size = mExternalShmem->size;
    pthread_mutex_unlock((pthread_mutex_t*)&(mExternalShmem->systemMutex));
  } else {
    return;
  }
  if (version != kVRExternalVersion) {
    mExternalShmem = nullptr;
    mExternalStructFailed = true;
    return;
  }
  if (size != sizeof(VRExternalShmem)) {
    mExternalShmem = nullptr;
    mExternalStructFailed = true;
    return;
  }
#endif
  CheckForShutdown();
}

void VRSystemManagerExternal::CheckForShutdown() {
  if (mDoShutdown) {
    Shutdown();
  }
}

void VRSystemManagerExternal::CloseShmem() {
#if !defined(MOZ_WIDGET_ANDROID)
  if (mSameProcess) {
    return;
  }
#endif
#if defined(XP_MACOSX)
  if (mExternalShmem) {
    munmap((void*)mExternalShmem, sizeof(VRExternalShmem));
    mExternalShmem = NULL;
  }
  if (mShmemFD) {
    close(mShmemFD);
  }
  mShmemFD = 0;
#elif defined(XP_WIN)
  if (mExternalShmem) {
    UnmapViewOfFile((void*)mExternalShmem);
    mExternalShmem = NULL;
  }
  if (mShmemFile) {
    CloseHandle(mShmemFile);
    mShmemFile = NULL;
  }
#elif defined(MOZ_WIDGET_ANDROID)
  mExternalShmem = NULL;
#endif
}

/*static*/
already_AddRefed<VRSystemManagerExternal> VRSystemManagerExternal::Create(
    VRExternalShmem* aAPIShmem /* = nullptr*/) {
  MOZ_ASSERT(NS_IsMainThread());

  if (!gfxPrefs::VREnabled()) {
    return nullptr;
  }

  if ((!gfxPrefs::VRExternalEnabled() && aAPIShmem == nullptr)
#if defined(XP_WIN)
      || !XRE_IsGPUProcess()
#endif
  ) {
    return nullptr;
  }

  RefPtr<VRSystemManagerExternal> manager =
      new VRSystemManagerExternal(aAPIShmem);
  return manager.forget();
}

void VRSystemManagerExternal::Destroy() { Shutdown(); }

void VRSystemManagerExternal::Shutdown() {
  if (mDisplay) {
    // We will close Shmem at the next frame to avoid
    // mSubmitThread is still running but its shmem
    // has been released.
    mDisplay->ShutdownSubmitThread();
    mDisplay = nullptr;
  } else {
    mDisplay = nullptr;
    CloseShmem();
  }

  mDoShutdown = false;
}

void VRSystemManagerExternal::Run100msTasks() {
  VRSystemManager::Run100msTasks();
  // 1ms and 10ms tasks will always be run before
  // the 100ms tasks, so no need to run them
  // redundantly here.

  CheckForShutdown();
}

void VRSystemManagerExternal::Enumerate() {
  if (mDisplay == nullptr) {
    OpenShmem();
    if (mExternalShmem) {
      VRDisplayState displayState;
      memset(&displayState, 0, sizeof(VRDisplayState));
      // We must block until enumeration has completed in order
      // to signal that the WebVR promise should be resolved at the
      // right time.
#if defined(MOZ_WIDGET_ANDROID)
      PullState(&displayState, nullptr, nullptr,
                [&]() { return mEnumerationCompleted; });
#else
      while (!PullState(&displayState)) {
#  ifdef XP_WIN
        Sleep(0);
#  else
        sleep(0);
#  endif  // XP_WIN
      }
#endif    // defined(MOZ_WIDGET_ANDROID)

      if (displayState.isConnected) {
        mDisplay = new VRDisplayExternal(displayState);
      }
    }
  }
}

bool VRSystemManagerExternal::ShouldInhibitEnumeration() {
  if (VRSystemManager::ShouldInhibitEnumeration()) {
    return true;
  }
  if (!mEarliestRestartTime.IsNull() &&
      mEarliestRestartTime > TimeStamp::Now()) {
    // When the VR Service shuts down it informs us of how long we
    // must wait until we can re-start it.
    // We must wait until mEarliestRestartTime before attempting
    // to enumerate again.
    return true;
  }
  if (mDisplay) {
    // When we find an a VR device, don't
    // allow any further enumeration as it
    // may get picked up redundantly by other
    // API's.
    return true;
  }
  return false;
}

void VRSystemManagerExternal::GetHMDs(
    nsTArray<RefPtr<VRDisplayHost>>& aHMDResult) {
  if (mDisplay) {
    aHMDResult.AppendElement(mDisplay);
  }
}

bool VRSystemManagerExternal::GetIsPresenting() {
  if (mDisplay) {
    VRDisplayInfo displayInfo(mDisplay->GetDisplayInfo());
    return displayInfo.GetPresentingGroups() != 0;
  }

  return false;
}

void VRSystemManagerExternal::VibrateHaptic(uint32_t aControllerIdx,
                                            uint32_t aHapticIndex,
                                            double aIntensity, double aDuration,
                                            const VRManagerPromise& aPromise) {
  if (mDisplay) {
    // VRDisplayClient::FireGamepadEvents() assigns a controller ID with ranges
    // based on displayID.  We must translate this to the indexes understood by
    // VRDisplayExternal.
    uint32_t controllerBaseIndex =
        kVRControllerMaxCount * mDisplay->GetDisplayInfo().mDisplayID;
    uint32_t controllerIndex = aControllerIdx - controllerBaseIndex;
    double aDurationSeconds = aDuration * 0.001f;
    mDisplay->VibrateHaptic(controllerIndex, aHapticIndex, aIntensity,
                            aDurationSeconds, aPromise);
  }
}

void VRSystemManagerExternal::StopVibrateHaptic(uint32_t aControllerIdx) {
  if (mDisplay) {
    // VRDisplayClient::FireGamepadEvents() assigns a controller ID with ranges
    // based on displayID.  We must translate this to the indexes understood by
    // VRDisplayExternal.
    uint32_t controllerBaseIndex =
        kVRControllerMaxCount * mDisplay->GetDisplayInfo().mDisplayID;
    uint32_t controllerIndex = aControllerIdx - controllerBaseIndex;
    mDisplay->StopVibrateHaptic(controllerIndex);
  }
}

void VRSystemManagerExternal::GetControllers(
    nsTArray<RefPtr<VRControllerHost>>& aControllerResult) {
  // Controller updates are handled in VRDisplayClient for
  // VRSystemManagerExternal
  aControllerResult.Clear();
}

void VRSystemManagerExternal::ScanForControllers() {
  // Controller updates are handled in VRDisplayClient for
  // VRSystemManagerExternal
}

void VRSystemManagerExternal::HandleInput() {
  // Controller updates are handled in VRDisplayClient for
  // VRSystemManagerExternal
}

void VRSystemManagerExternal::RemoveControllers() {
  if (mDisplay) {
    mDisplay->StopAllHaptics();
  }
  // Controller updates are handled in VRDisplayClient for
  // VRSystemManagerExternal
}

#if defined(MOZ_WIDGET_ANDROID)
bool VRSystemManagerExternal::PullState(
    VRDisplayState* aDisplayState,
    VRHMDSensorState* aSensorState /* = nullptr */,
    VRControllerState* aControllerState /* = nullptr */,
    const std::function<bool()>& aWaitCondition /* = nullptr */) {
  MOZ_ASSERT(mExternalShmem);
  if (!mExternalShmem) {
    return false;
  }
  bool done = false;
  while (!done) {
    if (pthread_mutex_lock((pthread_mutex_t*)&(mExternalShmem->systemMutex)) ==
        0) {
      while (true) {
        memcpy(aDisplayState, (void*)&(mExternalShmem->state.displayState),
               sizeof(VRDisplayState));
        if (aSensorState) {
          memcpy(aSensorState, (void*)&(mExternalShmem->state.sensorState),
                 sizeof(VRHMDSensorState));
        }
        if (aControllerState) {
          memcpy(aControllerState,
                 (void*)&(mExternalShmem->state.controllerState),
                 sizeof(VRControllerState) * kVRControllerMaxCount);
        }
        mEnumerationCompleted = mExternalShmem->state.enumerationCompleted;
        if (aDisplayState->shutdown) {
          mDoShutdown = true;
          TimeStamp now = TimeStamp::Now();
          if (!mEarliestRestartTime.IsNull() && mEarliestRestartTime < now) {
            mEarliestRestartTime =
                now + TimeDuration::FromMilliseconds(
                          (double)aDisplayState->minRestartInterval);
          }
        }
        if (!aWaitCondition || aWaitCondition()) {
          done = true;
          break;
        }
        // Block current thead using the condition variable until data changes
        pthread_cond_wait((pthread_cond_t*)&mExternalShmem->systemCond,
                          (pthread_mutex_t*)&mExternalShmem->systemMutex);
      }
      pthread_mutex_unlock((pthread_mutex_t*)&(mExternalShmem->systemMutex));
    } else if (!aWaitCondition) {
      // pthread_mutex_lock failed and we are not waiting for a condition to
      // exit from PullState call. return false to indicate that PullState call
      // failed
      return false;
    }
  }
  return true;
}
#else
bool VRSystemManagerExternal::PullState(
    VRDisplayState* aDisplayState,
    VRHMDSensorState* aSensorState /* = nullptr */,
    VRControllerState* aControllerState /* = nullptr */) {
  bool success = false;
  bool status = true;
  MOZ_ASSERT(mExternalShmem);

#  if defined(XP_WIN)
  WaitForMutex lock(mMutex);
  status = lock.GetStatus();
#  endif  // defined(XP_WIN)

  if (mExternalShmem && status) {
    VRExternalShmem tmp;
    memcpy(&tmp, (void*)mExternalShmem, sizeof(VRExternalShmem));
    if (tmp.generationA == tmp.generationB && tmp.generationA != 0 &&
        tmp.generationA != -1 && tmp.state.enumerationCompleted) {
      memcpy(aDisplayState, &tmp.state.displayState, sizeof(VRDisplayState));
      if (aSensorState) {
        memcpy(aSensorState, &tmp.state.sensorState, sizeof(VRHMDSensorState));
      }
      if (aControllerState) {
        memcpy(aControllerState,
               (void*)&(mExternalShmem->state.controllerState),
               sizeof(VRControllerState) * kVRControllerMaxCount);
      }
      if (aDisplayState->shutdown) {
        mDoShutdown = true;
        TimeStamp now = TimeStamp::Now();
        if (!mEarliestRestartTime.IsNull() && mEarliestRestartTime < now) {
          mEarliestRestartTime =
              now + TimeDuration::FromMilliseconds(
                        (double)aDisplayState->minRestartInterval);
        }
      }
      success = true;
    }
  }

  return success;
}
#endif    // defined(MOZ_WIDGET_ANDROID)

void VRSystemManagerExternal::PushState(VRBrowserState* aBrowserState,
                                        bool aNotifyCond) {
  MOZ_ASSERT(aBrowserState);
  if (mExternalShmem) {
#if defined(MOZ_WIDGET_ANDROID)
    if (pthread_mutex_lock((pthread_mutex_t*)&(mExternalShmem->geckoMutex)) ==
        0) {
      memcpy((void*)&(mExternalShmem->geckoState), aBrowserState,
             sizeof(VRBrowserState));
      if (aNotifyCond) {
        pthread_cond_signal((pthread_cond_t*)&(mExternalShmem->geckoCond));
      }
      pthread_mutex_unlock((pthread_mutex_t*)&(mExternalShmem->geckoMutex));
    }
#else
    bool status = true;
#  if defined(XP_WIN)
    WaitForMutex lock(mMutex);
    status = lock.GetStatus();
#  endif  // defined(XP_WIN)
    if (status) {
      mExternalShmem->geckoGenerationA++;
      memcpy((void*)&(mExternalShmem->geckoState), (void*)aBrowserState,
             sizeof(VRBrowserState));
      mExternalShmem->geckoGenerationB++;
    }
#endif    // defined(MOZ_WIDGET_ANDROID)
  }
}
