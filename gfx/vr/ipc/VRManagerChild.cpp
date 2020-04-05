/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VRManagerChild.h"
#include "VRLayerChild.h"
#include "VRManagerParent.h"
#include "VRThread.h"
#include "VRDisplayClient.h"
#include "nsGlobalWindow.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/layers/CompositorThread.h"  // for CompositorThread
#include "mozilla/dom/Navigator.h"
#include "mozilla/dom/VREventObserver.h"
#include "mozilla/dom/WindowBinding.h"  // for FrameRequestCallback
#include "mozilla/dom/ContentChild.h"
#include "nsContentUtils.h"
#include "mozilla/dom/GamepadManager.h"
#include "mozilla/layers/SyncObject.h"

using namespace mozilla::dom;

namespace {
const nsTArray<RefPtr<mozilla::gfx::VRManagerEventObserver>>::index_type
    kNoIndex = nsTArray<RefPtr<mozilla::gfx::VRManagerEventObserver>>::NoIndex;
}  // namespace

namespace mozilla {
namespace gfx {

static StaticRefPtr<VRManagerChild> sVRManagerChildSingleton;
static StaticRefPtr<VRManagerParent> sVRManagerParentSingleton;

void ReleaseVRManagerParentSingleton() { sVRManagerParentSingleton = nullptr; }

VRManagerChild::VRManagerChild()
    : mRuntimeCapabilities(VRDisplayCapabilityFlags::Cap_None),
      mMessageLoop(MessageLoop::current()),
      mFrameRequestCallbackCounter(0),
      mWaitingForEnumeration(false),
      mBackend(layers::LayersBackend::LAYERS_NONE) {
  MOZ_ASSERT(NS_IsMainThread());

  mStartTimeStamp = TimeStamp::Now();
  AddRef();
}

VRManagerChild::~VRManagerChild() { MOZ_ASSERT(NS_IsMainThread()); }

/*static*/
void VRManagerChild::IdentifyTextureHost(
    const TextureFactoryIdentifier& aIdentifier) {
  if (sVRManagerChildSingleton) {
    sVRManagerChildSingleton->mBackend = aIdentifier.mParentBackend;
    sVRManagerChildSingleton->mSyncObject =
        layers::SyncObjectClient::CreateSyncObjectClient(
            aIdentifier.mSyncHandle);
  }
}

layers::LayersBackend VRManagerChild::GetBackendType() const {
  return mBackend;
}

/*static*/
VRManagerChild* VRManagerChild::Get() {
  MOZ_ASSERT(sVRManagerChildSingleton);
  return sVRManagerChildSingleton;
}

/* static */
bool VRManagerChild::IsCreated() { return !!sVRManagerChildSingleton; }

/* static */
bool VRManagerChild::IsPresenting() {
  if (!VRManagerChild::IsCreated()) {
    return false;
  }

  nsTArray<RefPtr<VRDisplayClient>> displays;
  sVRManagerChildSingleton->GetVRDisplays(displays);

  bool result = false;
  for (auto& display : displays) {
    result |= display->IsPresenting();
  }
  return result;
}

/* static */
bool VRManagerChild::InitForContent(Endpoint<PVRManagerChild>&& aEndpoint) {
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<VRManagerChild> child(new VRManagerChild());
  if (!aEndpoint.Bind(child)) {
    return false;
  }
  sVRManagerChildSingleton = child;
  return true;
}

/*static*/
void VRManagerChild::InitSameProcess() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!sVRManagerChildSingleton);

  sVRManagerChildSingleton = new VRManagerChild();
  sVRManagerParentSingleton = VRManagerParent::CreateSameProcess();
  sVRManagerChildSingleton->Open(sVRManagerParentSingleton->GetIPCChannel(),
                                 CompositorThreadHolder::Loop(),
                                 mozilla::ipc::ChildSide);
}

