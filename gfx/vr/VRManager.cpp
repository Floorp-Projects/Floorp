/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VRManager.h"

#include "VRManagerParent.h"
#include "VRShMem.h"
#include "VRThread.h"
#include "gfxVR.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/dom/VRDisplay.h"
#include "mozilla/dom/GamepadEventTypes.h"
#include "mozilla/layers/TextureHost.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/Telemetry.h"
#include "mozilla/Unused.h"

#include "gfxVR.h"
#include <cstring>

#include "ipc/VRLayerParent.h"
#if !defined(MOZ_WIDGET_ANDROID)
#  include "VRServiceHost.h"
#endif

#ifdef XP_WIN
#  include "CompositorD3D11.h"
#  include "TextureD3D11.h"
#  include <d3d11.h>
#  include "gfxWindowsPlatform.h"
#  include "mozilla/gfx/DeviceManagerDx.h"
#elif defined(XP_MACOSX)
#  include "mozilla/gfx/MacIOSurface.h"
#  include <errno.h>
#elif defined(MOZ_WIDGET_ANDROID)
#  include <string.h>
#  include <pthread.h>
#  include "GeckoVRManager.h"
#  include "mozilla/layers/CompositorThread.h"
#endif  // defined(MOZ_WIDGET_ANDROID)

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::layers;
using namespace mozilla::gl;

namespace mozilla {
namespace gfx {

/**
 * When VR content is active, we run the tasks at 1ms
 * intervals, enabling multiple events to be processed
 * per frame, such as haptic feedback pulses.
 */
const uint32_t kVRActiveTaskInterval = 1;  // milliseconds

/**
 * When VR content is inactive, we run the tasks at 100ms
 * intervals, enabling VR display enumeration and
 * presentation startup to be relatively responsive
 * while not consuming unnecessary resources.
 */
const uint32_t kVRIdleTaskInterval = 100;  // milliseconds

/**
 * Max frame duration before the watchdog submits a new one.
 * Probably we can get rid of this when we enforce that SubmitFrame can only be
 * called in a VRDisplay loop.
 */
const double kVRMaxFrameSubmitDuration = 4000.0f;  // milliseconds

static StaticRefPtr<VRManager> sVRManagerSingleton;

/* static */
VRManager* VRManager::Get() {
  MOZ_ASSERT(sVRManagerSingleton != nullptr);

  return sVRManagerSingleton;
}

Atomic<uint32_t> VRManager::sDisplayBase(0);

/* static */
uint32_t VRManager::AllocateDisplayID() { return ++sDisplayBase; }

/*static*/
void VRManager::ManagerInit() {
  MOZ_ASSERT(NS_IsMainThread());

  // Enable gamepad extensions while VR is enabled.
  // Preference only can be set at the Parent process.
  if (StaticPrefs::dom_vr_enabled() && XRE_IsParentProcess()) {
    Preferences::SetBool("dom.gamepad.extensions.enabled", true);
  }

  if (sVRManagerSingleton == nullptr) {
    sVRManagerSingleton = new VRManager();
    ClearOnShutdown(&sVRManagerSingleton);
  }
}

VRManager::VRManager()
    : mState(VRManagerState::Disabled),
      mAccumulator100ms(0.0f),
      mVRDisplaysRequested(false),
      mVRDisplaysRequestedNonFocus(false),
      mVRControllersRequested(false),
      mFrameStarted(false),
      mTaskInterval(0),
      mCurrentSubmitTaskMonitor("CurrentSubmitTaskMonitor"),
      mCurrentSubmitTask(nullptr),
      mLastSubmittedFrameId(0),
      mLastStartedFrame(0),
      mEnumerationCompleted(false),
      mAppPaused(false),
      mShmem(nullptr),
      mHapticPulseRemaining{},
      mDisplayInfo{},
      mLastUpdateDisplayInfo{},
      mBrowserState{},
      mLastSensorState{} {
  MOZ_ASSERT(sVRManagerSingleton == nullptr);
  MOZ_ASSERT(NS_IsMainThread());

#if !defined(MOZ_WIDGET_ANDROID)
  // XRE_IsGPUProcess() is helping us to check some platforms like
  // Win 7 try which are not using GPU process but VR process is enabled.
  mVRProcessEnabled =
      StaticPrefs::dom_vr_process_enabled_AtStartup() && XRE_IsGPUProcess();
  VRServiceHost::Init(mVRProcessEnabled);
  mServiceHost = VRServiceHost::Get();
  // We must shutdown before VRServiceHost, which is cleared
  // on ShutdownPhase::ShutdownFinal, potentially before VRManager.
  // We hold a reference to VRServiceHost to ensure it stays
  // alive until we have shut down.
#else
  // For Android, there is no VRProcess available and no VR service is
  // created, so default to false.
  mVRProcessEnabled = false;
#endif  // !defined(MOZ_WIDGET_ANDROID)

  nsCOMPtr<nsIObserverService> service = services::GetObserverService();
  if (service) {
    service->AddObserver(this, "application-background", false);
    service->AddObserver(this, "application-foreground", false);
  }
}

void VRManager::OpenShmem() {
  if (mShmem == nullptr) {
    mShmem = (new VRShMem(nullptr, mVRProcessEnabled, XRE_IsParentProcess()));
    mShmem->CreateShMem();

#if !defined(MOZ_WIDGET_ANDROID)
    // The VR Service accesses all hardware from a separate process
    // and replaces the other VRManager when enabled.
    // If the VR process is not enabled, create an in-process VRService.
    if (!mVRProcessEnabled) {
      // If the VR process is disabled, attempt to create a
      // VR service within the current process
      mServiceHost->CreateService(mShmem->GetExternalShmem());
      return;
    }
#endif
  } else {
    mShmem->ClearShMem();
  }
}

void VRManager::CloseShmem() {
  if (mShmem != nullptr) {
    mShmem->CloseShMem();
    delete mShmem;
    mShmem = nullptr;
  }
}

VRManager::~VRManager() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mState == VRManagerState::Disabled);

