/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VRManagerChild.h"
#include "VRManagerParent.h"
#include "VRDisplayClient.h"
#include "nsGlobalWindow.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/layers/CompositorThread.h" // for CompositorThread
#include "mozilla/dom/Navigator.h"
#include "mozilla/dom/VREventObserver.h"
#include "mozilla/dom/WindowBinding.h" // for FrameRequestCallback
#include "mozilla/dom/ContentChild.h"
#include "mozilla/layers/TextureClient.h"
#include "nsContentUtils.h"

#ifdef MOZ_GAMEPAD
#include "mozilla/dom/GamepadManager.h"
#endif

using layers::TextureClient;

namespace {
const nsTArray<RefPtr<dom::VREventObserver>>::index_type kNoIndex =
  nsTArray<RefPtr<dom::VREventObserver> >::NoIndex;
} // namespace

namespace mozilla {
namespace gfx {

static StaticRefPtr<VRManagerChild> sVRManagerChildSingleton;
static StaticRefPtr<VRManagerParent> sVRManagerParentSingleton;

void ReleaseVRManagerParentSingleton() {
  sVRManagerParentSingleton = nullptr;
}

VRManagerChild::VRManagerChild()
  : TextureForwarder()
  , mDisplaysInitialized(false)
  , mInputFrameID(-1)
  , mMessageLoop(MessageLoop::current())
  , mFrameRequestCallbackCounter(0)
  , mBackend(layers::LayersBackend::LAYERS_NONE)
{
  MOZ_COUNT_CTOR(VRManagerChild);
  MOZ_ASSERT(NS_IsMainThread());

  mStartTimeStamp = TimeStamp::Now();
}

VRManagerChild::~VRManagerChild()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_COUNT_DTOR(VRManagerChild);
}

/*static*/ void
VRManagerChild::IdentifyTextureHost(const TextureFactoryIdentifier& aIdentifier)
{
  if (sVRManagerChildSingleton) {
    sVRManagerChildSingleton->mBackend = aIdentifier.mParentBackend;
    sVRManagerChildSingleton->mSyncObject = SyncObject::CreateSyncObject(aIdentifier.mSyncHandle);
  }
}

layers::LayersBackend
VRManagerChild::GetBackendType() const
{
  return mBackend;
}

/*static*/ VRManagerChild*
VRManagerChild::Get()
{
  MOZ_ASSERT(sVRManagerChildSingleton);
  return sVRManagerChildSingleton;
}

/* static */ bool
VRManagerChild::IsCreated()
{
  return !!sVRManagerChildSingleton;
}

/* static */ bool
VRManagerChild::InitForContent(Endpoint<PVRManagerChild>&& aEndpoint)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!sVRManagerChildSingleton);

  RefPtr<VRManagerChild> child(new VRManagerChild());
  if (!aEndpoint.Bind(child)) {
    NS_RUNTIMEABORT("Couldn't Open() Compositor channel.");
    return false;
  }
  sVRManagerChildSingleton = child;
  return true;
}

/* static */ bool
VRManagerChild::ReinitForContent(Endpoint<PVRManagerChild>&& aEndpoint)
{
  MOZ_ASSERT(NS_IsMainThread());

  ShutDown();

  return InitForContent(Move(aEndpoint));
}

/*static*/ void
VRManagerChild::InitSameProcess()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!sVRManagerChildSingleton);

  sVRManagerChildSingleton = new VRManagerChild();
  sVRManagerParentSingleton = VRManagerParent::CreateSameProcess();
  sVRManagerChildSingleton->Open(sVRManagerParentSingleton->GetIPCChannel(),
                                 mozilla::layers::CompositorThreadHolder::Loop(),
                                 mozilla::ipc::ChildSide);
}

/* static */ void
VRManagerChild::InitWithGPUProcess(Endpoint<PVRManagerChild>&& aEndpoint)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!sVRManagerChildSingleton);

  sVRManagerChildSingleton = new VRManagerChild();
  if (!aEndpoint.Bind(sVRManagerChildSingleton)) {
    NS_RUNTIMEABORT("Couldn't Open() Compositor channel.");
    return;
  }
}

/*static*/ void
VRManagerChild::ShutDown()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (sVRManagerChildSingleton) {
    sVRManagerChildSingleton->Destroy();
    sVRManagerChildSingleton = nullptr;
  }
}

/*static*/ void
VRManagerChild::DeferredDestroy(RefPtr<VRManagerChild> aVRManagerChild)
{
  aVRManagerChild->Close();
}

