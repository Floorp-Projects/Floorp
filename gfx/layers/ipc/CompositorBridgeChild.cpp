/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/CompositorBridgeChild.h"
#include "mozilla/layers/CompositorBridgeParent.h"
#include "mozilla/layers/CompositorThread.h"
#include <stddef.h>              // for size_t
#include "ClientLayerManager.h"  // for ClientLayerManager
#include "base/task.h"           // for NewRunnableMethod, etc
#include "mozilla/StaticPrefs_layers.h"
#include "mozilla/layers/CompositorManagerChild.h"
#include "mozilla/layers/ImageBridgeChild.h"
#include "mozilla/layers/APZChild.h"
#include "mozilla/layers/IAPZCTreeManager.h"
#include "mozilla/layers/APZCTreeManagerChild.h"
#include "mozilla/layers/CanvasChild.h"
#include "mozilla/layers/LayerTransactionChild.h"
#include "mozilla/layers/PaintThread.h"
#include "mozilla/layers/PLayerTransactionChild.h"
#include "mozilla/layers/PTextureChild.h"
#include "mozilla/layers/TextureClient.h"      // for TextureClient
#include "mozilla/layers/TextureClientPool.h"  // for TextureClientPool
#include "mozilla/layers/WebRenderBridgeChild.h"
#include "mozilla/layers/SyncObject.h"  // for SyncObjectClient
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/gfx/GPUProcessManager.h"
#include "mozilla/gfx/Logging.h"
#include "mozilla/ipc/Endpoint.h"
#include "mozilla/webgpu/WebGPUChild.h"
#include "mozilla/mozalloc.h"  // for operator new, etc
#include "mozilla/Telemetry.h"
#include "gfxConfig.h"
#include "nsDebug.h"          // for NS_WARNING
#include "nsISupportsImpl.h"  // for MOZ_COUNT_CTOR, etc
#include "nsTArray.h"         // for nsTArray, nsTArray_Impl
#include "mozilla/dom/BrowserChild.h"
#include "mozilla/dom/BrowserParent.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/Unused.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "nsThreadUtils.h"
#if defined(XP_WIN)
#  include "WinUtils.h"
#endif
#include "mozilla/widget/CompositorWidget.h"
#ifdef MOZ_WIDGET_SUPPORTS_OOP_COMPOSITING
#  include "mozilla/widget/CompositorWidgetChild.h"
#endif
#include "VsyncSource.h"

#ifdef MOZ_WIDGET_ANDROID
#  include "mozilla/layers/AndroidHardwareBuffer.h"
#endif

using mozilla::Unused;
using mozilla::gfx::GPUProcessManager;
using mozilla::layers::LayerTransactionChild;