  nsCOMPtr<nsIObserverService> service = services::GetObserverService();
  if (service) {
    service->RemoveObserver(this, "application-background");
    service->RemoveObserver(this, "application-foreground");
  }

#if !defined(MOZ_WIDGET_ANDROID)
  mServiceHost->Shutdown();
#endif
  CloseShmem();
}

void VRManager::AddLayer(VRLayerParent* aLayer) {
  mLayers.AppendElement(aLayer);
  mDisplayInfo.mPresentingGroups |= aLayer->GetGroup();
  if (mLayers.Length() == 1) {
    StartPresentation();
  }

  // Ensure that the content process receives the change immediately
  RefreshVRDisplays();
}

void VRManager::RemoveLayer(VRLayerParent* aLayer) {
  mLayers.RemoveElement(aLayer);
  if (mLayers.Length() == 0) {
    StopPresentation();
  }
  mDisplayInfo.mPresentingGroups = 0;
  for (auto layer : mLayers) {
    mDisplayInfo.mPresentingGroups |= layer->GetGroup();
  }

  // Ensure that the content process receives the change immediately
  RefreshVRDisplays();
}

void VRManager::AddVRManagerParent(VRManagerParent* aVRManagerParent) {
  mVRManagerParents.PutEntry(aVRManagerParent);
}

void VRManager::RemoveVRManagerParent(VRManagerParent* aVRManagerParent) {
  mVRManagerParents.RemoveEntry(aVRManagerParent);
  if (mVRManagerParents.IsEmpty()) {
    Destroy();
  }
}

void VRManager::UpdateRequestedDevices() {
  bool bHaveEventListener = false;
  bool bHaveEventListenerNonFocus = false;
  bool bHaveControllerListener = false;

  for (auto iter = mVRManagerParents.Iter(); !iter.Done(); iter.Next()) {
    VRManagerParent* vmp = iter.Get()->GetKey();
    bHaveEventListener |= vmp->HaveEventListener() && vmp->GetVRActiveStatus();
    bHaveEventListenerNonFocus |=
        vmp->HaveEventListener() && !vmp->GetVRActiveStatus();
    bHaveControllerListener |= vmp->HaveControllerListener();
  }

  mVRDisplaysRequested = bHaveEventListener;
  mVRDisplaysRequestedNonFocus = bHaveEventListenerNonFocus;
  // We only currently allow controllers to be used when
  // also activating a VR display
  mVRControllersRequested = mVRDisplaysRequested && bHaveControllerListener;
}

/**
 * VRManager::NotifyVsync must be called on every 2d vsync (usually at 60hz).
 * This must be called even when no WebVR site is active.
 * If we don't have a 2d display attached to the system, we can call this
 * at the VR display's native refresh rate.
 **/
void VRManager::NotifyVsync(const TimeStamp& aVsyncTimestamp) {
  if (mState != VRManagerState::Active) {
    return;
  }
  /**
   * If the display isn't presenting, refresh the sensors and trigger
   * VRDisplay.requestAnimationFrame at the normal 2d display refresh rate.
   */
  if (mDisplayInfo.mPresentingGroups == 0) {
    StartFrame();
  }
}

void VRManager::StartTasks() {
  if (!mTaskTimer) {
    mTaskInterval = GetOptimalTaskInterval();
    mTaskTimer = NS_NewTimer();
    mTaskTimer->SetTarget(CompositorThreadHolder::Loop()->SerialEventTarget());
    mTaskTimer->InitWithNamedFuncCallback(
        TaskTimerCallback, this, mTaskInterval,
        nsITimer::TYPE_REPEATING_PRECISE_CAN_SKIP,
        "VRManager::TaskTimerCallback");
  }
}

void VRManager::StopTasks() {
  if (mTaskTimer) {
    mTaskTimer->Cancel();
    mTaskTimer = nullptr;
  }
}

/*static*/
void VRManager::TaskTimerCallback(nsITimer* aTimer, void* aClosure) {
  /**
   * It is safe to use the pointer passed in aClosure to reference the
   * VRManager object as the timer is canceled in VRManager::Destroy.
   * VRManager::Destroy set mState to VRManagerState::Disabled, which
   * is asserted in the VRManager destructor, guaranteeing that this
   * functions runs if and only if the VRManager object is valid.
   */
  VRManager* self = static_cast<VRManager*>(aClosure);
  self->RunTasks();

  if (self->mAppPaused) {
    // When the apps goes the background (e.g. Android) we should stop the
    // tasks.
    self->StopTasks();
  }
}

