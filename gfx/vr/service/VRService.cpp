/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VRService.h"
#include "gfxPrefs.h"
#include "../gfxVRMutex.h"
#include "base/thread.h"  // for Thread
#include <cstring>        // for memcmp

#if defined(XP_WIN)
#  include "OculusSession.h"
#endif

#if defined(XP_WIN) || defined(XP_MACOSX) || \
    (defined(XP_LINUX) && !defined(MOZ_WIDGET_ANDROID))
#  include "OpenVRSession.h"
#endif
#if !defined(MOZ_WIDGET_ANDROID)
#  include "OSVRSession.h"
#endif

using namespace mozilla;
using namespace mozilla::gfx;
using namespace std;

namespace {

int64_t FrameIDFromBrowserState(const mozilla::gfx::VRBrowserState& aState) {
  for (const auto& layer : aState.layerState) {
    if (layer.type == VRLayerType::LayerType_Stereo_Immersive) {
      return layer.layer_stereo_immersive.frameId;
    }
  }
  return 0;
}

bool IsImmersiveContentActive(const mozilla::gfx::VRBrowserState& aState) {
  for (const auto& layer : aState.layerState) {
    if (layer.type == VRLayerType::LayerType_Stereo_Immersive) {
      return true;
    }
  }
  return false;
}

}  // anonymous namespace

/*static*/ already_AddRefed<VRService> VRService::Create() {
  MOZ_ASSERT(NS_IsMainThread());

  if (!gfxPrefs::VRServiceEnabled()) {
    return nullptr;
  }

  RefPtr<VRService> service = new VRService();
  return service.forget();
}

VRService::VRService()
    : mSystemState{},
      mBrowserState{},
      mBrowserGeneration(0),
      mServiceThread(nullptr),
      mShutdownRequested(false),
      mAPIShmem(nullptr),
      mTargetShmemFile(0),
      mLastHapticState{},
      mFrameStartTime{},
#if defined(XP_WIN)
      mMutex(NULL),
#endif
      mVRProcessEnabled(gfxPrefs::VRProcessEnabled()) {
  // When we have the VR process, we map the memory
  // of mAPIShmem from GPU process.
  // If we don't have the VR process, we will instantiate
  // mAPIShmem in VRService.
  if (!mVRProcessEnabled) {
    mAPIShmem = new VRExternalShmem();
    memset(mAPIShmem, 0, sizeof(VRExternalShmem));
  }
}

VRService::~VRService() {
  Stop();

  if (!mVRProcessEnabled && mAPIShmem) {
    delete mAPIShmem;
    mAPIShmem = nullptr;
  }
}

void VRService::Refresh() {
  if (!mAPIShmem) {
    return;
  }

  if (mAPIShmem->state.displayState.shutdown) {
    Stop();
  }
}

void VRService::Start() {
#if defined(XP_WIN)
  // Adding `!XRE_IsParentProcess()` to avoid Win 7 32-bit WebVR tests
  // to OpenMutex when there is no GPU process to create
  // VRSystemManagerExternal and its mutex.
  if (!mMutex && !XRE_IsParentProcess()) {
     mMutex = OpenMutex(
        MUTEX_ALL_ACCESS,       // request full access
        false,                  // handle not inheritable
        TEXT("mozilla::vr::ShmemMutex"));  // object name

    if (mMutex == NULL) {
      nsAutoCString msg;
      msg.AppendPrintf("VRService OpenMutex error \"%lu\".",
                       GetLastError());
      NS_WARNING(msg.get());
      MOZ_ASSERT(false);
    }
    MOZ_ASSERT(GetLastError() == 0);
  }
#endif

  if (!mServiceThread) {
    /**
     * We must ensure that any time the service is re-started, that
     * the VRSystemState is reset, including mSystemState.enumerationCompleted
     * This must happen before VRService::Start returns to the caller, in order
     * to prevent the WebVR/WebXR promises from being resolved before the
     * enumeration has been completed.
     */
    memset(&mSystemState, 0, sizeof(mSystemState));
    PushState(mSystemState);

    mServiceThread = new base::Thread("VRService");
    base::Thread::Options options;
    /* Timeout values are powers-of-two to enable us get better data.
       128ms is chosen for transient hangs because 8Hz should be the minimally
       acceptable goal for Compositor responsiveness (normal goal is 60Hz). */
    options.transient_hang_timeout = 128;  // milliseconds
    /* 2048ms is chosen for permanent hangs because it's longer than most
     * Compositor hangs seen in the wild, but is short enough to not miss
     * getting native hang stacks. */
    options.permanent_hang_timeout = 2048;  // milliseconds

    if (!mServiceThread->StartWithOptions(options)) {
      mServiceThread->Stop();
      delete mServiceThread;
      mServiceThread = nullptr;
      return;
    }

    mServiceThread->message_loop()->PostTask(
        NewRunnableMethod("gfx::VRService::ServiceInitialize", this,
                          &VRService::ServiceInitialize));
  }
}