namespace mozilla {
namespace layers {

static int sShmemCreationCounter = 0;

static void ResetShmemCounter() { sShmemCreationCounter = 0; }

static void ShmemAllocated(CompositorBridgeChild* aProtocol) {
  sShmemCreationCounter++;
  if (sShmemCreationCounter > 256) {
    aProtocol->SendSyncWithCompositor();
    ResetShmemCounter();
    MOZ_PERFORMANCE_WARNING(
        "gfx", "The number of shmem allocations is too damn high!");
  }
}

static StaticRefPtr<CompositorBridgeChild> sCompositorBridge;

Atomic<int32_t> KnowsCompositor::sSerialCounter(0);

CompositorBridgeChild::CompositorBridgeChild(CompositorManagerChild* aManager)
    : mCompositorManager(aManager),
      mIdNamespace(0),
      mResourceId(0),
      mCanSend(false),
      mActorDestroyed(false),
      mFwdTransactionId(0),
      mThread(NS_GetCurrentThread()),
      mProcessToken(0),
      mSectionAllocator(nullptr),
      mPaintLock("CompositorBridgeChild.mPaintLock"),
      mTotalAsyncPaints(0),
      mOutstandingAsyncPaints(0),
      mOutstandingAsyncEndTransaction(false),
      mIsDelayingForAsyncPaints(false),
      mSlowFlushCount(0),
      mTotalFlushCount(0) {
  MOZ_ASSERT(NS_IsMainThread());
}

CompositorBridgeChild::~CompositorBridgeChild() {
  if (mCanSend) {
    gfxCriticalError() << "CompositorBridgeChild was not deinitialized";
  }
}

bool CompositorBridgeChild::IsSameProcess() const {
  return OtherPid() == base::GetCurrentProcId();
}

void CompositorBridgeChild::PrepareFinalDestroy() {
  // Because of medium high priority DidComposite, we need to repost to
  // medium high priority queue to ensure the actor is destroyed after possible
  // pending DidComposite message.
  nsCOMPtr<nsIRunnable> runnable =
      NewRunnableMethod("CompositorBridgeChild::AfterDestroy", this,
                        &CompositorBridgeChild::AfterDestroy);
  NS_DispatchToCurrentThreadQueue(runnable.forget(),
                                  EventQueuePriority::MediumHigh);
}

void CompositorBridgeChild::AfterDestroy() {
  // Note that we cannot rely upon mCanSend here because we already set that to
  // false to prevent normal IPDL calls from being made after SendWillClose.
  // The only time we should not issue Send__delete__ is if the actor is already
  // destroyed, e.g. the compositor process crashed.
  if (!mActorDestroyed) {
    Send__delete__(this);
    mActorDestroyed = true;
  }

  if (mCanvasChild) {
    mCanvasChild->Destroy();
  }

  if (sCompositorBridge == this) {
    sCompositorBridge = nullptr;
  }
}

void CompositorBridgeChild::Destroy() {
  // This must not be called from the destructor!
  mTexturesWaitingNotifyNotUsed.clear();

  // Destroying the layer manager may cause all sorts of things to happen, so
  // let's make sure there is still a reference to keep this alive whatever
  // happens.
  RefPtr<CompositorBridgeChild> selfRef = this;

  for (size_t i = 0; i < mTexturePools.Length(); i++) {
    mTexturePools[i]->Destroy();
  }

  if (mSectionAllocator) {
    delete mSectionAllocator;
    mSectionAllocator = nullptr;
  }

  if (mLayerManager) {
    mLayerManager->Destroy();
    mLayerManager = nullptr;
  }

  // Flush async paints before we destroy texture data.
  FlushAsyncPaints();

  if (!mCanSend) {
    // We may have already called destroy but still have lingering references
    // or CompositorBridgeChild::ActorDestroy was called. Ensure that we do our
    // post destroy clean up no matter what. It is safe to call multiple times.
    NS_GetCurrentThread()->Dispatch(
        NewRunnableMethod("CompositorBridgeChild::PrepareFinalDestroy", selfRef,
                          &CompositorBridgeChild::PrepareFinalDestroy));
    return;
  }

  AutoTArray<PLayerTransactionChild*, 16> transactions;
  ManagedPLayerTransactionChild(transactions);
  for (int i = transactions.Length() - 1; i >= 0; --i) {
    RefPtr<LayerTransactionChild> layers =
        static_cast<LayerTransactionChild*>(transactions[i]);
    layers->Destroy();
  }

  AutoTArray<PWebRenderBridgeChild*, 16> wrBridges;
  ManagedPWebRenderBridgeChild(wrBridges);
  for (int i = wrBridges.Length() - 1; i >= 0; --i) {
    RefPtr<WebRenderBridgeChild> wrBridge =
        static_cast<WebRenderBridgeChild*>(wrBridges[i]);
    wrBridge->Destroy(/* aIsSync */ false);
  }

  AutoTArray<PAPZChild*, 16> apzChildren;
  ManagedPAPZChild(apzChildren);
  for (PAPZChild* child : apzChildren) {
    Unused << child->SendDestroy();
  }

  AutoTArray<PWebGPUChild*, 16> webGPUChildren;
  ManagedPWebGPUChild(webGPUChildren);
  for (PWebGPUChild* child : webGPUChildren) {
    Unused << child->SendShutdown();
  }

  const ManagedContainer<PTextureChild>& textures = ManagedPTextureChild();
  for (const auto& key : textures) {
    RefPtr<TextureClient> texture = TextureClient::AsTextureClient(key);

    if (texture) {
      texture->Destroy();
    }
  }

  // The WillClose message is synchronous, so we know that after it returns
  // any messages sent by the above code will have been processed on the
  // other side.
  SendWillClose();
  mCanSend = false;

  // We no longer care about unexpected shutdowns, in the remote process case.
  mProcessToken = 0;

  // The call just made to SendWillClose can result in IPC from the
  // CompositorBridgeParent to the CompositorBridgeChild (e.g. caused by the
  // destruction of shared memory). We need to ensure this gets processed by the
  // CompositorBridgeChild before it gets destroyed. It suffices to ensure that
  // events already in the thread get processed before the
  // CompositorBridgeChild is destroyed, so we add a task to the thread to
  // handle compositor destruction.

  // From now on we can't send any message message.
  NS_GetCurrentThread()->Dispatch(
      NewRunnableMethod("CompositorBridgeChild::PrepareFinalDestroy", selfRef,
                        &CompositorBridgeChild::PrepareFinalDestroy));
}

// static
void CompositorBridgeChild::ShutDown() {
  if (sCompositorBridge) {
    sCompositorBridge->Destroy();
    SpinEventLoopUntil([&]() { return !sCompositorBridge; });
  }
}

bool CompositorBridgeChild::LookupCompositorFrameMetrics(
    const ScrollableLayerGuid::ViewID aId, FrameMetrics& aFrame) {
  SharedFrameMetricsData* data = mFrameMetricsTable.Get(aId);
  if (data) {
    data->CopyFrameMetrics(&aFrame);
    return true;
  }
  return false;
}

void CompositorBridgeChild::InitForContent(uint32_t aNamespace) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aNamespace);

  if (RefPtr<CompositorBridgeChild> old = sCompositorBridge.forget()) {
    // Note that at this point, ActorDestroy may not have been called yet,
    // meaning mCanSend is still true. In this case we will try to send a
    // synchronous WillClose message to the parent, and will certainly get
    // a false result and a MsgDropped processing error. This is okay.
    old->Destroy();
  }

  mCanSend = true;
  mIdNamespace = aNamespace;

  sCompositorBridge = this;
}

