/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VRService.h"
#include "OpenVRSession.h"
#include "gfxPrefs.h"
#include "base/thread.h"                // for Thread

using namespace mozilla;
using namespace mozilla::gfx;
using namespace std;

namespace {

int64_t
FrameIDFromBrowserState(const mozilla::gfx::VRBrowserState& aState)
{
  for (int iLayer=0; iLayer < kVRLayerMaxCount; iLayer++) {
    const VRLayerState& layer = aState.layerState[iLayer];
    if (layer.type == VRLayerType::LayerType_Stereo_Immersive) {
      return layer.layer_stereo_immersive.mFrameId;
    }
  }
  return 0;
}

bool
IsImmersiveContentActive(const mozilla::gfx::VRBrowserState& aState)
{
  for (int iLayer=0; iLayer < kVRLayerMaxCount; iLayer++) {
    const VRLayerState& layer = aState.layerState[iLayer];
    if (layer.type == VRLayerType::LayerType_Stereo_Immersive) {
      return true;
    }
  }
  return false;
}

} // anonymous namespace

/*static*/ already_AddRefed<VRService>
VRService::Create()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!gfxPrefs::VRServiceEnabled()) {
    return nullptr;
  }

  RefPtr<VRService> service = new VRService();
  return service.forget();
}

VRService::VRService()
 : mSystemState{}
 , mBrowserState{}
 , mServiceThread(nullptr)
 , mShutdownRequested(false)
{
  memset(&mAPIShmem, 0, sizeof(mAPIShmem));
}

VRService::~VRService()
{
  Stop();
}

void
VRService::Start()
{
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
    options.transient_hang_timeout = 128; // milliseconds
    /* 2048ms is chosen for permanent hangs because it's longer than most
     * Compositor hangs seen in the wild, but is short enough to not miss getting
     * native hang stacks. */
    options.permanent_hang_timeout = 2048; // milliseconds

    if (!mServiceThread->StartWithOptions(options)) {
      delete mServiceThread;
      mServiceThread = nullptr;
      return;
    }

    mServiceThread->message_loop()->PostTask(NewRunnableMethod(
      "gfx::VRService::ServiceInitialize",
      this, &VRService::ServiceInitialize
    ));
  }
}

void
VRService::Stop()
{
  if (mServiceThread) {
    mServiceThread->message_loop()->PostTask(NewRunnableMethod(
      "gfx::VRService::RequestShutdown",
      this, &VRService::RequestShutdown
    ));
    delete mServiceThread;
    mServiceThread = nullptr;
  }
}

bool
VRService::IsInServiceThread()
{
  return mServiceThread && mServiceThread->thread_id() == PlatformThread::CurrentId();
}

void
VRService::RequestShutdown()
{
  MOZ_ASSERT(IsInServiceThread());
  mShutdownRequested = true;
}

void
VRService::ServiceInitialize()
{
  MOZ_ASSERT(IsInServiceThread());

  mShutdownRequested = false;
  memset(&mBrowserState, 0, sizeof(mBrowserState));

  // Try to start a VRSession
  unique_ptr<VRSession> session;

  // Try OpenVR
  session = make_unique<OpenVRSession>();
  if (!session->Initialize(mSystemState)) {
    session = nullptr;
  }
  if (session) {
    mSession = std::move(session);
    // Setting enumerationCompleted to true indicates to the browser
    // that it should resolve any promises in the WebVR/WebXR API
    // waiting for hardware detection.
    mSystemState.enumerationCompleted = true;
    PushState(mSystemState);

    MessageLoop::current()->PostTask(NewRunnableMethod(
      "gfx::VRService::ServiceWaitForImmersive",
      this, &VRService::ServiceWaitForImmersive
    ));
  } else {
    // VR hardware was not detected.
    // We must inform the browser of the failure so it may try again
    // later and resolve WebVR promises.  A failure or shutdown is
    // indicated by enumerationCompleted being set to true, with all
    // other fields remaining zeroed out.
    memset(&mSystemState, 0, sizeof(mSystemState));
    mSystemState.enumerationCompleted = true;
    PushState(mSystemState);
  }
}

void
VRService::ServiceShutdown()
{
  MOZ_ASSERT(IsInServiceThread());

  mSession = nullptr;

  // Notify the browser that we have shut down.
  // This is indicated by enumerationCompleted being set
  // to true, with all other fields remaining zeroed out.
  memset(&mSystemState, 0, sizeof(mSystemState));
  mSystemState.enumerationCompleted = true;
  PushState(mSystemState);
}