void VRService::Stop() {
  if (mServiceThread) {
    mShutdownRequested = true;
    mServiceThread->Stop();
    delete mServiceThread;
    mServiceThread = nullptr;
  }
  if (mTargetShmemFile) {
#if defined(XP_WIN)
    CloseHandle(mTargetShmemFile);
#endif
    mTargetShmemFile = 0;
  }
  if (mVRProcessEnabled && mAPIShmem) {
#if defined(XP_WIN)
    UnmapViewOfFile((void*)mAPIShmem);
#endif
    mAPIShmem = nullptr;
  }
#if defined(XP_WIN)
  if (mMutex) {
    CloseHandle(mMutex);
  }
#endif
  mSession = nullptr;
}

bool VRService::InitShmem() {
  if (!mVRProcessEnabled) {
    return true;
  }

#if defined(XP_WIN)
  const char* kShmemName = "moz.gecko.vr_ext.0.0.1";
  base::ProcessHandle targetHandle = 0;

  // Opening a file-mapping object by name
  targetHandle = OpenFileMappingA(FILE_MAP_ALL_ACCESS,  // read/write access
                                  FALSE,        // do not inherit the name
                                  kShmemName);  // name of mapping object

  MOZ_ASSERT(GetLastError() == 0);

  LARGE_INTEGER length;
  length.QuadPart = sizeof(VRExternalShmem);
  mAPIShmem = (VRExternalShmem*)MapViewOfFile(
      reinterpret_cast<base::ProcessHandle>(
          targetHandle),    // handle to map object
      FILE_MAP_ALL_ACCESS,  // read/write permission
      0, 0, length.QuadPart);
  MOZ_ASSERT(GetLastError() == 0);
  // TODO - Implement logging
  mTargetShmemFile = targetHandle;
  if (!mAPIShmem) {
    MOZ_ASSERT(mAPIShmem);
    return false;
  }
#else
  // TODO: Implement shmem for other platforms.
#endif

  return true;
}

bool VRService::IsInServiceThread() {
  return (mServiceThread != nullptr) &&
         mServiceThread->thread_id() == PlatformThread::CurrentId();
}

void VRService::ServiceInitialize() {
  MOZ_ASSERT(IsInServiceThread());

  if (!InitShmem()) {
    return;
  }

  mShutdownRequested = false;
  memset(&mBrowserState, 0, sizeof(mBrowserState));

  // Try to start a VRSession
  UniquePtr<VRSession> session;

  // We try Oculus first to ensure we use Oculus
  // devices trough the most native interface
  // when possible.
#if defined(XP_WIN)
  // Try Oculus
  session = MakeUnique<OculusSession>();
  if (!session->Initialize(mSystemState)) {
    session = nullptr;
  }
#endif

#if defined(XP_WIN) || defined(XP_MACOSX) || \
    (defined(XP_LINUX) && !defined(MOZ_WIDGET_ANDROID))
  // Try OpenVR
  if (!session) {
    session = MakeUnique<OpenVRSession>();
    if (!session->Initialize(mSystemState)) {
      session = nullptr;
    }
  }
#endif
#if !defined(MOZ_WIDGET_ANDROID)
  // Try OSVR
  if (!session) {
    session = MakeUnique<OSVRSession>();
    if (!session->Initialize(mSystemState)) {
      session = nullptr;
    }
  }
#endif

  if (session) {
    mSession = std::move(session);
    // Setting enumerationCompleted to true indicates to the browser
    // that it should resolve any promises in the WebVR/WebXR API
    // waiting for hardware detection.
    mSystemState.enumerationCompleted = true;
    PushState(mSystemState);

    MessageLoop::current()->PostTask(
        NewRunnableMethod("gfx::VRService::ServiceWaitForImmersive", this,
                          &VRService::ServiceWaitForImmersive));
  } else {
    // VR hardware was not detected.
    // We must inform the browser of the failure so it may try again
    // later and resolve WebVR promises.  A failure or shutdown is
    // indicated by enumerationCompleted being set to true, with all
    // other fields remaining zeroed out.
    memset(&mSystemState, 0, sizeof(mSystemState));
    mSystemState.enumerationCompleted = true;
    mSystemState.displayState.minRestartInterval =
        gfxPrefs::VRExternalNotDetectedTimeout();
    mSystemState.displayState.shutdown = true;
    PushState(mSystemState);
  }
}