void CompositorBridgeChild::InitForWidget(uint64_t aProcessToken,
                                          LayerManager* aLayerManager,
                                          uint32_t aNamespace) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aProcessToken);
  MOZ_ASSERT(aLayerManager);
  MOZ_ASSERT(aNamespace);

  mCanSend = true;
  mProcessToken = aProcessToken;
  mLayerManager = aLayerManager;
  mIdNamespace = aNamespace;
}

/*static*/
CompositorBridgeChild* CompositorBridgeChild::Get() {
  // This is only expected to be used in child processes. While the parent
  // process does have CompositorBridgeChild instances, it has _multiple_ (one
  // per window), and therefore there is no global singleton available.
  MOZ_ASSERT(!XRE_IsParentProcess());
  return sCompositorBridge;
}

// static
bool CompositorBridgeChild::ChildProcessHasCompositorBridge() {
  return sCompositorBridge != nullptr;
}

/* static */
bool CompositorBridgeChild::CompositorIsInGPUProcess() {
  MOZ_ASSERT(NS_IsMainThread());

  if (XRE_IsParentProcess()) {
    return !!GPUProcessManager::Get()->GetGPUChild();
  }

  MOZ_ASSERT(XRE_IsContentProcess());
  CompositorBridgeChild* bridge = CompositorBridgeChild::Get();
  if (!bridge) {
    return false;
  }

  return bridge->OtherPid() != dom::ContentChild::GetSingleton()->OtherPid();
}

PLayerTransactionChild* CompositorBridgeChild::AllocPLayerTransactionChild(
    const nsTArray<LayersBackend>& aBackendHints, const LayersId& aId) {
  LayerTransactionChild* c = new LayerTransactionChild(aId);
  c->AddIPDLReference();

  return c;
}

bool CompositorBridgeChild::DeallocPLayerTransactionChild(
    PLayerTransactionChild* actor) {
  LayersId childId = static_cast<LayerTransactionChild*>(actor)->GetId();
  ClearSharedFrameMetricsData(childId);
  static_cast<LayerTransactionChild*>(actor)->ReleaseIPDLReference();
  return true;
}