void
VRService::ServiceWaitForImmersive()
{
  MOZ_ASSERT(IsInServiceThread());
  MOZ_ASSERT(mSession);

  mSession->ProcessEvents(mSystemState);
  PushState(mSystemState);
  PullState(mBrowserState);

  if (mSession->ShouldQuit() || mShutdownRequested) {
    // Shut down
    MessageLoop::current()->PostTask(NewRunnableMethod(
      "gfx::VRService::ServiceShutdown",
      this, &VRService::ServiceShutdown
    ));
  } else if (IsImmersiveContentActive(mBrowserState)) {
    // Enter Immersive Mode
    mSession->StartPresentation();
    mSession->StartFrame(mSystemState);
    PushState(mSystemState);

    MessageLoop::current()->PostTask(NewRunnableMethod(
      "gfx::VRService::ServiceImmersiveMode",
      this, &VRService::ServiceImmersiveMode
    ));
  } else {
    // Continue waiting for immersive mode
    MessageLoop::current()->PostTask(NewRunnableMethod(
      "gfx::VRService::ServiceWaitForImmersive",
      this, &VRService::ServiceWaitForImmersive
    ));
  }
}

void
VRService::ServiceImmersiveMode()
{
  MOZ_ASSERT(IsInServiceThread());
  MOZ_ASSERT(mSession);

  mSession->ProcessEvents(mSystemState);
  PushState(mSystemState);
  PullState(mBrowserState);

  if (mSession->ShouldQuit() || mShutdownRequested) {
    // Shut down
    MessageLoop::current()->PostTask(NewRunnableMethod(
      "gfx::VRService::ServiceShutdown",
      this, &VRService::ServiceShutdown
    ));
    return;
  } else if (!IsImmersiveContentActive(mBrowserState)) {
    // Exit immersive mode
    mSession->StopPresentation();
    MessageLoop::current()->PostTask(NewRunnableMethod(
      "gfx::VRService::ServiceWaitForImmersive",
      this, &VRService::ServiceWaitForImmersive
    ));
    return;
  }

  uint64_t newFrameId = FrameIDFromBrowserState(mBrowserState);
  if (newFrameId != mSystemState.displayState.mLastSubmittedFrameId) {
    // A new immersive frame has been received.
    // Submit the textures to the VR system compositor.
    bool success = false;
    for (int iLayer=0; iLayer < kVRLayerMaxCount; iLayer++) {
      const VRLayerState& layer = mBrowserState.layerState[iLayer];
      if (layer.type == VRLayerType::LayerType_Stereo_Immersive) {
        success = mSession->SubmitFrame(layer.layer_stereo_immersive);
        break;
      }
    }

    // Changing mLastSubmittedFrameId triggers a new frame to start
    // rendering.  Changes to mLastSubmittedFrameId and the values
    // used for rendering, such as headset pose, must be pushed
    // atomically to the browser.
    mSystemState.displayState.mLastSubmittedFrameId = newFrameId;
    mSystemState.displayState.mLastSubmittedFrameSuccessful = success;
    mSession->StartFrame(mSystemState);
    PushState(mSystemState);
  }

  // Continue immersive mode
  MessageLoop::current()->PostTask(NewRunnableMethod(
    "gfx::VRService::ServiceImmersiveMode",
    this, &VRService::ServiceImmersiveMode
  ));
}

void
VRService::PushState(const mozilla::gfx::VRSystemState& aState)
{
  // Copying the VR service state to the shmem is atomic, infallable,
  // and non-blocking on x86/x64 architectures.  Arm requires a mutex
  // that is locked for the duration of the memcpy to and from shmem on
  // both sides.

#if defined(MOZ_WIDGET_ANDROID)
    if (pthread_mutex_lock((pthread_mutex_t*)&(mExternalShmem->systemMutex)) == 0) {
      memcpy((void *)&mAPIShmem.state, &aState, sizeof(VRSystemState));
      pthread_mutex_unlock((pthread_mutex_t*)&(mExternalShmem->systemMutex));
    }
#else
  mAPIShmem.generationA++;
  memcpy((void *)&mAPIShmem.state, &aState, sizeof(VRSystemState));
  mAPIShmem.generationB++;
#endif
}

void
VRService::PullState(mozilla::gfx::VRBrowserState& aState)
{
  // Copying the browser state from the shmem is non-blocking
  // on x86/x64 architectures.  Arm requires a mutex that is
  // locked for the duration of the memcpy to and from shmem on
  // both sides.
  // On x86/x64 It is fallable -- If a dirty copy is detected by
  // a mismatch of browserGenerationA and browserGenerationB,
  // the copy is discarded and will not replace the last known
  // browser state.

#if defined(MOZ_WIDGET_ANDROID)
    if (pthread_mutex_lock((pthread_mutex_t*)&(mExternalShmem->browserMutex)) == 0) {
      memcpy(&aState, &tmp.browserState, sizeof(VRBrowserState));
      pthread_mutex_unlock((pthread_mutex_t*)&(mExternalShmem->browserMutex));
    }
#else
  VRExternalShmem tmp;
  memcpy(&tmp, &mAPIShmem, sizeof(VRExternalShmem));
  if (tmp.browserGenerationA == tmp.browserGenerationB && tmp.browserGenerationA != 0 && tmp.browserGenerationA != -1) {
    memcpy(&aState, &tmp.browserState, sizeof(VRBrowserState));
  }
#endif
}

VRExternalShmem*
VRService::GetAPIShmem()
{
  return &mAPIShmem;
}