void VRService::ServiceShutdown() {
  MOZ_ASSERT(IsInServiceThread());

  // Notify the browser that we have shut down.
  // This is indicated by enumerationCompleted being set
  // to true, with all other fields remaining zeroed out.
  memset(&mSystemState, 0, sizeof(mSystemState));
  mSystemState.enumerationCompleted = true;
  mSystemState.displayState.shutdown = true;
  if (mSession && mSession->ShouldQuit()) {
    mSystemState.displayState.minRestartInterval =
        gfxPrefs::VRExternalQuitTimeout();
  }
  PushState(mSystemState);
  mSession = nullptr;
}

void VRService::ServiceWaitForImmersive() {
  MOZ_ASSERT(IsInServiceThread());
  MOZ_ASSERT(mSession);

  mSession->ProcessEvents(mSystemState);
  PushState(mSystemState);
  PullState(mBrowserState);

  if (mSession->ShouldQuit() || mShutdownRequested) {
    // Shut down
    MessageLoop::current()->PostTask(NewRunnableMethod(
        "gfx::VRService::ServiceShutdown", this, &VRService::ServiceShutdown));
  } else if (IsImmersiveContentActive(mBrowserState)) {
    // Enter Immersive Mode
    mSession->StartPresentation();
    mSession->StartFrame(mSystemState);
    PushState(mSystemState);

    MessageLoop::current()->PostTask(
        NewRunnableMethod("gfx::VRService::ServiceImmersiveMode", this,
                          &VRService::ServiceImmersiveMode));
  } else {
    // Continue waiting for immersive mode
    MessageLoop::current()->PostTask(
        NewRunnableMethod("gfx::VRService::ServiceWaitForImmersive", this,
                          &VRService::ServiceWaitForImmersive));
  }
}

void VRService::ServiceImmersiveMode() {
  MOZ_ASSERT(IsInServiceThread());
  MOZ_ASSERT(mSession);

  mSession->ProcessEvents(mSystemState);
  UpdateHaptics();
  PushState(mSystemState);
  PullState(mBrowserState);

  if (mSession->ShouldQuit() || mShutdownRequested) {
    // Shut down
    MessageLoop::current()->PostTask(NewRunnableMethod(
        "gfx::VRService::ServiceShutdown", this, &VRService::ServiceShutdown));
    return;
  }

  if (!IsImmersiveContentActive(mBrowserState)) {
    // Exit immersive mode
    mSession->StopAllHaptics();
    mSession->StopPresentation();
    MessageLoop::current()->PostTask(
        NewRunnableMethod("gfx::VRService::ServiceWaitForImmersive", this,
                          &VRService::ServiceWaitForImmersive));
    return;
  }

  uint64_t newFrameId = FrameIDFromBrowserState(mBrowserState);
  if (newFrameId != mSystemState.displayState.lastSubmittedFrameId) {
    // A new immersive frame has been received.
    // Submit the textures to the VR system compositor.
    bool success = false;
    for (const auto& layer : mBrowserState.layerState) {
      if (layer.type == VRLayerType::LayerType_Stereo_Immersive) {
        // SubmitFrame may block in order to control the timing for
        // the next frame start
        success = mSession->SubmitFrame(layer.layer_stereo_immersive);
        break;
      }
    }

    // Changing mLastSubmittedFrameId triggers a new frame to start
    // rendering.  Changes to mLastSubmittedFrameId and the values
    // used for rendering, such as headset pose, must be pushed
    // atomically to the browser.
    mSystemState.displayState.lastSubmittedFrameId = newFrameId;
    mSystemState.displayState.lastSubmittedFrameSuccessful = success;

    // StartFrame may block to control the timing for the next frame start
    mSession->StartFrame(mSystemState);
    mSystemState.sensorState.inputFrameID++;
    size_t historyIndex =
        mSystemState.sensorState.inputFrameID % ArrayLength(mFrameStartTime);
    mFrameStartTime[historyIndex] = TimeStamp::Now();
    PushState(mSystemState);
  }

  // Continue immersive mode
  MessageLoop::current()->PostTask(
      NewRunnableMethod("gfx::VRService::ServiceImmersiveMode", this,
                        &VRService::ServiceImmersiveMode));
}