mozilla::ipc::IPCResult CompositorBridgeChild::RecvInvalidateLayers(
    const LayersId& aLayersId) {
  if (mLayerManager) {
    MOZ_ASSERT(!aLayersId.IsValid());
  } else if (aLayersId.IsValid()) {
    if (dom::BrowserChild* child = dom::BrowserChild::GetFrom(aLayersId)) {
      child->InvalidateLayers();
    }
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult CompositorBridgeChild::RecvDidComposite(
    const LayersId& aId, const nsTArray<TransactionId>& aTransactionIds,
    const TimeStamp& aCompositeStart, const TimeStamp& aCompositeEnd) {
  // Hold a reference to keep texture pools alive.  See bug 1387799
  const auto texturePools = mTexturePools.Clone();

  for (const auto& id : aTransactionIds) {
    if (mLayerManager) {
      MOZ_ASSERT(!aId.IsValid());
      MOZ_ASSERT(mLayerManager->GetBackendType() ==
                     LayersBackend::LAYERS_CLIENT ||
                 mLayerManager->GetBackendType() == LayersBackend::LAYERS_WR);
      // Hold a reference to keep LayerManager alive. See Bug 1242668.
      RefPtr<LayerManager> m = mLayerManager;
      m->DidComposite(id, aCompositeStart, aCompositeEnd);
    } else if (aId.IsValid()) {
      RefPtr<dom::BrowserChild> child = dom::BrowserChild::GetFrom(aId);
      if (child) {
        child->DidComposite(id, aCompositeStart, aCompositeEnd);
      }
    }
  }

  for (size_t i = 0; i < texturePools.Length(); i++) {
    texturePools[i]->ReturnDeferredClients();
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult CompositorBridgeChild::RecvNotifyFrameStats(
    nsTArray<FrameStats>&& aFrameStats) {
  gfxPlatform::GetPlatform()->NotifyFrameStats(std::move(aFrameStats));
  return IPC_OK();
}

void CompositorBridgeChild::ActorDestroy(ActorDestroyReason aWhy) {
  if (aWhy == AbnormalShutdown) {
    // If the parent side runs into a problem then the actor will be destroyed.
    // There is nothing we can do in the child side, here sets mCanSend as
    // false.
    gfxCriticalNote << "Receive IPC close with reason=AbnormalShutdown";
  }

  {
    // We take the lock to update these fields, since they are read from the
    // paint thread. We don't need the lock to init them, since that happens
    // on the main thread before the paint thread can ever grab a reference
    // to the CompositorBridge object.
    //
    // Note that it is useful to take this lock for one other reason: It also
    // tells us whether GetIPCChannel is safe to call. If we access the IPC
    // channel within this lock, when mCanSend is true, then we know it has not
    // been zapped by IPDL.
    MonitorAutoLock lock(mPaintLock);
    mCanSend = false;
    mActorDestroyed = true;
  }

  if (mProcessToken && XRE_IsParentProcess()) {
    GPUProcessManager::Get()->NotifyRemoteActorDestroyed(mProcessToken);
  }
}

mozilla::ipc::IPCResult CompositorBridgeChild::RecvSharedCompositorFrameMetrics(
    const mozilla::ipc::SharedMemoryBasic::Handle& metrics,
    const CrossProcessMutexHandle& handle, const LayersId& aLayersId,
    const uint32_t& aAPZCId) {
  auto data =
      MakeUnique<SharedFrameMetricsData>(metrics, handle, aLayersId, aAPZCId);
  const auto& viewID = data->GetViewID();
  mFrameMetricsTable.InsertOrUpdate(viewID, std::move(data));
  return IPC_OK();
}

mozilla::ipc::IPCResult
CompositorBridgeChild::RecvReleaseSharedCompositorFrameMetrics(
    const ViewID& aId, const uint32_t& aAPZCId) {
  if (auto entry = mFrameMetricsTable.Lookup(aId)) {
    // The SharedFrameMetricsData may have been removed previously if
    // a SharedFrameMetricsData with the same ViewID but later APZCId had
    // been store and over wrote it.
    if (entry.Data()->GetAPZCId() == aAPZCId) {
      entry.Remove();
    }
  }
  return IPC_OK();
}

CompositorBridgeChild::SharedFrameMetricsData::SharedFrameMetricsData(
    const ipc::SharedMemoryBasic::Handle& metrics,
    const CrossProcessMutexHandle& handle, const LayersId& aLayersId,
    const uint32_t& aAPZCId)
    : mMutex(nullptr), mLayersId(aLayersId), mAPZCId(aAPZCId) {
  mBuffer = new ipc::SharedMemoryBasic;
  mBuffer->SetHandle(metrics, ipc::SharedMemory::RightsReadOnly);
  mBuffer->Map(sizeof(FrameMetrics));
  mMutex = new CrossProcessMutex(handle);
  MOZ_COUNT_CTOR(SharedFrameMetricsData);
}

CompositorBridgeChild::SharedFrameMetricsData::~SharedFrameMetricsData() {
  // When the hash table deletes the class, delete
  // the shared memory and mutex.
  delete mMutex;
  mBuffer = nullptr;
  MOZ_COUNT_DTOR(SharedFrameMetricsData);
}

void CompositorBridgeChild::SharedFrameMetricsData::CopyFrameMetrics(
    FrameMetrics* aFrame) {
  const FrameMetrics* frame =
      static_cast<const FrameMetrics*>(mBuffer->memory());
  MOZ_ASSERT(frame);
  mMutex->Lock();
  *aFrame = *frame;
  mMutex->Unlock();
}

ScrollableLayerGuid::ViewID
CompositorBridgeChild::SharedFrameMetricsData::GetViewID() {
  const FrameMetrics* frame =
      static_cast<const FrameMetrics*>(mBuffer->memory());
  MOZ_ASSERT(frame);
  // Not locking to read of mScrollId since it should not change after being
  // initially set.
  return frame->GetScrollId();
}

LayersId CompositorBridgeChild::SharedFrameMetricsData::GetLayersId() const {
  return mLayersId;
}

uint32_t CompositorBridgeChild::SharedFrameMetricsData::GetAPZCId() {
  return mAPZCId;
}

bool CompositorBridgeChild::SendWillClose() {
  MOZ_RELEASE_ASSERT(mCanSend);
  return PCompositorBridgeChild::SendWillClose();
}

bool CompositorBridgeChild::SendPause() {
  if (!mCanSend) {
    return false;
  }
  return PCompositorBridgeChild::SendPause();
}

bool CompositorBridgeChild::SendResume() {
  if (!mCanSend) {
    return false;
  }
  return PCompositorBridgeChild::SendResume();
}

bool CompositorBridgeChild::SendResumeAsync() {
  if (!mCanSend) {
    return false;
  }
  return PCompositorBridgeChild::SendResumeAsync();
}

bool CompositorBridgeChild::SendNotifyChildCreated(
    const LayersId& id, CompositorOptions* aOptions) {
  if (!mCanSend) {
    return false;
  }
  return PCompositorBridgeChild::SendNotifyChildCreated(id, aOptions);
}

bool CompositorBridgeChild::SendAdoptChild(const LayersId& id) {
  if (!mCanSend) {
    return false;
  }
  return PCompositorBridgeChild::SendAdoptChild(id);
}

bool CompositorBridgeChild::SendMakeSnapshot(
    const SurfaceDescriptor& inSnapshot, const gfx::IntRect& dirtyRect) {
  if (!mCanSend) {
    return false;
  }
  return PCompositorBridgeChild::SendMakeSnapshot(inSnapshot, dirtyRect);
}

bool CompositorBridgeChild::SendFlushRendering() {
  if (!mCanSend) {
    return false;
  }
  return PCompositorBridgeChild::SendFlushRendering();
}

bool CompositorBridgeChild::SendStartFrameTimeRecording(
    const int32_t& bufferSize, uint32_t* startIndex) {
  if (!mCanSend) {
    return false;
  }
  return PCompositorBridgeChild::SendStartFrameTimeRecording(bufferSize,
                                                             startIndex);
}

bool CompositorBridgeChild::SendStopFrameTimeRecording(
    const uint32_t& startIndex, nsTArray<float>* intervals) {
  if (!mCanSend) {
    return false;
  }
  return PCompositorBridgeChild::SendStopFrameTimeRecording(startIndex,
                                                            intervals);
}

bool CompositorBridgeChild::SendNotifyRegionInvalidated(
    const nsIntRegion& region) {
  if (!mCanSend) {
    return false;
  }
  return PCompositorBridgeChild::SendNotifyRegionInvalidated(region);
}

PTextureChild* CompositorBridgeChild::AllocPTextureChild(
    const SurfaceDescriptor&, const ReadLockDescriptor&, const LayersBackend&,
    const TextureFlags&, const LayersId&, const uint64_t& aSerial,
    const wr::MaybeExternalImageId& aExternalImageId) {
  return TextureClient::CreateIPDLActor();
}

bool CompositorBridgeChild::DeallocPTextureChild(PTextureChild* actor) {
  return TextureClient::DestroyIPDLActor(actor);
}

mozilla::ipc::IPCResult CompositorBridgeChild::RecvParentAsyncMessages(
    nsTArray<AsyncParentMessageData>&& aMessages) {
  for (AsyncParentMessageArray::index_type i = 0; i < aMessages.Length(); ++i) {
    const AsyncParentMessageData& message = aMessages[i];

    switch (message.type()) {
      case AsyncParentMessageData::TOpNotifyNotUsed: {
        const OpNotifyNotUsed& op = message.get_OpNotifyNotUsed();
        NotifyNotUsed(op.TextureId(), op.fwdTransactionId());
        break;
      }
      case AsyncParentMessageData::TOpDeliverReleaseFence: {
        // Release fences are delivered via ImageBridge.
        // Since some TextureClients are recycled without recycle callback.
        MOZ_ASSERT_UNREACHABLE("unexpected to be called");
        break;
      }
      default:
        NS_ERROR("unknown AsyncParentMessageData type");
        return IPC_FAIL_NO_REASON(this);
    }
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult CompositorBridgeChild::RecvObserveLayersUpdate(
    const LayersId& aLayersId, const LayersObserverEpoch& aEpoch,
    const bool& aActive) {
  // This message is sent via the window compositor, not the tab compositor -
  // however it still has a layers id.
  MOZ_ASSERT(aLayersId.IsValid());
  MOZ_ASSERT(XRE_IsParentProcess());

  if (RefPtr<dom::BrowserParent> tab =
          dom::BrowserParent::GetBrowserParentFromLayersId(aLayersId)) {
    tab->LayerTreeUpdate(aEpoch, aActive);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult CompositorBridgeChild::RecvCompositorOptionsChanged(
    const LayersId& aLayersId, const CompositorOptions& aNewOptions) {
  MOZ_ASSERT(aLayersId.IsValid());
  MOZ_ASSERT(XRE_IsParentProcess());

  if (RefPtr<dom::BrowserParent> tab =
          dom::BrowserParent::GetBrowserParentFromLayersId(aLayersId)) {
    Unused << tab->SendCompositorOptionsChanged(aNewOptions);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult CompositorBridgeChild::RecvNotifyJankedAnimations(
    const LayersId& aLayersId, nsTArray<uint64_t>&& aJankedAnimations) {
  if (mLayerManager) {
    MOZ_ASSERT(!aLayersId.IsValid());
    mLayerManager->UpdatePartialPrerenderedAnimations(aJankedAnimations);
  } else if (aLayersId.IsValid()) {
    RefPtr<dom::BrowserChild> child = dom::BrowserChild::GetFrom(aLayersId);
    if (child) {
      child->NotifyJankedAnimations(aJankedAnimations);
    }
  }

  return IPC_OK();
}

void CompositorBridgeChild::HoldUntilCompositableRefReleasedIfNecessary(
    TextureClient* aClient) {
  if (!aClient) {
    return;
  }

#ifdef MOZ_WIDGET_ANDROID
  auto bufferId = aClient->GetInternalData()->GetBufferId();
  if (bufferId.isSome()) {
    MOZ_ASSERT(aClient->GetFlags() & TextureFlags::WAIT_HOST_USAGE_END);
    AndroidHardwareBufferManager::Get()->HoldUntilNotifyNotUsed(
        bufferId.ref(), GetFwdTransactionId(), /* aUsesImageBridge */ false);
  }
#endif

  bool waitNotifyNotUsed =
      aClient->GetFlags() & TextureFlags::RECYCLE ||
      aClient->GetFlags() & TextureFlags::WAIT_HOST_USAGE_END;
  if (!waitNotifyNotUsed) {
    return;
  }

  aClient->SetLastFwdTransactionId(GetFwdTransactionId());
  mTexturesWaitingNotifyNotUsed.emplace(aClient->GetSerial(), aClient);
}

void CompositorBridgeChild::NotifyNotUsed(uint64_t aTextureId,
                                          uint64_t aFwdTransactionId) {
  auto it = mTexturesWaitingNotifyNotUsed.find(aTextureId);
  if (it != mTexturesWaitingNotifyNotUsed.end()) {
    if (aFwdTransactionId < it->second->GetLastFwdTransactionId()) {
      // Released on host side, but client already requested newer use texture.
      return;
    }
    mTexturesWaitingNotifyNotUsed.erase(it);
  }
}

void CompositorBridgeChild::CancelWaitForNotifyNotUsed(uint64_t aTextureId) {
  mTexturesWaitingNotifyNotUsed.erase(aTextureId);
}

TextureClientPool* CompositorBridgeChild::GetTexturePool(
    KnowsCompositor* aAllocator, SurfaceFormat aFormat, TextureFlags aFlags) {
  for (size_t i = 0; i < mTexturePools.Length(); i++) {
    if (mTexturePools[i]->GetBackend() ==
            aAllocator->GetCompositorBackendType() &&
        mTexturePools[i]->GetMaxTextureSize() ==
            aAllocator->GetMaxTextureSize() &&
        mTexturePools[i]->GetFormat() == aFormat &&
        mTexturePools[i]->GetFlags() == aFlags) {
      return mTexturePools[i];
    }
  }

  mTexturePools.AppendElement(new TextureClientPool(
      aAllocator, aFormat, gfx::gfxVars::TileSize(), aFlags,
      StaticPrefs::layers_tile_pool_shrink_timeout_AtStartup(),
      StaticPrefs::layers_tile_pool_clear_timeout_AtStartup(),
      StaticPrefs::layers_tile_initial_pool_size_AtStartup(),
      StaticPrefs::layers_tile_pool_unused_size_AtStartup(), this));

  return mTexturePools.LastElement();
}

void CompositorBridgeChild::HandleMemoryPressure() {
  for (size_t i = 0; i < mTexturePools.Length(); i++) {
    mTexturePools[i]->Clear();
  }
}

void CompositorBridgeChild::ClearTexturePool() {
  for (size_t i = 0; i < mTexturePools.Length(); i++) {
    mTexturePools[i]->Clear();
  }
}

FixedSizeSmallShmemSectionAllocator*
CompositorBridgeChild::GetTileLockAllocator() {
  if (!IPCOpen()) {
    return nullptr;
  }

  if (!mSectionAllocator) {
    mSectionAllocator = new FixedSizeSmallShmemSectionAllocator(this);
  }
  return mSectionAllocator;
}

PTextureChild* CompositorBridgeChild::CreateTexture(
    const SurfaceDescriptor& aSharedData, const ReadLockDescriptor& aReadLock,
    LayersBackend aLayersBackend, TextureFlags aFlags, uint64_t aSerial,
    wr::MaybeExternalImageId& aExternalImageId, nsISerialEventTarget* aTarget) {
  PTextureChild* textureChild =
      AllocPTextureChild(aSharedData, aReadLock, aLayersBackend, aFlags,
                         LayersId{0} /* FIXME */, aSerial, aExternalImageId);

  // Do the DOM labeling.
  if (aTarget) {
    SetEventTargetForActor(textureChild, aTarget);
  }

  return SendPTextureConstructor(
      textureChild, aSharedData, aReadLock, aLayersBackend, aFlags,
      LayersId{0} /* FIXME? */, aSerial, aExternalImageId);
}

already_AddRefed<CanvasChild> CompositorBridgeChild::GetCanvasChild() {
  MOZ_ASSERT(gfx::gfxVars::RemoteCanvasEnabled());

  if (CanvasChild::Deactivated()) {
    return nullptr;
  }

  if (!mCanvasChild) {
    ipc::Endpoint<PCanvasParent> parentEndpoint;
    ipc::Endpoint<PCanvasChild> childEndpoint;
    nsresult rv = PCanvas::CreateEndpoints(OtherPid(), base::GetCurrentProcId(),
                                           &parentEndpoint, &childEndpoint);
    if (NS_SUCCEEDED(rv)) {
      Unused << SendInitPCanvasParent(std::move(parentEndpoint));
      mCanvasChild = new CanvasChild(std::move(childEndpoint));
    }
  }

  return do_AddRef(mCanvasChild);
}

void CompositorBridgeChild::EndCanvasTransaction() {
  if (mCanvasChild) {
    mCanvasChild->EndTransaction();
    if (mCanvasChild->ShouldBeCleanedUp()) {
      mCanvasChild->Destroy();
      Unused << SendReleasePCanvasParent();
      mCanvasChild = nullptr;
    }
  }
}

RefPtr<webgpu::WebGPUChild> CompositorBridgeChild::GetWebGPUChild() {
  MOZ_ASSERT(gfx::gfxConfig::IsEnabled(gfx::Feature::WEBGPU));
  if (!mWebGPUChild) {
    webgpu::PWebGPUChild* bridge = SendPWebGPUConstructor();
    mWebGPUChild = static_cast<webgpu::WebGPUChild*>(bridge);
  }

  return mWebGPUChild;
}

bool CompositorBridgeChild::AllocUnsafeShmem(
    size_t aSize, ipc::SharedMemory::SharedMemoryType aType,
    ipc::Shmem* aShmem) {
  ShmemAllocated(this);
  return PCompositorBridgeChild::AllocUnsafeShmem(aSize, aType, aShmem);
}

bool CompositorBridgeChild::AllocShmem(
    size_t aSize, ipc::SharedMemory::SharedMemoryType aType,
    ipc::Shmem* aShmem) {
  ShmemAllocated(this);
  return PCompositorBridgeChild::AllocShmem(aSize, aType, aShmem);
}

bool CompositorBridgeChild::DeallocShmem(ipc::Shmem& aShmem) {
  if (!mCanSend) {
    return false;
  }
  return PCompositorBridgeChild::DeallocShmem(aShmem);
}

widget::PCompositorWidgetChild*
CompositorBridgeChild::AllocPCompositorWidgetChild(
    const CompositorWidgetInitData& aInitData) {
  // We send the constructor manually.
  MOZ_CRASH("Should not be called");
  return nullptr;
}

bool CompositorBridgeChild::DeallocPCompositorWidgetChild(
    PCompositorWidgetChild* aActor) {
#ifdef MOZ_WIDGET_SUPPORTS_OOP_COMPOSITING
  delete aActor;
  return true;
#else
  return false;
#endif
}

PAPZCTreeManagerChild* CompositorBridgeChild::AllocPAPZCTreeManagerChild(
    const LayersId& aLayersId) {
  APZCTreeManagerChild* child = new APZCTreeManagerChild();
  child->AddIPDLReference();

  return child;
}

PAPZChild* CompositorBridgeChild::AllocPAPZChild(const LayersId& aLayersId) {
  // We send the constructor manually.
  MOZ_CRASH("Should not be called");
  return nullptr;
}

bool CompositorBridgeChild::DeallocPAPZChild(PAPZChild* aActor) {
  delete aActor;
  return true;
}

bool CompositorBridgeChild::DeallocPAPZCTreeManagerChild(
    PAPZCTreeManagerChild* aActor) {
  APZCTreeManagerChild* child = static_cast<APZCTreeManagerChild*>(aActor);
  child->ReleaseIPDLReference();
  return true;
}

// -

void CompositorBridgeChild::WillEndTransaction() { ResetShmemCounter(); }

PWebRenderBridgeChild* CompositorBridgeChild::AllocPWebRenderBridgeChild(
    const wr::PipelineId& aPipelineId, const LayoutDeviceIntSize&,
    const WindowKind&) {
  WebRenderBridgeChild* child = new WebRenderBridgeChild(aPipelineId);
  child->AddIPDLReference();
  return child;
}

bool CompositorBridgeChild::DeallocPWebRenderBridgeChild(
    PWebRenderBridgeChild* aActor) {
  WebRenderBridgeChild* child = static_cast<WebRenderBridgeChild*>(aActor);
  ClearSharedFrameMetricsData(wr::AsLayersId(child->GetPipeline()));
  child->ReleaseIPDLReference();
  return true;
}

webgpu::PWebGPUChild* CompositorBridgeChild::AllocPWebGPUChild() {
  webgpu::WebGPUChild* child = new webgpu::WebGPUChild();
  child->AddIPDLReference();
  return child;
}

bool CompositorBridgeChild::DeallocPWebGPUChild(webgpu::PWebGPUChild* aActor) {
  webgpu::WebGPUChild* child = static_cast<webgpu::WebGPUChild*>(aActor);
  child->ReleaseIPDLReference();
  return true;
}

void CompositorBridgeChild::ClearSharedFrameMetricsData(LayersId aLayersId) {
  for (auto iter = mFrameMetricsTable.Iter(); !iter.Done(); iter.Next()) {
    auto data = iter.UserData();
    if (data->GetLayersId() == aLayersId) {
      iter.Remove();
    }
  }
}

uint64_t CompositorBridgeChild::GetNextResourceId() {
  ++mResourceId;
  MOZ_RELEASE_ASSERT(mResourceId != UINT32_MAX);

  uint64_t id = mIdNamespace;
  id = (id << 32) | mResourceId;

  return id;
}

wr::MaybeExternalImageId CompositorBridgeChild::GetNextExternalImageId() {
  return Some(wr::ToExternalImageId(GetNextResourceId()));
}

wr::PipelineId CompositorBridgeChild::GetNextPipelineId() {
  return wr::AsPipelineId(GetNextResourceId());
}

void CompositorBridgeChild::FlushAsyncPaints() {
  MOZ_ASSERT(NS_IsMainThread());

  Maybe<TimeStamp> start;
  if (XRE_IsContentProcess() && gfx::gfxVars::UseOMTP()) {
    start = Some(TimeStamp::Now());
  }

  {
    MonitorAutoLock lock(mPaintLock);
    while (mOutstandingAsyncPaints > 0 || mOutstandingAsyncEndTransaction) {
      lock.Wait();
    }

    // It's now safe to free any TextureClients that were used during painting.
    mTextureClientsForAsyncPaint.Clear();
  }

  if (start) {
    float ms = (TimeStamp::Now() - start.value()).ToMilliseconds();

    // Anything above 200us gets recorded.
    if (ms >= 0.2) {
      mSlowFlushCount++;
      Telemetry::Accumulate(Telemetry::GFX_OMTP_PAINT_WAIT_TIME, int32_t(ms));
    }
    mTotalFlushCount++;

    double ratio = double(mSlowFlushCount) / double(mTotalFlushCount);
    Telemetry::ScalarSet(Telemetry::ScalarID::GFX_OMTP_PAINT_WAIT_RATIO,
                         uint32_t(ratio * 100 * 100));
  }
}

void CompositorBridgeChild::NotifyBeginAsyncPaint(PaintTask* aTask) {
  MOZ_ASSERT(NS_IsMainThread());

  MonitorAutoLock lock(mPaintLock);

  if (mTotalAsyncPaints == 0) {
    mAsyncTransactionBegin = TimeStamp::Now();
  }
  mTotalAsyncPaints += 1;

  // We must not be waiting for paints or buffer copying to complete yet. This
  // would imply we started a new paint without waiting for a previous one,
  // which could lead to incorrect rendering or IPDL deadlocks.
  MOZ_ASSERT(!mIsDelayingForAsyncPaints);

  mOutstandingAsyncPaints++;

  // Mark texture clients that they are being used for async painting, and
  // make sure we hold them alive on the main thread.
  for (auto& client : aTask->mClients) {
    client->AddPaintThreadRef();
    mTextureClientsForAsyncPaint.AppendElement(client);
  };
}

// Must only be called from the paint thread. Notifies the CompositorBridge
// that the paint thread has finished an asynchronous paint request.
bool CompositorBridgeChild::NotifyFinishedAsyncWorkerPaint(PaintTask* aTask) {
  MOZ_ASSERT(PaintThread::Get()->IsOnPaintWorkerThread());

  MonitorAutoLock lock(mPaintLock);
  mOutstandingAsyncPaints--;

  for (auto& client : aTask->mClients) {
    client->DropPaintThreadRef();
  };
  aTask->DropTextureClients();

  // If the main thread has completed queuing work and this was the
  // last paint, then it is time to end the layer transaction and sync
  return mOutstandingAsyncEndTransaction && mOutstandingAsyncPaints == 0;
}

bool CompositorBridgeChild::NotifyBeginAsyncEndLayerTransaction(
    SyncObjectClient* aSyncObject) {
  MOZ_ASSERT(NS_IsMainThread());
  MonitorAutoLock lock(mPaintLock);

  MOZ_ASSERT(!mOutstandingAsyncEndTransaction);
  mOutstandingAsyncEndTransaction = true;
  mOutstandingAsyncSyncObject = aSyncObject;
  return mOutstandingAsyncPaints == 0;
}

void CompositorBridgeChild::NotifyFinishedAsyncEndLayerTransaction() {
  MOZ_ASSERT(PaintThread::Get()->IsOnPaintWorkerThread());

  if (mOutstandingAsyncSyncObject) {
    mOutstandingAsyncSyncObject->Synchronize();
    mOutstandingAsyncSyncObject = nullptr;
  }

  MonitorAutoLock lock(mPaintLock);

  if (mTotalAsyncPaints > 0) {
    float tenthMs =
        (TimeStamp::Now() - mAsyncTransactionBegin).ToMilliseconds() * 10;
    Telemetry::Accumulate(Telemetry::GFX_OMTP_PAINT_TASK_COUNT,
                          int32_t(mTotalAsyncPaints));
    Telemetry::Accumulate(Telemetry::GFX_OMTP_PAINT_TIME, int32_t(tenthMs));
    mTotalAsyncPaints = 0;
  }

  // Since this should happen after ALL paints are done and
  // at the end of a transaction, this should always be true.
  MOZ_RELEASE_ASSERT(mOutstandingAsyncPaints == 0);
  MOZ_ASSERT(mOutstandingAsyncEndTransaction);

  mOutstandingAsyncEndTransaction = false;

  // It's possible that we painted so fast that the main thread never reached
  // the code that starts delaying messages. If so, mIsDelayingForAsyncPaints
  // will be false, and we can safely return.
  if (mIsDelayingForAsyncPaints) {
    ResumeIPCAfterAsyncPaint();
  }

  // Notify the main thread in case it's blocking. We do this unconditionally
  // to avoid deadlocking.
  lock.Notify();
}

void CompositorBridgeChild::ResumeIPCAfterAsyncPaint() {
  // Note: the caller is responsible for holding the lock.
  mPaintLock.AssertCurrentThreadOwns();
  MOZ_ASSERT(PaintThread::Get()->IsOnPaintWorkerThread());
  MOZ_ASSERT(mOutstandingAsyncPaints == 0);
  MOZ_ASSERT(!mOutstandingAsyncEndTransaction);
  MOZ_ASSERT(mIsDelayingForAsyncPaints);

  mIsDelayingForAsyncPaints = false;

  // It's also possible that the channel has shut down already.
  if (!mCanSend || mActorDestroyed) {
    return;
  }

  GetIPCChannel()->StopPostponingSends();
}

void CompositorBridgeChild::PostponeMessagesIfAsyncPainting() {
  MOZ_ASSERT(NS_IsMainThread());

  MonitorAutoLock lock(mPaintLock);

  MOZ_ASSERT(!mIsDelayingForAsyncPaints);

  // We need to wait for async paints and the async end transaction as
  // it will do texture synchronization
  if (mOutstandingAsyncPaints > 0 || mOutstandingAsyncEndTransaction) {
    mIsDelayingForAsyncPaints = true;
    GetIPCChannel()->BeginPostponingSends();
  }
}

}  // namespace layers
}  // namespace mozilla