/* static */
void VRManagerChild::InitWithGPUProcess(Endpoint<PVRManagerChild>&& aEndpoint) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!sVRManagerChildSingleton);

  sVRManagerChildSingleton = new VRManagerChild();
  if (!aEndpoint.Bind(sVRManagerChildSingleton)) {
    MOZ_CRASH("Couldn't Open() Compositor channel.");
  }
}

/*static*/
void VRManagerChild::ShutDown() {
  MOZ_ASSERT(NS_IsMainThread());
  if (!sVRManagerChildSingleton) {
    return;
  }
  sVRManagerChildSingleton->Close();
  sVRManagerChildSingleton = nullptr;
}

void VRManagerChild::ActorDealloc() { Release(); }

void VRManagerChild::ActorDestroy(ActorDestroyReason aReason) {
  if (sVRManagerChildSingleton == this) {
    sVRManagerChildSingleton = nullptr;
  }
}

PVRLayerChild* VRManagerChild::AllocPVRLayerChild(const uint32_t& aDisplayID,
                                                  const uint32_t& aGroup) {
  return VRLayerChild::CreateIPDLActor();
}

bool VRManagerChild::DeallocPVRLayerChild(PVRLayerChild* actor) {
  return VRLayerChild::DestroyIPDLActor(actor);
}

void VRManagerChild::UpdateDisplayInfo(const VRDisplayInfo& aDisplayInfo) {
  nsTArray<uint32_t> disconnectedDisplays;
  nsTArray<uint32_t> connectedDisplays;

  const nsTArray<RefPtr<VRDisplayClient>> prevDisplays(mDisplays);

  // Check if any displays have been disconnected
  for (auto& display : prevDisplays) {
    bool found = false;
    if (aDisplayInfo.GetDisplayID() != 0) {
      if (display->GetDisplayInfo().GetDisplayID() ==
          aDisplayInfo.GetDisplayID()) {
        found = true;
        break;
      }
    }
    if (!found) {
      // In order to make the current VRDisplay can continue to apply for the
      // newest VRDisplayInfo, we need to exit presentionation before
      // disconnecting.
      if (display->IsPresentationGenerationCurrent()) {
        NotifyPresentationGenerationChangedInternal(
            display->GetDisplayInfo().GetDisplayID());

        RefPtr<VRManagerChild> vm = VRManagerChild::Get();
        vm->FireDOMVRDisplayPresentChangeEvent(
            display->GetDisplayInfo().GetDisplayID());
      }
      display->NotifyDisconnected();
      disconnectedDisplays.AppendElement(
          display->GetDisplayInfo().GetDisplayID());
    }
  }

  // mDisplays could be a hashed container for more scalability, but not worth
  // it now as we expect < 10 entries.
  nsTArray<RefPtr<VRDisplayClient>> displays;
  if (aDisplayInfo.GetDisplayID() != 0) {
    bool isNewDisplay = true;
    for (auto& display : prevDisplays) {
      const VRDisplayInfo& prevInfo = display->GetDisplayInfo();
      if (prevInfo.GetDisplayID() == aDisplayInfo.GetDisplayID()) {
        if (aDisplayInfo.GetIsConnected() && !prevInfo.GetIsConnected()) {
          connectedDisplays.AppendElement(aDisplayInfo.GetDisplayID());
        }
        if (!aDisplayInfo.GetIsConnected() && prevInfo.GetIsConnected()) {
          disconnectedDisplays.AppendElement(aDisplayInfo.GetDisplayID());
        }
        // MOZ_KnownLive because 'prevDisplays' is guaranteed to keep it alive.
        //
        // This can go away once
        // https://bugzilla.mozilla.org/show_bug.cgi?id=1620312 is fixed.
        MOZ_KnownLive(display)->UpdateDisplayInfo(aDisplayInfo);
        displays.AppendElement(display);
        isNewDisplay = false;
        break;
      }
    }
    if (isNewDisplay) {
      displays.AppendElement(new VRDisplayClient(aDisplayInfo));
      connectedDisplays.AppendElement(aDisplayInfo.GetDisplayID());
    }
  }

  mDisplays = displays;

  // We wish to fire the events only after mDisplays is updated
  for (uint32_t displayID : disconnectedDisplays) {
    FireDOMVRDisplayDisconnectEvent(displayID);
  }

  for (uint32_t displayID : connectedDisplays) {
    FireDOMVRDisplayConnectEvent(displayID);
  }
}