void
VRManagerChild::Destroy()
{
  mTexturesWaitingRecycled.Clear();

  // Keep ourselves alive until everything has been shut down
  RefPtr<VRManagerChild> selfRef = this;

  // The DeferredDestroyVRManager task takes ownership of
  // the VRManagerChild and will release it when it runs.
  MessageLoop::current()->PostTask(
             NewRunnableFunction(DeferredDestroy, selfRef));
}

layers::PTextureChild*
VRManagerChild::AllocPTextureChild(const SurfaceDescriptor&,
                                   const LayersBackend&,
                                   const TextureFlags&,
                                   const uint64_t&)
{
  return TextureClient::CreateIPDLActor();
}

bool
VRManagerChild::DeallocPTextureChild(PTextureChild* actor)
{
  return TextureClient::DestroyIPDLActor(actor);
}

PVRLayerChild*
VRManagerChild::AllocPVRLayerChild(const uint32_t& aDisplayID,
                                   const float& aLeftEyeX,
                                   const float& aLeftEyeY,
                                   const float& aLeftEyeWidth,
                                   const float& aLeftEyeHeight,
                                   const float& aRightEyeX,
                                   const float& aRightEyeY,
                                   const float& aRightEyeWidth,
                                   const float& aRightEyeHeight)
{
  RefPtr<VRLayerChild> layer = new VRLayerChild(aDisplayID, this);
  return layer.forget().take();
}

bool
VRManagerChild::DeallocPVRLayerChild(PVRLayerChild* actor)
{
  delete actor;
  return true;
}

void
VRManagerChild::UpdateDisplayInfo(nsTArray<VRDisplayInfo>& aDisplayUpdates)
{
  bool bDisplayConnected = false;
  bool bDisplayDisconnected = false;

  // Check if any displays have been disconnected
  for (auto& display : mDisplays) {
    bool found = false;
    for (auto& displayUpdate : aDisplayUpdates) {
      if (display->GetDisplayInfo().GetDisplayID() == displayUpdate.GetDisplayID()) {
        found = true;
        break;
      }
    }
    if (!found) {
      display->NotifyDisconnected();
      bDisplayDisconnected = true;
    }
  }

  // mDisplays could be a hashed container for more scalability, but not worth
  // it now as we expect < 10 entries.
  nsTArray<RefPtr<VRDisplayClient>> displays;
  for (VRDisplayInfo& displayUpdate : aDisplayUpdates) {
    bool isNewDisplay = true;
    for (auto& display : mDisplays) {
      const VRDisplayInfo& prevInfo = display->GetDisplayInfo();
      if (prevInfo.GetDisplayID() == displayUpdate.GetDisplayID()) {
        if (displayUpdate.GetIsConnected() && !prevInfo.GetIsConnected()) {
          bDisplayConnected = true;
        }
        if (!displayUpdate.GetIsConnected() && prevInfo.GetIsConnected()) {
          bDisplayDisconnected = true;
        }
        display->UpdateDisplayInfo(displayUpdate);
        displays.AppendElement(display);
        isNewDisplay = false;
        break;
      }
    }
    if (isNewDisplay) {
      displays.AppendElement(new VRDisplayClient(displayUpdate));
      bDisplayConnected = true;
    }
  }

  mDisplays = displays;

  if (bDisplayConnected) {
    FireDOMVRDisplayConnectEvent();
  }
  if (bDisplayDisconnected) {
    FireDOMVRDisplayDisconnectEvent();
  }

  mDisplaysInitialized = true;
}

mozilla::ipc::IPCResult
VRManagerChild::RecvUpdateDisplayInfo(nsTArray<VRDisplayInfo>&& aDisplayUpdates)
{
  UpdateDisplayInfo(aDisplayUpdates);
  for (auto& windowId : mNavigatorCallbacks) {
    /** We must call NotifyVRDisplaysUpdated for every
     * window's Navigator in mNavigatorCallbacks to ensure that
     * the promise returned by Navigator.GetVRDevices
     * can resolve.  This must happen even if no changes
     * to VRDisplays have been detected here.
     */
    nsGlobalWindow* window = nsGlobalWindow::GetInnerWindowWithId(windowId);
    if (!window) {
      continue;
    }
    ErrorResult result;
    dom::Navigator* nav = window->GetNavigator(result);
    if (NS_WARN_IF(result.Failed())) {
      continue;
    }
    nav->NotifyVRDisplaysUpdated();
  }
  mNavigatorCallbacks.Clear();
  return IPC_OK();
}