void VRService::UpdateHaptics() {
  MOZ_ASSERT(IsInServiceThread());
  MOZ_ASSERT(mSession);

  for (size_t i = 0; i < ArrayLength(mBrowserState.hapticState); i++) {
    VRHapticState& state = mBrowserState.hapticState[i];
    VRHapticState& lastState = mLastHapticState[i];
    // Note that VRHapticState is asserted to be a POD type, thus memcmp is safe
    if (memcmp(&state, &lastState, sizeof(VRHapticState)) == 0) {
      // No change since the last update
      continue;
    }
    if (state.inputFrameID == 0) {
      // The haptic feedback was stopped
      mSession->StopVibrateHaptic(state.controllerIndex);
    } else {
      TimeStamp now;
      if (now.IsNull()) {
        // TimeStamp::Now() is expensive, so we
        // must call it only when needed and save the
        // output for further loop iterations.
        now = TimeStamp::Now();
      }
      // This is a new haptic pulse, or we are overriding a prior one
      size_t historyIndex = state.inputFrameID % ArrayLength(mFrameStartTime);
      float startOffset =
          (float)(now - mFrameStartTime[historyIndex]).ToSeconds();

      // state.pulseStart is guaranteed never to be in the future
      mSession->VibrateHaptic(
          state.controllerIndex, state.hapticIndex, state.pulseIntensity,
          state.pulseDuration + state.pulseStart - startOffset);
    }
    // Record the state for comparison in the next run
    memcpy(&lastState, &state, sizeof(VRHapticState));
  }
}

void VRService::PushState(const mozilla::gfx::VRSystemState& aState) {
  if (!mAPIShmem) {
    return;
  }
  // Copying the VR service state to the shmem is atomic, infallable,
  // and non-blocking on x86/x64 architectures.  Arm requires a mutex
  // that is locked for the duration of the memcpy to and from shmem on
  // both sides.

#if defined(MOZ_WIDGET_ANDROID)
  if (pthread_mutex_lock((pthread_mutex_t*)&(mExternalShmem->systemMutex)) ==
      0) {
    memcpy((void*)&mAPIShmem->state, &aState, sizeof(VRSystemState));
    pthread_mutex_unlock((pthread_mutex_t*)&(mExternalShmem->systemMutex));
  }
#else
  bool state = true;
#if defined(XP_WIN)
  if (!XRE_IsParentProcess()) {
    WaitForMutex lock(mMutex);
    state = lock.GetStatus();
  }
#endif  // defined(XP_WIN)
  if (state) {
    mAPIShmem->generationA++;
    memcpy((void*)&mAPIShmem->state, &aState, sizeof(VRSystemState));
    mAPIShmem->generationB++;
  }
#endif // defined(MOZ_WIDGET_ANDROID)
}

void VRService::PullState(mozilla::gfx::VRBrowserState& aState) {
  if (!mAPIShmem) {
    return;
  }
  // Copying the browser state from the shmem is non-blocking
  // on x86/x64 architectures.  Arm requires a mutex that is
  // locked for the duration of the memcpy to and from shmem on
  // both sides.
  // On x86/x64 It is fallable -- If a dirty copy is detected by
  // a mismatch of browserGenerationA and browserGenerationB,
  // the copy is discarded and will not replace the last known
  // browser state.

#if defined(MOZ_WIDGET_ANDROID)
  if (pthread_mutex_lock((pthread_mutex_t*)&(mExternalShmem->browserMutex)) ==
      0) {
    memcpy(&aState, &tmp.browserState, sizeof(VRBrowserState));
    pthread_mutex_unlock((pthread_mutex_t*)&(mExternalShmem->browserMutex));
  }
#else
  bool status = true;
#if defined(XP_WIN)
  if (!XRE_IsParentProcess()) {
    WaitForMutex lock(mMutex);
    status = lock.GetStatus();
  }
#endif  // defined(XP_WIN)
  if (status) {
    VRExternalShmem tmp;
    if (mAPIShmem->browserGenerationA != mBrowserGeneration) {
      memcpy(&tmp, mAPIShmem, sizeof(VRExternalShmem));
      if (tmp.browserGenerationA == tmp.browserGenerationB &&
          tmp.browserGenerationA != 0 && tmp.browserGenerationA != -1) {
        memcpy(&aState, &tmp.browserState, sizeof(VRBrowserState));
        mBrowserGeneration = tmp.browserGenerationA;
      }
    }
  }
#endif  // defined(MOZ_WIDGET_ANDROID)
}

VRExternalShmem* VRService::GetAPIShmem() { return mAPIShmem; }