void VRManager::RunTasks() {
  // Will be called once every 1ms when a VR presentation
  // is active or once per vsync when a VR presentation is
  // not active.

  if (mState == VRManagerState::Disabled) {
    // We may have been destroyed but still have messages
    // in the queue from mTaskTimer.  Bail out to avoid
    // running them.
    return;
  }

  TimeStamp now = TimeStamp::Now();
  double lastTickMs = mAccumulator100ms;
  double deltaTime = 0.0f;
  if (!mLastTickTime.IsNull()) {
    deltaTime = (now - mLastTickTime).ToMilliseconds();
  }
  mAccumulator100ms += deltaTime;
  mLastTickTime = now;

  if (deltaTime > 0.0f && floor(mAccumulator100ms) != floor(lastTickMs)) {
    // Even if more than 1 ms has passed, we will only
    // execute Run1msTasks() once.
    Run1msTasks(deltaTime);
  }

  if (floor(mAccumulator100ms * 0.1f) != floor(lastTickMs * 0.1f)) {
    // Even if more than 10 ms has passed, we will only
    // execute Run10msTasks() once.
    Run10msTasks();
  }

  if (mAccumulator100ms >= 100.0f) {
    // Even if more than 100 ms has passed, we will only
    // execute Run100msTasks() once.
    Run100msTasks();
    mAccumulator100ms = fmod(mAccumulator100ms, 100.0f);
  }

  uint32_t optimalTaskInterval = GetOptimalTaskInterval();
  if (mTaskTimer && optimalTaskInterval != mTaskInterval) {
    mTaskTimer->SetDelay(optimalTaskInterval);
    mTaskInterval = optimalTaskInterval;
  }
}

uint32_t VRManager::GetOptimalTaskInterval() {
  /**
   * When either VR content is detected or VR hardware
   * has already been activated, we schedule tasks more
   * frequently.
   */
  bool wantGranularTasks = mVRDisplaysRequested || mVRControllersRequested ||
                           mDisplayInfo.mDisplayID != 0;
  if (wantGranularTasks) {
    return kVRActiveTaskInterval;
  }

  return kVRIdleTaskInterval;
}

/**
 * Run1msTasks() is guaranteed not to be
 * called more than once within 1ms.
 * When VR is not active, this will be
 * called once per VSync if it wasn't
 * called within the last 1ms.
 */
void VRManager::Run1msTasks(double aDeltaTime) { UpdateHaptics(aDeltaTime); }

/**
 * Run10msTasks() is guaranteed not to be
 * called more than once within 10ms.
 * When VR is not active, this will be
 * called once per VSync if it wasn't
 * called within the last 10ms.
 */
void VRManager::Run10msTasks() {
  UpdateRequestedDevices();
  CheckWatchDog();
  ExpireNavigationTransition();
  PullState();
  PushState();
}

/**
 * Run100msTasks() is guaranteed not to be
 * called more than once within 100ms.
 * When VR is not active, this will be
 * called once per VSync if it wasn't
 * called within the last 100ms.
 */
void VRManager::Run100msTasks() {
  // We must continually refresh the VR display enumeration to check
  // for events that we must fire such as Window.onvrdisplayconnect
  // Note that enumeration itself may activate display hardware, such
  // as Oculus, so we only do this when we know we are displaying content
  // that is looking for VR displays.
#if !defined(MOZ_WIDGET_ANDROID)
  mServiceHost->Refresh();
  CheckForPuppetCompletion();
#endif
  RefreshVRDisplays();
  CheckForInactiveTimeout();
  CheckForShutdown();
}

void VRManager::CheckForInactiveTimeout() {
  // Shut down the VR devices when not in use
  if (mVRDisplaysRequested || mVRDisplaysRequestedNonFocus ||
      mVRControllersRequested) {
    // We are using a VR device, keep it alive
    mLastActiveTime = TimeStamp::Now();
  } else if (mLastActiveTime.IsNull()) {
    Shutdown();
  } else {
    TimeDuration duration = TimeStamp::Now() - mLastActiveTime;
    if (duration.ToMilliseconds() > StaticPrefs::dom_vr_inactive_timeout()) {
      Shutdown();
      // We must not throttle the next enumeration request
      // after an idle timeout, as it may result in the
      // user needing to refresh the browser to detect
      // VR hardware when leaving and returning to a VR
      // site.
      mLastDisplayEnumerationTime = TimeStamp();
    }
  }
}

void VRManager::CheckForShutdown() {
  // Check for remote end shutdown
  if (mState != VRManagerState::Disabled && mState != VRManagerState::Idle &&
      mDisplayInfo.mDisplayState.shutdown) {
    Shutdown();
  }
}