bool
VRManagerChild::GetVRDisplays(nsTArray<RefPtr<VRDisplayClient>>& aDisplays)
{
  if (!mDisplaysInitialized) {
    /**
     * If we haven't received any asynchronous callback after requesting
     * display enumeration with RefreshDisplays, get the existing displays
     * that have already been enumerated by other VRManagerChild instances.
     */
    nsTArray<VRDisplayInfo> displays;
    Unused << SendGetDisplays(&displays);
    UpdateDisplayInfo(displays);
  }
  aDisplays = mDisplays;
  return true;
}

bool
VRManagerChild::RefreshVRDisplaysWithCallback(uint64_t aWindowId)
{
  bool success = SendRefreshDisplays();
  if (success) {
    mNavigatorCallbacks.AppendElement(aWindowId);
  }
  return success;
}

int
VRManagerChild::GetInputFrameID()
{
  return mInputFrameID;
}

mozilla::ipc::IPCResult
VRManagerChild::RecvParentAsyncMessages(InfallibleTArray<AsyncParentMessageData>&& aMessages)
{
  for (InfallibleTArray<AsyncParentMessageData>::index_type i = 0; i < aMessages.Length(); ++i) {
    const AsyncParentMessageData& message = aMessages[i];

    switch (message.type()) {
      case AsyncParentMessageData::TOpNotifyNotUsed: {
        const OpNotifyNotUsed& op = message.get_OpNotifyNotUsed();
        NotifyNotUsed(op.TextureId(), op.fwdTransactionId());
        break;
      }
      default:
        NS_ERROR("unknown AsyncParentMessageData type");
        return IPC_FAIL_NO_REASON(this);
    }
  }
  return IPC_OK();
}

PTextureChild*
VRManagerChild::CreateTexture(const SurfaceDescriptor& aSharedData,
                              LayersBackend aLayersBackend,
                              TextureFlags aFlags,
                              uint64_t aSerial)
{
  return SendPTextureConstructor(aSharedData, aLayersBackend, aFlags, aSerial);
}

void
VRManagerChild::CancelWaitForRecycle(uint64_t aTextureId)
{
  RefPtr<TextureClient> client = mTexturesWaitingRecycled.Get(aTextureId);
  if (!client) {
    return;
  }
  mTexturesWaitingRecycled.Remove(aTextureId);
}

void
VRManagerChild::NotifyNotUsed(uint64_t aTextureId, uint64_t aFwdTransactionId)
{
  RefPtr<TextureClient> client = mTexturesWaitingRecycled.Get(aTextureId);
  if (!client) {
    return;
  }
  mTexturesWaitingRecycled.Remove(aTextureId);
}

bool
VRManagerChild::AllocShmem(size_t aSize,
                           ipc::SharedMemory::SharedMemoryType aType,
                           ipc::Shmem* aShmem)
{
  return PVRManagerChild::AllocShmem(aSize, aType, aShmem);
}

bool
VRManagerChild::AllocUnsafeShmem(size_t aSize,
                                 ipc::SharedMemory::SharedMemoryType aType,
                                 ipc::Shmem* aShmem)
{
  return PVRManagerChild::AllocUnsafeShmem(aSize, aType, aShmem);
}

bool
VRManagerChild::DeallocShmem(ipc::Shmem& aShmem)
{
  return PVRManagerChild::DeallocShmem(aShmem);
}

PVRLayerChild*
VRManagerChild::CreateVRLayer(uint32_t aDisplayID, const Rect& aLeftEyeRect, const Rect& aRightEyeRect)
{
  return SendPVRLayerConstructor(aDisplayID,
                                 aLeftEyeRect.x, aLeftEyeRect.y, aLeftEyeRect.width, aLeftEyeRect.height,
                                 aRightEyeRect.x, aRightEyeRect.y, aRightEyeRect.width, aRightEyeRect.height);
}


// XXX TODO - VRManagerChild::FrameRequest is the same as nsIDocument::FrameRequest, should we consolodate these?
struct VRManagerChild::FrameRequest
{
  FrameRequest(mozilla::dom::FrameRequestCallback& aCallback,
    int32_t aHandle) :
    mCallback(&aCallback),
    mHandle(aHandle)
  {}

  // Conversion operator so that we can append these to a
  // FrameRequestCallbackList
  operator const RefPtr<mozilla::dom::FrameRequestCallback>& () const {
    return mCallback;
  }