bool VRManagerChild::RuntimeSupportsVR() const {
  return bool(mRuntimeCapabilities & VRDisplayCapabilityFlags::Cap_ImmersiveVR);
}
bool VRManagerChild::RuntimeSupportsAR() const {
  return bool(mRuntimeCapabilities & VRDisplayCapabilityFlags::Cap_ImmersiveAR);
}

mozilla::ipc::IPCResult VRManagerChild::RecvUpdateRuntimeCapabilities(
    const VRDisplayCapabilityFlags& aCapabilities) {
  mRuntimeCapabilities = aCapabilities;
  nsContentUtils::AddScriptRunner(NewRunnableMethod<>(
      "gfx::VRManagerChild::NotifyRuntimeCapabilitiesUpdatedInternal", this,
      &VRManagerChild::NotifyRuntimeCapabilitiesUpdatedInternal));
  return IPC_OK();
}

void VRManagerChild::NotifyRuntimeCapabilitiesUpdatedInternal() {
  nsTArray<RefPtr<VRManagerEventObserver>> listeners;
  listeners = mListeners;
  for (auto& listener : listeners) {
    listener->NotifyDetectRuntimesCompleted();
  }
}

mozilla::ipc::IPCResult VRManagerChild::RecvUpdateDisplayInfo(
    const VRDisplayInfo& aDisplayInfo) {
  UpdateDisplayInfo(aDisplayInfo);
  for (auto& windowId : mNavigatorCallbacks) {
    /** We must call NotifyVRDisplaysUpdated for every
     * window's Navigator in mNavigatorCallbacks to ensure that
     * the promise returned by Navigator.GetVRDevices
     * can resolve.  This must happen even if no changes
     * to VRDisplays have been detected here.
     */
    nsGlobalWindowInner* window =
        nsGlobalWindowInner::GetInnerWindowWithId(windowId);
    if (!window) {
      continue;
    }
    dom::Navigator* nav = window->Navigator();
    if (!nav) {
      continue;
    }
    nav->NotifyVRDisplaysUpdated();
  }
  mNavigatorCallbacks.Clear();
  if (mWaitingForEnumeration) {
    nsContentUtils::AddScriptRunner(NewRunnableMethod<>(
        "gfx::VRManagerChild::NotifyEnumerationCompletedInternal", this,
        &VRManagerChild::NotifyEnumerationCompletedInternal));
    mWaitingForEnumeration = false;
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult VRManagerChild::RecvNotifyPuppetCommandBufferCompleted(
    bool aSuccess) {
  RefPtr<dom::Promise> promise = mRunPuppetPromise;
  mRunPuppetPromise = nullptr;
  if (aSuccess) {
    promise->MaybeResolve(JS::UndefinedHandleValue);
  } else {
    promise->MaybeRejectWithUndefined();
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult VRManagerChild::RecvNotifyPuppetResetComplete() {
  nsTArray<RefPtr<dom::Promise>> promises;
  promises.AppendElements(mResetPuppetPromises);
  mResetPuppetPromises.Clear();
  for (const auto& promise : promises) {
    promise->MaybeResolve(JS::UndefinedHandleValue);
  }
  return IPC_OK();
}

void VRManagerChild::RunPuppet(const nsTArray<uint64_t>& aBuffer,
                               dom::Promise* aPromise, ErrorResult& aRv) {
  if (mRunPuppetPromise) {
    // We only allow one puppet script to run simultaneously.
    // The prior promise must be resolved before running a new
    // script.
    aRv.Throw(NS_ERROR_INVALID_ARG);
    return;
  }
  if (!SendRunPuppet(aBuffer)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }
  mRunPuppetPromise = aPromise;
}

void VRManagerChild::ResetPuppet(dom::Promise* aPromise, ErrorResult& aRv) {
  if (!SendResetPuppet()) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }
  mResetPuppetPromises.AppendElement(aPromise);
}

void VRManagerChild::GetVRDisplays(
    nsTArray<RefPtr<VRDisplayClient>>& aDisplays) {
  aDisplays = mDisplays;
}

bool VRManagerChild::RefreshVRDisplaysWithCallback(uint64_t aWindowId) {
  bool success = SendRefreshDisplays();
  if (success) {
    mNavigatorCallbacks.AppendElement(aWindowId);
  }
  return success;
}

bool VRManagerChild::EnumerateVRDisplays() {
  bool success = SendRefreshDisplays();
  if (success) {
    mWaitingForEnumeration = true;
  }
  return success;
}

void VRManagerChild::DetectRuntimes() { Unused << SendDetectRuntimes(); }

PVRLayerChild* VRManagerChild::CreateVRLayer(uint32_t aDisplayID,
                                             nsIEventTarget* aTarget,
                                             uint32_t aGroup) {
  PVRLayerChild* vrLayerChild = AllocPVRLayerChild(aDisplayID, aGroup);
  // Do the DOM labeling.
  if (aTarget) {
    SetEventTargetForActor(vrLayerChild, aTarget);
    MOZ_ASSERT(vrLayerChild->GetActorEventTarget());
  }
  return SendPVRLayerConstructor(vrLayerChild, aDisplayID, aGroup);
}

nsresult VRManagerChild::ScheduleFrameRequestCallback(
    mozilla::dom::FrameRequestCallback& aCallback, int32_t* aHandle) {
  if (mFrameRequestCallbackCounter == INT32_MAX) {
    // Can't increment without overflowing; bail out
    return NS_ERROR_NOT_AVAILABLE;
  }
  int32_t newHandle = ++mFrameRequestCallbackCounter;

  DebugOnly<FrameRequest*> request =
      mFrameRequestCallbacks.AppendElement(FrameRequest(aCallback, newHandle));
  NS_ASSERTION(request, "This is supposed to be infallible!");

  *aHandle = newHandle;
  return NS_OK;
}

void VRManagerChild::CancelFrameRequestCallback(int32_t aHandle) {
  // mFrameRequestCallbacks is stored sorted by handle
  mFrameRequestCallbacks.RemoveElementSorted(aHandle);
}

void VRManagerChild::RunFrameRequestCallbacks() {
  AUTO_PROFILER_TRACING_MARKER("VR", "RunFrameRequestCallbacks", GRAPHICS);

  TimeStamp nowTime = TimeStamp::Now();
  mozilla::TimeDuration duration = nowTime - mStartTimeStamp;
  DOMHighResTimeStamp timeStamp = duration.ToMilliseconds();

  nsTArray<FrameRequest> callbacks;
  callbacks.AppendElements(mFrameRequestCallbacks);
  mFrameRequestCallbacks.Clear();
  for (auto& callback : callbacks) {
    // The FrameRequest copied into the on-stack array holds a strong ref to its
    // mCallback and there's nothing that can drop that ref until we return.
    MOZ_KnownLive(callback.mCallback)->Call(timeStamp);
  }
}

void VRManagerChild::NotifyPresentationGenerationChanged(uint32_t aDisplayID) {
  nsContentUtils::AddScriptRunner(NewRunnableMethod<uint32_t>(
      "gfx::VRManagerChild::NotifyPresentationGenerationChangedInternal", this,
      &VRManagerChild::NotifyPresentationGenerationChangedInternal,
      aDisplayID));
}

void VRManagerChild::FireDOMVRDisplayMountedEvent(uint32_t aDisplayID) {
  nsContentUtils::AddScriptRunner(NewRunnableMethod<uint32_t>(
      "gfx::VRManagerChild::FireDOMVRDisplayMountedEventInternal", this,
      &VRManagerChild::FireDOMVRDisplayMountedEventInternal, aDisplayID));
}

void VRManagerChild::FireDOMVRDisplayUnmountedEvent(uint32_t aDisplayID) {
  nsContentUtils::AddScriptRunner(NewRunnableMethod<uint32_t>(
      "gfx::VRManagerChild::FireDOMVRDisplayUnmountedEventInternal", this,
      &VRManagerChild::FireDOMVRDisplayUnmountedEventInternal, aDisplayID));
}

void VRManagerChild::FireDOMVRDisplayConnectEvent(uint32_t aDisplayID) {
  nsContentUtils::AddScriptRunner(NewRunnableMethod<uint32_t>(
      "gfx::VRManagerChild::FireDOMVRDisplayConnectEventInternal", this,
      &VRManagerChild::FireDOMVRDisplayConnectEventInternal, aDisplayID));
}

void VRManagerChild::FireDOMVRDisplayDisconnectEvent(uint32_t aDisplayID) {
  nsContentUtils::AddScriptRunner(NewRunnableMethod<uint32_t>(
      "gfx::VRManagerChild::FireDOMVRDisplayDisconnectEventInternal", this,
      &VRManagerChild::FireDOMVRDisplayDisconnectEventInternal, aDisplayID));
}

void VRManagerChild::FireDOMVRDisplayPresentChangeEvent(uint32_t aDisplayID) {
  nsContentUtils::AddScriptRunner(NewRunnableMethod<uint32_t>(
      "gfx::VRManagerChild::FireDOMVRDisplayPresentChangeEventInternal", this,
      &VRManagerChild::FireDOMVRDisplayPresentChangeEventInternal, aDisplayID));
}

void VRManagerChild::FireDOMVRDisplayMountedEventInternal(uint32_t aDisplayID) {
  // Iterate over a copy of mListeners, as dispatched events may modify it.
  nsTArray<RefPtr<VRManagerEventObserver>> listeners(mListeners);
  for (auto& listener : listeners) {
    listener->NotifyVRDisplayMounted(aDisplayID);
  }
}

void VRManagerChild::FireDOMVRDisplayUnmountedEventInternal(
    uint32_t aDisplayID) {
  // Iterate over a copy of mListeners, as dispatched events may modify it.
  nsTArray<RefPtr<VRManagerEventObserver>> listeners(mListeners);
  for (auto& listener : listeners) {
    listener->NotifyVRDisplayUnmounted(aDisplayID);
  }
}

void VRManagerChild::FireDOMVRDisplayConnectEventInternal(uint32_t aDisplayID) {
  // Iterate over a copy of mListeners, as dispatched events may modify it.
  nsTArray<RefPtr<VRManagerEventObserver>> listeners(mListeners);
  for (auto& listener : listeners) {
    listener->NotifyVRDisplayConnect(aDisplayID);
  }
}

void VRManagerChild::FireDOMVRDisplayDisconnectEventInternal(
    uint32_t aDisplayID) {
  // Iterate over a copy of mListeners, as dispatched events may modify it.
  nsTArray<RefPtr<VRManagerEventObserver>> listeners(mListeners);
  for (auto& listener : listeners) {
    listener->NotifyVRDisplayDisconnect(aDisplayID);
  }
}

void VRManagerChild::FireDOMVRDisplayPresentChangeEventInternal(
    uint32_t aDisplayID) {
  // Iterate over a copy of mListeners, as dispatched events may modify it.
  const nsTArray<RefPtr<VRManagerEventObserver>> listeners(mListeners);
  for (auto& listener : listeners) {
    // MOZ_KnownLive because 'listeners' is guaranteed to keep it alive.
    //
    // This can go away once
    // https://bugzilla.mozilla.org/show_bug.cgi?id=1620312 is fixed.
    MOZ_KnownLive(listener)->NotifyVRDisplayPresentChange(aDisplayID);
  }
}

void VRManagerChild::FireDOMVRDisplayConnectEventsForLoadInternal(
    uint32_t aDisplayID, VRManagerEventObserver* aObserver) {
  aObserver->NotifyVRDisplayConnect(aDisplayID);
}

void VRManagerChild::NotifyPresentationGenerationChangedInternal(
    uint32_t aDisplayID) {
  nsTArray<RefPtr<VRManagerEventObserver>> listeners(mListeners);
  for (auto& listener : listeners) {
    listener->NotifyPresentationGenerationChanged(aDisplayID);
  }
}

void VRManagerChild::NotifyEnumerationCompletedInternal() {
  nsTArray<RefPtr<VRManagerEventObserver>> listeners;
  listeners = mListeners;
  for (auto& listener : listeners) {
    listener->NotifyEnumerationCompleted();
  }
}

void VRManagerChild::FireDOMVRDisplayConnectEventsForLoad(
    VRManagerEventObserver* aObserver) {
  // We need to fire the VRDisplayConnect event when a page is loaded
  // for each VR Display that has already been enumerated
  nsTArray<RefPtr<VRDisplayClient>> displays;
  displays = mDisplays;
  for (auto& display : displays) {
    const VRDisplayInfo& info = display->GetDisplayInfo();
    if (info.GetIsConnected()) {
      nsContentUtils::AddScriptRunner(NewRunnableMethod<
                                      uint32_t, RefPtr<VRManagerEventObserver>>(
          "gfx::VRManagerChild::FireDOMVRDisplayConnectEventsForLoadInternal",
          this, &VRManagerChild::FireDOMVRDisplayConnectEventsForLoadInternal,
          info.GetDisplayID(), aObserver));
    }
  }
}

void VRManagerChild::AddListener(VRManagerEventObserver* aObserver) {
  MOZ_ASSERT(aObserver);

  if (mListeners.IndexOf(aObserver) != kNoIndex) {
    return;  // already exists
  }

  mListeners.AppendElement(aObserver);
  if (mListeners.Length() == 1) {
    Unused << SendSetHaveEventListener(true);
  }
}

void VRManagerChild::RemoveListener(VRManagerEventObserver* aObserver) {
  MOZ_ASSERT(aObserver);

  mListeners.RemoveElement(aObserver);
  if (mListeners.IsEmpty()) {
    Unused << SendSetHaveEventListener(false);
  }
}

void VRManagerChild::StartActivity() { Unused << SendStartActivity(); }

void VRManagerChild::StopActivity() {
  for (auto& listener : mListeners) {
    if (!listener->GetStopActivityStatus()) {
      // We are still showing VR in the active window.
      return;
    }
  }

  Unused << SendStopActivity();
}

void VRManagerChild::HandleFatalError(const char* aMsg) const {
  dom::ContentChild::FatalErrorIfNotUsingGPUProcess(aMsg, OtherPid());
}

void VRManagerChild::AddPromise(const uint32_t& aID, dom::Promise* aPromise) {
  MOZ_ASSERT(!mGamepadPromiseList.Get(aID, nullptr));
  mGamepadPromiseList.Put(aID, RefPtr{aPromise});
}

mozilla::ipc::IPCResult VRManagerChild::RecvReplyGamepadVibrateHaptic(
    const uint32_t& aPromiseID) {
  // VRManagerChild could be at other processes, but GamepadManager
  // only exists at the content process or the same process
  // in non-e10s mode.
  MOZ_ASSERT(XRE_IsContentProcess() || IsSameProcess());

  RefPtr<dom::Promise> p;
  if (!mGamepadPromiseList.Get(aPromiseID, getter_AddRefs(p))) {
    MOZ_CRASH("We should always have a promise.");
  }

  p->MaybeResolve(true);
  mGamepadPromiseList.Remove(aPromiseID);
  return IPC_OK();
}

}  // namespace gfx
}  // namespace mozilla