#if !defined(MOZ_WIDGET_ANDROID)
void VRManager::CheckForPuppetCompletion() {
  // Notify content process about completion of puppet test resets
  if (mState != VRManagerState::Active) {
    for (auto iter = mManagerParentsWaitingForPuppetReset.Iter(); !iter.Done();
         iter.Next()) {
      Unused << iter.Get()->GetKey()->SendNotifyPuppetResetComplete();
    }
    mManagerParentsWaitingForPuppetReset.Clear();
  }
  // Notify content process about completion of puppet test scripts
  if (mManagerParentRunningPuppet) {
    if (mServiceHost->PuppetHasEnded()) {
      Unused << mManagerParentRunningPuppet
                    ->SendNotifyPuppetCommandBufferCompleted(true);
      mManagerParentRunningPuppet = nullptr;
    }
  }
}
#endif  // !defined(MOZ_WIDGET_ANDROID)

void VRManager::StartFrame() {
  if (mState != VRManagerState::Active) {
    return;
  }
  AUTO_PROFILER_TRACING("VR", "GetSensorState", OTHER);

  /**
   * Do not start more VR frames until the last submitted frame is already
   * processed, or the last has stalled for more than
   * kVRMaxFrameSubmitDuration milliseconds.
   */
  TimeStamp now = TimeStamp::Now();
  const TimeStamp lastFrameStart =
      mLastFrameStart[mDisplayInfo.mFrameId % kVRMaxLatencyFrames];
  const bool isPresenting = mLastUpdateDisplayInfo.GetPresentingGroups() != 0;
  double duration =
      lastFrameStart.IsNull() ? 0.0 : (now - lastFrameStart).ToMilliseconds();
  if (isPresenting && mLastStartedFrame > 0 &&
      mDisplayInfo.mDisplayState.lastSubmittedFrameId < mLastStartedFrame &&
      duration < kVRMaxFrameSubmitDuration) {
    return;
  }

  mDisplayInfo.mFrameId++;
  size_t bufferIndex = mDisplayInfo.mFrameId % kVRMaxLatencyFrames;
  mDisplayInfo.mLastSensorState[bufferIndex] = mLastSensorState;
  mLastFrameStart[bufferIndex] = now;
  mFrameStarted = true;
  mLastStartedFrame = mDisplayInfo.mFrameId;

  DispatchVRDisplayInfoUpdate();
}

void VRManager::EnumerateVRDisplays() {
  if (!StaticPrefs::dom_vr_enabled()) {
    return;
  }

  if (mState == VRManagerState::Disabled) {
    StartTasks();
    mState = VRManagerState::Idle;
  }

  if (mState == VRManagerState::Idle) {
    /**
     * Throttle the rate of enumeration to the interval set in
     * VRDisplayEnumerateInterval
     */
    if (!mLastDisplayEnumerationTime.IsNull()) {
      TimeDuration duration = TimeStamp::Now() - mLastDisplayEnumerationTime;
      if (duration.ToMilliseconds() <
          StaticPrefs::dom_vr_display_enumerate_interval()) {
        return;
      }
    }

    if (!mEarliestRestartTime.IsNull() &&
        mEarliestRestartTime > TimeStamp::Now()) {
      // When the VR Service shuts down it informs us of how long we
      // must wait until we can re-start it.
      // We must wait until mEarliestRestartTime before attempting
      // to enumerate again.
      return;
    }

    /**
     * If we get this far, don't try again until
     * the VRDisplayEnumerateInterval elapses
     */
    mLastDisplayEnumerationTime = TimeStamp::Now();

    OpenShmem();

    /**
     * We must start the VR Service thread
     * and VR Process before enumeration.
     * We don't want to start this until we will
     * actualy enumerate, to avoid continuously
     * re-launching the thread/process when
     * no hardware is found or a VR software update
     * is in progress
     */
#if !defined(MOZ_WIDGET_ANDROID)
    mServiceHost->StartService();
#endif
    if (mShmem) {
      mDisplayInfo.Clear();
      mLastUpdateDisplayInfo.Clear();
      mFrameStarted = false;
      mBrowserState.Clear();
      mLastSensorState.Clear();
      mDisplayInfo.mGroupMask = kVRGroupContent;
      // We must block until enumeration has completed in order
      // to signal that the WebVR promise should be resolved at the
      // right time.
#if defined(MOZ_WIDGET_ANDROID)
      // In Android, we need to make sure calling
      // GeckoVRManager::SetExternalContext() from an external VR service
      // before doing enumeration.
      if (mShmem->GetExternalShmem()) {
        mState = VRManagerState::Enumeration;
      }
#else
      mState = VRManagerState::Enumeration;
#endif  // MOZ_WIDGET_ANDROID
    }
  }  // if (mState == VRManagerState::Idle)

  if (mState == VRManagerState::Enumeration) {
    MOZ_ASSERT(mShmem != nullptr);

    PullState();
    if (mEnumerationCompleted) {
      if (mDisplayInfo.mDisplayState.isConnected) {
        mDisplayInfo.mDisplayID = VRManager::AllocateDisplayID();
        mState = VRManagerState::Active;
      } else {
        Shutdown();
      }
    }
  }  // if (mState == VRManagerState::Enumeration)
}

