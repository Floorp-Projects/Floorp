/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VRManager.h"
#include "VRManagerParent.h"
#include "VRThread.h"
#include "gfxVR.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/dom/VRDisplay.h"
#include "mozilla/dom/GamepadEventTypes.h"
#include "mozilla/layers/TextureHost.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/Unused.h"

#include "gfxPrefs.h"
#include "gfxVR.h"
#include "gfxVRExternal.h"

#include "gfxVRPuppet.h"
#include "ipc/VRLayerParent.h"
#if !defined(MOZ_WIDGET_ANDROID)
#  include "service/VRService.h"
#  include "service/VRServiceManager.h"
#endif

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::layers;
using namespace mozilla::gl;

namespace mozilla {
namespace gfx {

static StaticRefPtr<VRManager> sVRManagerSingleton;

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

/*static*/
void VRManager::ManagerInit() {
  MOZ_ASSERT(NS_IsMainThread());

  // TODO: We should make VRManager::ManagerInit
  // be called when entering VR content pages.
  if (sVRManagerSingleton == nullptr) {
    sVRManagerSingleton = new VRManager();
    ClearOnShutdown(&sVRManagerSingleton);
  }
}

VRManager::VRManager()
    : mInitialized(false),
      mAccumulator100ms(0.0f),
      mVRDisplaysRequested(false),
      mVRDisplaysRequestedNonFocus(false),
      mVRControllersRequested(false),
      mVRServiceStarted(false),
      mTaskInterval(0) {
  MOZ_COUNT_CTOR(VRManager);
  MOZ_ASSERT(sVRManagerSingleton == nullptr);

  RefPtr<VRSystemManager> mgr;

#if !defined(MOZ_WIDGET_ANDROID)
  // The VR Service accesses all hardware from a separate process
  // and replaces the other VRSystemManager when enabled.
  if (!gfxPrefs::VRProcessEnabled() || !XRE_IsGPUProcess()) {
    VRServiceManager::Get().CreateService();
  }
  if (VRServiceManager::Get().IsServiceValid()) {
    mExternalManager =
        VRSystemManagerExternal::Create(VRServiceManager::Get().GetAPIShmem());
  }
  if (mExternalManager) {
    mManagers.AppendElement(mExternalManager);
  }
#endif

  if (!mExternalManager) {
    mExternalManager = VRSystemManagerExternal::Create();
    if (mExternalManager) {
      mManagers.AppendElement(mExternalManager);
    }
  }

  // Enable gamepad extensions while VR is enabled.
  // Preference only can be set at the Parent process.
  if (XRE_IsParentProcess() && gfxPrefs::VREnabled()) {
    Preferences::SetBool("dom.gamepad.extensions.enabled", true);
  }
}

VRManager::~VRManager() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mInitialized);
#if !defined(MOZ_WIDGET_ANDROID)
  if (VRServiceManager::Get().IsServiceValid()) {
    VRServiceManager::Get().Shutdown();
  }
#endif
  MOZ_COUNT_DTOR(VRManager);
}

void VRManager::Destroy() {
  StopTasks();
  mVRDisplayIDs.Clear();
  mVRControllerIDs.Clear();
  for (uint32_t i = 0; i < mManagers.Length(); ++i) {
    mManagers[i]->Destroy();
  }
#if !defined(MOZ_WIDGET_ANDROID)
  if (VRServiceManager::Get().IsServiceValid()) {
    VRServiceManager::Get().Shutdown();
  }
#endif
  Shutdown();
  mInitialized = false;
}