  // Comparator operators to allow RemoveElementSorted with an
  // integer argument on arrays of FrameRequest
  bool operator==(int32_t aHandle) const {
    return mHandle == aHandle;
  }
  bool operator<(int32_t aHandle) const {
    return mHandle < aHandle;
  }

  RefPtr<mozilla::dom::FrameRequestCallback> mCallback;
  int32_t mHandle;
};

nsresult
VRManagerChild::ScheduleFrameRequestCallback(mozilla::dom::FrameRequestCallback& aCallback,
                                             int32_t *aHandle)
{
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

void
VRManagerChild::CancelFrameRequestCallback(int32_t aHandle)
{
  // mFrameRequestCallbacks is stored sorted by handle
  mFrameRequestCallbacks.RemoveElementSorted(aHandle);
}

mozilla::ipc::IPCResult
VRManagerChild::RecvNotifyVSync()
{
  for (auto& display : mDisplays) {
    display->NotifyVsync();
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult
VRManagerChild::RecvNotifyVRVSync(const uint32_t& aDisplayID)
{
  for (auto& display : mDisplays) {
    if (display->GetDisplayInfo().GetDisplayID() == aDisplayID) {
      display->NotifyVRVsync();
    }
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult
VRManagerChild::RecvGamepadUpdate(const GamepadChangeEvent& aGamepadEvent)
{
#ifdef MOZ_GAMEPAD
  // VRManagerChild could be at other processes, but GamepadManager
  // only exists at the content process or the same process
  // in non-e10s mode.
  MOZ_ASSERT(XRE_IsContentProcess() || IsSameProcess());

  RefPtr<GamepadManager> gamepadManager(GamepadManager::GetService());
  if (gamepadManager) {
    gamepadManager->Update(aGamepadEvent);
  }
#endif

  return IPC_OK();
}

void
VRManagerChild::RunFrameRequestCallbacks()
{
  TimeStamp nowTime = TimeStamp::Now();
  mozilla::TimeDuration duration = nowTime - mStartTimeStamp;
  DOMHighResTimeStamp timeStamp = duration.ToMilliseconds();


  nsTArray<FrameRequest> callbacks;
  callbacks.AppendElements(mFrameRequestCallbacks);
  mFrameRequestCallbacks.Clear();
  for (auto& callback : callbacks) {
    callback.mCallback->Call(timeStamp);
  }
}

void
VRManagerChild::FireDOMVRDisplayConnectEvent()
{
  nsContentUtils::AddScriptRunner(NewRunnableMethod(this,
    &VRManagerChild::FireDOMVRDisplayConnectEventInternal));
}

void
VRManagerChild::FireDOMVRDisplayDisconnectEvent()
{
  nsContentUtils::AddScriptRunner(NewRunnableMethod(this,
    &VRManagerChild::FireDOMVRDisplayDisconnectEventInternal));
}

void
VRManagerChild::FireDOMVRDisplayPresentChangeEvent()
{
  nsContentUtils::AddScriptRunner(NewRunnableMethod(this,
    &VRManagerChild::FireDOMVRDisplayPresentChangeEventInternal));
}

void
VRManagerChild::FireDOMVRDisplayConnectEventInternal()
{
  for (auto& listener : mListeners) {
    listener->NotifyVRDisplayConnect();
  }
}

void
VRManagerChild::FireDOMVRDisplayDisconnectEventInternal()
{
  for (auto& listener : mListeners) {
    listener->NotifyVRDisplayDisconnect();
  }
}

void
VRManagerChild::FireDOMVRDisplayPresentChangeEventInternal()
{
  for (auto& listener : mListeners) {
    listener->NotifyVRDisplayPresentChange();
  }
}

void
VRManagerChild::AddListener(dom::VREventObserver* aObserver)
{
  MOZ_ASSERT(aObserver);

  if (mListeners.IndexOf(aObserver) != kNoIndex) {
    return; // already exists
  }

  mListeners.AppendElement(aObserver);
  if (mListeners.Length() == 1) {
    Unused << SendSetHaveEventListener(true);
  }
}

void
VRManagerChild::RemoveListener(dom::VREventObserver* aObserver)
{
  MOZ_ASSERT(aObserver);

  mListeners.RemoveElement(aObserver);
  if (mListeners.IsEmpty()) {
    Unused << SendSetHaveEventListener(false);
  }
}

void
VRManagerChild::HandleFatalError(const char* aName, const char* aMsg) const
{
  dom::ContentChild::FatalErrorIfNotUsingGPUProcess(aName, aMsg, OtherPid());
}

} // namespace gfx
} // namespace mozilla