void VRManager::RefreshVRDisplays(bool aMustDispatch) {
  uint32_t previousDisplayID = mDisplayInfo.GetDisplayID();

  /**
   * If we aren't viewing WebVR content, don't enumerate
   * new hardware, as it will cause some devices to power on
   * or interrupt other VR activities.
   */
  if (mVRDisplaysRequested || aMustDispatch) {
    EnumerateVRDisplays();
  }
  if (mState == VRManagerState::Enumeration) {
    // If we are enumerating VR Displays, do not dispatch
    // updates until the enumeration has completed.
    return;
  }
  bool changed = false;

  if (previousDisplayID != mDisplayInfo.GetDisplayID()) {
    changed = true;
  }

  if (mState == VRManagerState::Active &&
      mDisplayInfo != mLastUpdateDisplayInfo) {
    // This display's info has changed
    changed = true;
  }

  if (changed || aMustDispatch) {
    DispatchVRDisplayInfoUpdate();
  }
}

void VRManager::DispatchVRDisplayInfoUpdate() {
  // This could be simplified further by only supporting one display
  nsTArray<VRDisplayInfo> displayUpdates;
  if (mState == VRManagerState::Active) {
    MOZ_ASSERT(mDisplayInfo.mDisplayID != 0);
    displayUpdates.AppendElement(mDisplayInfo);
  }
  for (auto iter = mVRManagerParents.Iter(); !iter.Done(); iter.Next()) {
    Unused << iter.Get()->GetKey()->SendUpdateDisplayInfo(displayUpdates);
  }
  mLastUpdateDisplayInfo = mDisplayInfo;
}

void VRManager::StopAllHaptics() {
  for (size_t i = 0; i < mozilla::ArrayLength(mBrowserState.hapticState); i++) {
    ClearHapticSlot(i);
  }
  PushState();
}

void VRManager::VibrateHaptic(uint32_t aControllerIdx, uint32_t aHapticIndex,
                              double aIntensity, double aDuration,
                              const VRManagerPromise& aPromise)