void VRManager::Shutdown() {
  mVRDisplayIDs.Clear();
  mVRControllerIDs.Clear();
  for (uint32_t i = 0; i < mManagers.Length(); ++i) {
    mManagers[i]->Shutdown();
  }
#if !defined(MOZ_WIDGET_ANDROID)
  if (VRServiceManager::Get().IsServiceValid()) {
    VRServiceManager::Get().Stop();
  }
  // XRE_IsGPUProcess() is helping us to check some platforms like
  // Win 7 try which are not using GPU process but VR process is enabled.
  if (XRE_IsGPUProcess() && gfxPrefs::VRProcessEnabled() && mVRServiceStarted) {
    RefPtr<Runnable> task = NS_NewRunnableFunction(
        "VRServiceManager::ShutdownVRProcess",
        []() -> void { VRServiceManager::Get().ShutdownVRProcess(); });
    NS_DispatchToMainThread(task.forget());
  }
#endif
  mVRServiceStarted = false;
}

void VRManager::Init() { mInitialized = true; }

/* static */
VRManager* VRManager::Get() {
  MOZ_ASSERT(sVRManagerSingleton != nullptr);

  return sVRManagerSingleton;
}

void VRManager::AddVRManagerParent(VRManagerParent* aVRManagerParent) {
  if (mVRManagerParents.IsEmpty()) {
    Init();
  }
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
  for (const auto& manager : mManagers) {
    manager->NotifyVSync();
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
   * VRManager::Destroy set mInitialized to false, which is asserted
   * in the VRManager destructor, guaranteeing that this functions
   * runs if and only if the VRManager object is valid.
   */
  VRManager* self = static_cast<VRManager*>(aClosure);
  self->RunTasks();
}

void VRManager::RunTasks() {
  // Will be called once every 1ms when a VR presentation
  // is active or once per vsync when a VR presentation is
  // not active.

  if (!mInitialized) {
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
                           mVRDisplayIDs.Length() || mVRControllerIDs.Length();
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
void VRManager::Run1msTasks(double aDeltaTime) {
  for (const auto& manager : mManagers) {
    manager->Run1msTasks(aDeltaTime);
  }

  for (const auto& displayID : mVRDisplayIDs) {
    RefPtr<VRDisplayHost> display(GetDisplay(displayID));
    if (display) {
      display->Run1msTasks(aDeltaTime);
    }
  }
}

/**
 * Run10msTasks() is guaranteed not to be
 * called more than once within 10ms.
 * When VR is not active, this will be
 * called once per VSync if it wasn't
 * called within the last 10ms.
 */
void VRManager::Run10msTasks() {
  UpdateRequestedDevices();

  for (const auto& manager : mManagers) {
    manager->Run10msTasks();
  }

  for (const auto& displayID : mVRDisplayIDs) {
    RefPtr<VRDisplayHost> display(GetDisplay(displayID));
    if (display) {
      display->Run10msTasks();
    }
  }
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
  RefreshVRDisplays();

  // Update state and enumeration of VR controllers
  RefreshVRControllers();

  CheckForInactiveTimeout();

  for (const auto& manager : mManagers) {
    manager->Run100msTasks();
  }

  for (const auto& displayID : mVRDisplayIDs) {
    RefPtr<VRDisplayHost> display(GetDisplay(displayID));
    if (display) {
      display->Run100msTasks();
    }
  }
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
    if (duration.ToMilliseconds() > gfxPrefs::VRInactiveTimeout()) {
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

void VRManager::NotifyVRVsync(const uint32_t& aDisplayID) {
  for (const auto& manager : mManagers) {
    if (manager->GetIsPresenting()) {
      manager->HandleInput();
    }
  }

  RefPtr<VRDisplayHost> display = GetDisplay(aDisplayID);
  if (display) {
    display->StartFrame();
  }

  DispatchVRDisplayInfoUpdate();
}

void VRManager::EnumerateVRDisplays() {
  StartTasks();
  /**
   * Throttle the rate of enumeration to the interval set in
   * VRDisplayEnumerateInterval
   */
  if (!mLastDisplayEnumerationTime.IsNull()) {
    TimeDuration duration = TimeStamp::Now() - mLastDisplayEnumerationTime;
    if (duration.ToMilliseconds() < gfxPrefs::VRDisplayEnumerateInterval()) {
      return;
    }
  }

  /**
   * Any VRSystemManager instance may request that no enumeration
   * should occur, including enumeration from other VRSystemManager
   * instances.
   */
  for (const auto& manager : mManagers) {
    if (manager->ShouldInhibitEnumeration()) {
      return;
    }
  }

  /**
   * If we get this far, don't try again until
   * the VRDisplayEnumerateInterval elapses
   */
  mLastDisplayEnumerationTime = TimeStamp::Now();

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
  if (!mVRServiceStarted) {
    if (XRE_IsGPUProcess() && gfxPrefs::VRProcessEnabled()) {
      VRServiceManager::Get().CreateVRProcess();
      mVRServiceStarted = true;
    } else {
      if (VRServiceManager::Get().IsServiceValid()) {
        VRServiceManager::Get().Start();
        mVRServiceStarted = true;
      }
    }
  }
#endif

  /**
   * VRSystemManagers are inserted into mManagers in
   * a strict order of priority.  The managers for the
   * most device-specialized API's will have a chance
   * to enumerate devices before the more generic
   * device-agnostic APIs.
   */
  for (const auto& manager : mManagers) {
    manager->Enumerate();
    /**
     * After a VRSystemManager::Enumerate is called, it may request
     * that further enumeration should stop.  This can be used to prevent
     * erraneous redundant enumeration of the same HMD by multiple managers.
     * XXX - Perhaps there will be a better way to detect duplicate displays
     * in the future.
     */
    if (manager->ShouldInhibitEnumeration()) {
      return;
    }
  }

  nsTArray<RefPtr<gfx::VRDisplayHost>> displays;
  for (const auto& manager : mManagers) {
    manager->GetHMDs(displays);
  }

  mVRDisplayIDs.Clear();
  for (const auto& display : displays) {
    mVRDisplayIDs.AppendElement(display->GetDisplayInfo().GetDisplayID());
  }

  nsTArray<RefPtr<gfx::VRControllerHost>> controllers;
  for (const auto& manager : mManagers) {
    manager->GetControllers(controllers);
  }

  mVRControllerIDs.Clear();
  for (const auto& controller : controllers) {
    mVRControllerIDs.AppendElement(
        controller->GetControllerInfo().GetControllerID());
  }
}

void VRManager::RefreshVRDisplays(bool aMustDispatch) {
  /**
   * If we aren't viewing WebVR content, don't enumerate
   * new hardware, as it will cause some devices to power on
   * or interrupt other VR activities.
   */
  if (mVRDisplaysRequested || aMustDispatch) {
    EnumerateVRDisplays();
  }
#if !defined(MOZ_WIDGET_ANDROID)
  VRServiceManager::Get().Refresh();
#endif

  /**
   * VRSystemManager::GetHMDs will not activate new hardware
   * or result in interruption of other VR activities.
   * We can call it even when suppressing enumeration to get
   * the already-enumerated displays.
   */
  nsTArray<RefPtr<gfx::VRDisplayHost>> displays;
  for (const auto& manager : mManagers) {
    manager->GetHMDs(displays);
  }

  bool displayInfoChanged = false;
  bool displaySetChanged = false;

  if (displays.Length() != mVRDisplayIDs.Length()) {
    // Catch cases where a VR display has been removed
    displaySetChanged = true;
  }

  for (const auto& display : displays) {
    if (!GetDisplay(display->GetDisplayInfo().GetDisplayID())) {
      // This is a new display
      displaySetChanged = true;
      break;
    }

    if (display->CheckClearDisplayInfoDirty()) {
      // This display's info has changed
      displayInfoChanged = true;
      break;
    }
  }

  // Rebuild the HashMap if there are additions or removals
  if (displaySetChanged) {
    mVRDisplayIDs.Clear();
    for (const auto& display : displays) {
      mVRDisplayIDs.AppendElement(display->GetDisplayInfo().GetDisplayID());
    }
  }

  if (displayInfoChanged || displaySetChanged || aMustDispatch) {
    DispatchVRDisplayInfoUpdate();
  }
}

void VRManager::DispatchVRDisplayInfoUpdate() {
  nsTArray<VRDisplayInfo> update;
  GetVRDisplayInfo(update);

  for (auto iter = mVRManagerParents.Iter(); !iter.Done(); iter.Next()) {
    Unused << iter.Get()->GetKey()->SendUpdateDisplayInfo(update);
  }
}

/**
 * Get any VR displays that have already been enumerated without
 * activating any new devices.
 */
void VRManager::GetVRDisplayInfo(nsTArray<VRDisplayInfo>& aDisplayInfo) {
  aDisplayInfo.Clear();
  for (const auto& displayID : mVRDisplayIDs) {
    RefPtr<VRDisplayHost> display(GetDisplay(displayID));
    if (display) {
      aDisplayInfo.AppendElement(display->GetDisplayInfo());
    }
  }
}

// Verify aDisplayID matches the exisiting displayID from mVRDisplayIDs list,
// then using the exiting displayID to get VRDisplayHost from VRManagers.
RefPtr<gfx::VRDisplayHost> VRManager::GetDisplay(const uint32_t& aDisplayID) {
  bool found = false;
  for (const auto& displayID : mVRDisplayIDs) {
    if (displayID == aDisplayID) {
      found = true;
      break;
    }
  }

  if (found) {
    nsTArray<RefPtr<gfx::VRDisplayHost>> displays;
    for (const auto& manager : mManagers) {
      manager->GetHMDs(displays);
      for (const auto& display : displays) {
        if (display->GetDisplayInfo().GetDisplayID() == aDisplayID) {
          return display;
        }
      }
    }
  }

  return nullptr;
}

// Verify aControllerID matches the exisiting controllerID from mVRControllerIDs
// list, then using the exiting controllerID to get VRControllerHost from
// VRManagers.
RefPtr<gfx::VRControllerHost> VRManager::GetController(
    const uint32_t& aControllerID) {
  bool found = false;
  for (const auto& controllerID : mVRControllerIDs) {
    if (controllerID == aControllerID) {
      found = true;
      break;
    }
  }

  if (found) {
    nsTArray<RefPtr<gfx::VRControllerHost>> controllers;
    for (const auto& manager : mManagers) {
      manager->GetControllers(controllers);
      for (const auto& controller : controllers) {
        if (controller->GetControllerInfo().GetControllerID() ==
            aControllerID) {
          return controller;
        }
      }
    }
  }

  return nullptr;
}

void VRManager::GetVRControllerInfo(
    nsTArray<VRControllerInfo>& aControllerInfo) {
  aControllerInfo.Clear();
  for (const auto& controllerID : mVRControllerIDs) {
    RefPtr<VRControllerHost> controller(GetController(controllerID));
    if (controller) {
      aControllerInfo.AppendElement(controller->GetControllerInfo());
    }
  }
}

void VRManager::RefreshVRControllers() {
  ScanForControllers();

  nsTArray<RefPtr<gfx::VRControllerHost>> controllers;

  for (uint32_t i = 0; i < mManagers.Length() && controllers.Length() == 0;
       ++i) {
    mManagers[i]->GetControllers(controllers);
  }

  bool controllerInfoChanged = false;

  if (controllers.Length() != mVRControllerIDs.Length()) {
    // Catch cases where VR controllers has been removed
    controllerInfoChanged = true;
  }

  for (const auto& controller : controllers) {
    if (!GetController(controller->GetControllerInfo().GetControllerID())) {
      // This is a new controller
      controllerInfoChanged = true;
      break;
    }
  }

  if (controllerInfoChanged) {
    mVRControllerIDs.Clear();
    for (const auto& controller : controllers) {
      mVRControllerIDs.AppendElement(
          controller->GetControllerInfo().GetControllerID());
    }
  }
}

void VRManager::ScanForControllers() {
  // We don't have to do this every frame, so check if we
  // have enumerated recently
  if (!mLastControllerEnumerationTime.IsNull()) {
    TimeDuration duration = TimeStamp::Now() - mLastControllerEnumerationTime;
    if (duration.ToMilliseconds() < gfxPrefs::VRControllerEnumerateInterval()) {
      return;
    }
  }

  // Only enumerate controllers once we need them
  if (!mVRControllersRequested) {
    return;
  }

  for (uint32_t i = 0; i < mManagers.Length(); ++i) {
    mManagers[i]->ScanForControllers();
  }

  mLastControllerEnumerationTime = TimeStamp::Now();
}

void VRManager::RemoveControllers() {
  for (uint32_t i = 0; i < mManagers.Length(); ++i) {
    mManagers[i]->RemoveControllers();
  }
  mVRControllerIDs.Clear();
}

void VRManager::CreateVRTestSystem() {
  if (mPuppetManager) {
    mPuppetManager->ClearTestDisplays();
    return;
  }

  mPuppetManager = VRSystemManagerPuppet::Create();
  mManagers.AppendElement(mPuppetManager);
}

VRSystemManagerPuppet* VRManager::GetPuppetManager() {
  MOZ_ASSERT(mPuppetManager);
  return mPuppetManager;
}

VRSystemManagerExternal* VRManager::GetExternalManager() {
  MOZ_ASSERT(mExternalManager);
  return mExternalManager;
}

template <class T>
void VRManager::NotifyGamepadChange(uint32_t aIndex, const T& aInfo) {
  dom::GamepadChangeEventBody body(aInfo);
  dom::GamepadChangeEvent e(aIndex, dom::GamepadServiceType::VR, body);

  for (auto iter = mVRManagerParents.Iter(); !iter.Done(); iter.Next()) {
    Unused << iter.Get()->GetKey()->SendGamepadUpdate(e);
  }
}

void VRManager::VibrateHaptic(uint32_t aControllerIdx, uint32_t aHapticIndex,
                              double aIntensity, double aDuration,
                              const VRManagerPromise& aPromise)

{
  for (uint32_t i = 0; i < mManagers.Length(); ++i) {
    mManagers[i]->VibrateHaptic(aControllerIdx, aHapticIndex, aIntensity,
                                aDuration, aPromise);
  }
}

void VRManager::StopVibrateHaptic(uint32_t aControllerIdx) {
  for (const auto& manager : mManagers) {
    manager->StopVibrateHaptic(aControllerIdx);
  }
}

void VRManager::NotifyVibrateHapticCompleted(const VRManagerPromise& aPromise) {
  aPromise.mParent->SendReplyGamepadVibrateHaptic(aPromise.mPromiseID);
}

void VRManager::DispatchSubmitFrameResult(
    uint32_t aDisplayID, const VRSubmitFrameResultInfo& aResult) {
  for (auto iter = mVRManagerParents.Iter(); !iter.Done(); iter.Next()) {
    Unused << iter.Get()->GetKey()->SendDispatchSubmitFrameResult(aDisplayID,
                                                                  aResult);
  }
}

void VRManager::StartVRNavigation(const uint32_t& aDisplayID) {
  RefPtr<VRDisplayHost> display = GetDisplay(aDisplayID);
  if (display) {
    display->StartVRNavigation();
  }
}

void VRManager::StopVRNavigation(const uint32_t& aDisplayID,
                                 const TimeDuration& aTimeout) {
  RefPtr<VRDisplayHost> display = GetDisplay(aDisplayID);
  if (display) {
    display->StopVRNavigation(aTimeout);
  }
}

bool VRManager::IsPresenting() {
  for (const auto& manager : mManagers) {
    if (manager->GetIsPresenting()) {
      return true;
    }
  }

  return false;
}

}  // namespace gfx
}  // namespace mozilla