{
  if (mState != VRManagerState::Active) {
    return;
  }
  // VRDisplayClient::FireGamepadEvents() assigns a controller ID with
  // ranges based on displayID.  We must translate this to the indexes
  // understood by VRDisplayExternal.
  uint32_t controllerBaseIndex =
      kVRControllerMaxCount * mDisplayInfo.mDisplayID;
  uint32_t controllerIndex = aControllerIdx - controllerBaseIndex;

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
    if (state.controllerIndex == controllerIndex &&
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
  bestSlot.controllerIndex = controllerIndex;
  bestSlot.hapticIndex = aHapticIndex;
  bestSlot.pulseStart = (float)(now - mLastFrameStart[bufferIndex]).ToSeconds();
  bestSlot.pulseDuration =
      (float)aDuration * 0.001f;  // Convert from ms to seconds
  bestSlot.pulseIntensity = (float)aIntensity;

  mHapticPulseRemaining[bestSlotIndex] = aDuration;
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

void VRManager::StopVibrateHaptic(uint32_t aControllerIdx) {
  if (mState != VRManagerState::Active) {
    return;
  }
  // VRDisplayClient::FireGamepadEvents() assigns a controller ID with
  // ranges based on displayID.  We must translate this to the indexes
  // understood by VRDisplayExternal.
  uint32_t controllerBaseIndex =
      kVRControllerMaxCount * mDisplayInfo.mDisplayID;
  uint32_t controllerIndex = aControllerIdx - controllerBaseIndex;

  for (size_t i = 0; i < mozilla::ArrayLength(mBrowserState.hapticState); i++) {
    VRHapticState& state = mBrowserState.hapticState[i];
    if (state.controllerIndex == controllerIndex) {
      memset(&state, 0, sizeof(VRHapticState));
    }
  }
  PushState();
}

void VRManager::NotifyVibrateHapticCompleted(const VRManagerPromise& aPromise) {
  aPromise.mParent->SendReplyGamepadVibrateHaptic(aPromise.mPromiseID);
}

void VRManager::StartVRNavigation(const uint32_t& aDisplayID) {
  if (mState != VRManagerState::Active) {
    return;
  }
  /**
   * We only support a single VRSession with a single VR display at a
   * time; however, due to the asynchronous nature of the API, it's possible
   * that the previously used VR display was a different one than the one now
   * allocated. We catch these cases to avoid automatically activating the new
   * VR displays. This situation is expected to be very rare and possibly never
   * seen. Perhaps further simplification could be made in the content process
   * code which passes around displayID's that may no longer be needed.
   **/
  if (mDisplayInfo.GetDisplayID() != aDisplayID) {
    return;
  }
  mBrowserState.navigationTransitionActive = true;
  mVRNavigationTransitionEnd = TimeStamp();
  PushState();
}

void VRManager::StopVRNavigation(const uint32_t& aDisplayID,
                                 const TimeDuration& aTimeout) {
  if (mState != VRManagerState::Active) {
    return;
  }
  if (mDisplayInfo.GetDisplayID() != aDisplayID) {
    return;
  }
  if (aTimeout.ToMilliseconds() <= 0) {
    mBrowserState.navigationTransitionActive = false;
    mVRNavigationTransitionEnd = TimeStamp();
    PushState();
  }
  mVRNavigationTransitionEnd = TimeStamp::Now() + aTimeout;
}

#if !defined(MOZ_WIDGET_ANDROID)

bool VRManager::RunPuppet(const nsTArray<uint64_t>& aBuffer,
                          VRManagerParent* aManagerParent) {
  if (!StaticPrefs::dom_vr_puppet_enabled()) {
    // Sanity check to ensure that a compromised content process
    // can't use this to escalate permissions.
    return false;
  }
  if (mManagerParentRunningPuppet != nullptr) {
    // Only one parent may run a puppet at a time
    return false;
  }
  mManagerParentRunningPuppet = aManagerParent;
  mServiceHost->PuppetSubmit(aBuffer);
  return true;
}

void VRManager::ResetPuppet(VRManagerParent* aManagerParent) {
  mManagerParentsWaitingForPuppetReset.PutEntry(aManagerParent);
  if (mManagerParentRunningPuppet != nullptr) {
    Unused << mManagerParentRunningPuppet
                  ->SendNotifyPuppetCommandBufferCompleted(false);
    mManagerParentRunningPuppet = nullptr;
  }
  mServiceHost->PuppetReset();
  // In the event that we are shut down, the task timer won't be running
  // to trigger CheckForPuppetCompletion.
  // In this case, CheckForPuppetCompletion() would immediately resolve
  // the promises for mManagerParentsWaitingForPuppetReset.
  // We can simply call it once here to handle that case.
  CheckForPuppetCompletion();
}

#endif  // !defined(MOZ_WIDGET_ANDROID)

void VRManager::PullState(
    const std::function<bool()>& aWaitCondition /* = nullptr */) {
  if (mShmem != nullptr) {
    mShmem->PullSystemState(mDisplayInfo.mDisplayState, mLastSensorState,
                            mDisplayInfo.mControllerState,
                            mEnumerationCompleted, aWaitCondition);
  }
}

void VRManager::PushState(bool aNotifyCond) {
  if (mShmem != nullptr) {
    mShmem->PushBrowserState(mBrowserState, aNotifyCond);
  }
}

void VRManager::Destroy() {
  if (mState == VRManagerState::Disabled) {
    return;
  }
  Shutdown();
  StopTasks();
  mState = VRManagerState::Disabled;
}

void VRManager::Shutdown() {
  if (mState == VRManagerState::Disabled || mState == VRManagerState::Idle) {
    return;
  }

  if (mDisplayInfo.mDisplayState.shutdown) {
    // Shutdown was requested by VR Service, so we must throttle
    // as requested by the VR Service
    TimeStamp now = TimeStamp::Now();
    mEarliestRestartTime =
        now + TimeDuration::FromMilliseconds(
                  (double)mDisplayInfo.mDisplayState.minRestartInterval);
  }

  StopAllHaptics();
  StopPresentation();
  CancelCurrentSubmitTask();
  ShutdownSubmitThread();
  mDisplayInfo.Clear();
  mEnumerationCompleted = false;

  if (mState == VRManagerState::Enumeration) {
    // Ensure that enumeration promises are resolved
    DispatchVRDisplayInfoUpdate();
  }

#if !defined(MOZ_WIDGET_ANDROID)
  mServiceHost->StopService();
#endif
  mState = VRManagerState::Idle;

  // We will close Shmem in the DTOR to avoid
  // mSubmitThread is still running but its shmem
  // has been released.
}

void VRManager::CheckWatchDog() {
  /**
   * We will trigger a new frame immediately after a successful frame
   * texture submission.  If content fails to call VRDisplay.submitFrame
   * after dom.vr.display.rafMaxDuration milliseconds has elapsed since the
   * last VRDisplay.requestAnimationFrame, we act as a "watchdog" and
   * kick-off a new VRDisplay.requestAnimationFrame to avoid a render loop
   * stall and to give content a chance to recover.
   *
   * If the lower level VR platform API's are rejecting submitted frames,
   * such as when the Oculus "Health and Safety Warning" is displayed,
   * we will not kick off the next frame immediately after
   * VRDisplay.submitFrame as it would result in an unthrottled render loop
   * that would free run at potentially extreme frame rates.  To ensure that
   * content has a chance to resume its presentation when the frames are
   * accepted once again, we rely on this "watchdog" to act as a VR refresh
   * driver cycling at a rate defined by dom.vr.display.rafMaxDuration.
   *
   * This number must be larger than the slowest expected frame time during
   * normal VR presentation, but small enough not to break content that
   * makes assumptions of reasonably minimal VSync rate.
   *
   * The slowest expected refresh rate for a VR display currently is an
   * Oculus CV1 when ASW (Asynchronous Space Warp) is enabled, at 45hz.
   * A dom.vr.display.rafMaxDuration value of 50 milliseconds results in a
   * 20hz rate, which avoids inadvertent triggering of the watchdog during
   * Oculus ASW even if every second frame is dropped.
   */
  if (mState != VRManagerState::Active) {
    return;
  }
  bool bShouldStartFrame = false;

  // If content fails to call VRDisplay.submitFrame, we must eventually
  // time-out and trigger a new frame.
  TimeStamp lastFrameStart =
      mLastFrameStart[mDisplayInfo.mFrameId % kVRMaxLatencyFrames];
  if (lastFrameStart.IsNull()) {
    bShouldStartFrame = true;
  } else {
    TimeDuration duration = TimeStamp::Now() - lastFrameStart;
    if (duration.ToMilliseconds() >
        StaticPrefs::dom_vr_display_rafMaxDuration()) {
      bShouldStartFrame = true;
    }
  }

  if (bShouldStartFrame) {
    StartFrame();
  }
}

void VRManager::ExpireNavigationTransition() {
  if (mState != VRManagerState::Active) {
    return;
  }
  if (!mVRNavigationTransitionEnd.IsNull() &&
      TimeStamp::Now() > mVRNavigationTransitionEnd) {
    mBrowserState.navigationTransitionActive = false;
  }
}

void VRManager::UpdateHaptics(double aDeltaTime) {
  if (mState != VRManagerState::Active) {
    return;
  }
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

void VRManager::ClearHapticSlot(size_t aSlot) {
  MOZ_ASSERT(aSlot < mozilla::ArrayLength(mBrowserState.hapticState));
  memset(&mBrowserState.hapticState[aSlot], 0, sizeof(VRHapticState));
  mHapticPulseRemaining[aSlot] = 0.0f;
  if (aSlot < mHapticPromises.Length() && mHapticPromises[aSlot]) {
    NotifyVibrateHapticCompleted(*(mHapticPromises[aSlot]));
    mHapticPromises[aSlot] = nullptr;
  }
}

void VRManager::ShutdownSubmitThread() {
  if (mSubmitThread) {
    mSubmitThread->Shutdown();
    mSubmitThread = nullptr;
  }
}

void VRManager::StartPresentation() {
  if (mState != VRManagerState::Active) {
    return;
  }
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

  mLastSubmittedFrameId = 0;
  mLastStartedFrame = 0;
}

void VRManager::StopPresentation() {
  if (mState != VRManagerState::Active) {
    return;
  }
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
        (uint32_t)((double)(mDisplayInfo.mDisplayState.droppedFrameCount -
                            mTelemetry.mLastDroppedFrameCount) /
                   duration.ToSeconds());
    Telemetry::Accumulate(droppedFramesID, droppedFramesPerSec);
  }
}

bool VRManager::IsPresenting() {
  if (mShmem) {
    return mDisplayInfo.mPresentingGroups != 0;
  }
  return false;
}

void VRManager::SetGroupMask(uint32_t aGroupMask) {
  if (mState != VRManagerState::Active) {
    return;
  }
  mDisplayInfo.mGroupMask = aGroupMask;
}

void VRManager::SubmitFrame(VRLayerParent* aLayer,
                            const layers::SurfaceDescriptor& aTexture,
                            uint64_t aFrameId, const gfx::Rect& aLeftEyeRect,
                            const gfx::Rect& aRightEyeRect) {
  if (mState != VRManagerState::Active) {
    return;
  }
  MonitorAutoLock lock(mCurrentSubmitTaskMonitor);
  if ((mDisplayInfo.mGroupMask & aLayer->GetGroup()) == 0) {
    // Suppress layers hidden by the group mask
    return;
  }

  // Ensure that we only accept the first SubmitFrame call per RAF cycle.
  if (!mFrameStarted || aFrameId != mDisplayInfo.mFrameId) {
    return;
  }

  /**
   * Do not queue more submit frames until the last submitted frame is
   * already processed and the new WebGL texture is ready.
   */
  if (mLastSubmittedFrameId > 0 &&
      mLastSubmittedFrameId !=
          mDisplayInfo.mDisplayState.lastSubmittedFrameId) {
    mLastStartedFrame = 0;
    return;
  }

  mLastSubmittedFrameId = aFrameId;

  mFrameStarted = false;

  RefPtr<CancelableRunnable> task = NewCancelableRunnableMethod<
      StoreCopyPassByConstLRef<layers::SurfaceDescriptor>, uint64_t,
      StoreCopyPassByConstLRef<gfx::Rect>, StoreCopyPassByConstLRef<gfx::Rect>>(
      "gfx::VRManager::SubmitFrameInternal", this,
      &VRManager::SubmitFrameInternal, aTexture, aFrameId, aLeftEyeRect,
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

bool VRManager::SubmitFrame(const layers::SurfaceDescriptor& aTexture,
                            uint64_t aFrameId, const gfx::Rect& aLeftEyeRect,
                            const gfx::Rect& aRightEyeRect) {
  if (mState != VRManagerState::Active) {
    return false;
  }
#if defined(XP_WIN) || defined(XP_MACOSX) || defined(MOZ_WIDGET_ANDROID)
  MOZ_ASSERT(mBrowserState.layerState[0].type ==
             VRLayerType::LayerType_Stereo_Immersive);
  VRLayer_Stereo_Immersive& layer =
      mBrowserState.layerState[0].layer_stereo_immersive;

  switch (aTexture.type()) {
#  if defined(XP_WIN)
    case SurfaceDescriptor::TSurfaceDescriptorD3D10: {
      const SurfaceDescriptorD3D10& surf =
          aTexture.get_SurfaceDescriptorD3D10();
      layer.textureType =
          VRLayerTextureType::LayerTextureType_D3D10SurfaceDescriptor;
      layer.textureHandle = (void*)surf.handle();
      layer.textureSize.width = surf.size().width;
      layer.textureSize.height = surf.size().height;
    } break;
#  elif defined(XP_MACOSX)
    case SurfaceDescriptor::TSurfaceDescriptorMacIOSurface: {
      // MacIOSurface ptr can't be fetched or used at different threads.
      // Both of fetching and using this MacIOSurface are at the VRService
      // thread.
      const auto& desc = aTexture.get_SurfaceDescriptorMacIOSurface();
      layer.textureType = VRLayerTextureType::LayerTextureType_MacIOSurface;
      layer.textureHandle = desc.surfaceId();
      RefPtr<MacIOSurface> surf =
          MacIOSurface::LookupSurface(desc.surfaceId(), desc.scaleFactor(),
                                      !desc.isOpaque(), desc.yUVColorSpace());
      if (surf) {
        layer.textureSize.width = surf->GetDevicePixelWidth();
        layer.textureSize.height = surf->GetDevicePixelHeight();
      }
    } break;
#  elif defined(MOZ_WIDGET_ANDROID)
    case SurfaceDescriptor::TSurfaceTextureDescriptor: {
      const SurfaceTextureDescriptor& desc =
          aTexture.get_SurfaceTextureDescriptor();
      java::GeckoSurfaceTexture::LocalRef surfaceTexture =
          java::GeckoSurfaceTexture::Lookup(desc.handle());
      if (!surfaceTexture) {
        NS_WARNING("VRManager::SubmitFrame failed to get a SurfaceTexture");
        return false;
      }
      layer.textureType =
          VRLayerTextureType::LayerTextureType_GeckoSurfaceTexture;
      layer.textureHandle = desc.handle();
      layer.textureSize.width = desc.size().width;
      layer.textureSize.height = desc.size().height;
    } break;
#  endif
    default: {
      MOZ_ASSERT(false);
      return false;
    }
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

  PullState([&]() {
    return (mDisplayInfo.mDisplayState.lastSubmittedFrameId >= aFrameId) ||
           mDisplayInfo.mDisplayState.suppressFrames ||
           !mDisplayInfo.mDisplayState.isConnected;
  });

  if (mDisplayInfo.mDisplayState.suppressFrames ||
      !mDisplayInfo.mDisplayState.isConnected) {
    // External implementation wants to supress frames, service has shut
    // down or hardware has been disconnected.
    return false;
  }

  return mDisplayInfo.mDisplayState.lastSubmittedFrameSuccessful;
#else
  MOZ_ASSERT(false);  // Not implmented for this platform
  return false;
#endif
}

void VRManager::SubmitFrameInternal(const layers::SurfaceDescriptor& aTexture,
                                    uint64_t aFrameId,
                                    const gfx::Rect& aLeftEyeRect,
                                    const gfx::Rect& aRightEyeRect) {
#if !defined(MOZ_WIDGET_ANDROID)
  MOZ_ASSERT(mSubmitThread->GetThread() == NS_GetCurrentThread());
#endif  // !defined(MOZ_WIDGET_ANDROID)
  AUTO_PROFILER_TRACING("VR", "SubmitFrameAtVRDisplayExternal", OTHER);

  {  // scope lock
    MonitorAutoLock lock(mCurrentSubmitTaskMonitor);

    if (!SubmitFrame(aTexture, aFrameId, aLeftEyeRect, aRightEyeRect)) {
      mCurrentSubmitTask = nullptr;
      return;
    }
    mCurrentSubmitTask = nullptr;
  }

#if defined(XP_WIN) || defined(XP_MACOSX)

  /**
   * Trigger the next VSync immediately after we are successfully
   * submitting frames.  As SubmitFrame is responsible for throttling
   * the render loop, if we don't successfully call it, we shouldn't trigger
   * StartFrame immediately, as it will run unbounded.
   * If StartFrame is not called here due to SubmitFrame failing, the
   * fallback "watchdog" code in VRManager::NotifyVSync() will cause
   * frames to continue at a lower refresh rate until frame submission
   * succeeds again.
   */
  MessageLoop* loop = CompositorThreadHolder::Loop();

  loop->PostTask(NewRunnableMethod("gfx::VRManager::StartFrame", this,
                                   &VRManager::StartFrame));
#elif defined(MOZ_WIDGET_ANDROID)
  // We are already in the CompositorThreadHolder event loop on Android.
  StartFrame();
#endif
}

void VRManager::CancelCurrentSubmitTask() {
  MonitorAutoLock lock(mCurrentSubmitTaskMonitor);
  if (mCurrentSubmitTask) {
    mCurrentSubmitTask->Cancel();
    mCurrentSubmitTask = nullptr;
  }
}

//-----------------------------------------------------------------------------
// VRManager::nsIObserver
//-----------------------------------------------------------------------------

NS_IMETHODIMP
VRManager::Observe(nsISupports* subject, const char* topic,
                   const char16_t* data) {
  if (!strcmp(topic, "application-background")) {
    // StopTasks() is called later in the timer thread based on this flag to
    // avoid threading issues.
    mAppPaused = true;
  } else if (!strcmp(topic, "application-foreground") && mAppPaused) {
    mAppPaused = false;
    // When the apps goes the foreground (e.g. Android) we should restart the
    // tasks.
    StartTasks();
  }
  return NS_OK;
}

NS_IMPL_ISUPPORTS(VRManager, nsIObserver)

}  // namespace gfx
}  // namespace mozilla
