/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImageBridgeChild.h"

#include <vector>  // for vector

#include "ImageBridgeParent.h"  // for ImageBridgeParent
#include "ImageContainer.h"     // for ImageContainer
#include "Layers.h"             // for Layer, etc
#include "SynchronousTask.h"
#include "mozilla/Assertions.h"        // for MOZ_ASSERT, etc
#include "mozilla/Monitor.h"           // for Monitor, MonitorAutoLock
#include "mozilla/ReentrantMonitor.h"  // for ReentrantMonitor, etc
#include "mozilla/StaticMutex.h"
#include "mozilla/StaticPtr.h"  // for StaticRefPtr
#include "mozilla/dom/ContentChild.h"
#include "mozilla/gfx/Point.h"  // for IntSize
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/ipc/Endpoint.h"
#include "mozilla/ipc/MessageChannel.h"         // for MessageChannel, etc
#include "mozilla/layers/CompositableClient.h"  // for CompositableChild, etc
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/layers/ISurfaceAllocator.h"  // for ISurfaceAllocator
#include "mozilla/layers/ImageClient.h"        // for ImageClient
#include "mozilla/layers/LayersMessages.h"     // for CompositableOperation
#include "mozilla/layers/TextureClient.h"      // for TextureClient
#include "mozilla/layers/TextureClient.h"
#include "mozilla/media/MediaSystemResourceManager.h"  // for MediaSystemResourceManager
#include "mozilla/media/MediaSystemResourceManagerChild.h"  // for MediaSystemResourceManagerChild
#include "mozilla/mozalloc.h"  // for operator new, etc
#include "transport/runnable_utils.h"
#include "nsContentUtils.h"
#include "nsISupportsImpl.h"         // for ImageContainer::AddRef, etc
#include "nsTArray.h"                // for AutoTArray, nsTArray, etc
#include "nsTArrayForwardDeclare.h"  // for AutoTArray
#include "nsThreadUtils.h"           // for NS_IsMainThread

#if defined(XP_WIN)
#  include "mozilla/gfx/DeviceManagerDx.h"
#endif

#ifdef MOZ_WIDGET_ANDROID
#  include "mozilla/layers/AndroidHardwareBuffer.h"
#endif

namespace mozilla {
namespace ipc {
class Shmem;
}  // namespace ipc

namespace layers {

using namespace mozilla::ipc;
using namespace mozilla::gfx;
using namespace mozilla::media;

typedef std::vector<CompositableOperation> OpVector;
typedef nsTArray<OpDestroy> OpDestroyVector;

struct CompositableTransaction {
  CompositableTransaction() : mFinished(true) {}
  ~CompositableTransaction() { End(); }
  bool Finished() const { return mFinished; }
  void Begin() {
    MOZ_ASSERT(mFinished);
    mFinished = false;
  }
  void End() {
    mFinished = true;
    mOperations.clear();
    mDestroyedActors.Clear();
  }
  bool IsEmpty() const {
    return mOperations.empty() && mDestroyedActors.IsEmpty();
  }
  void AddNoSwapEdit(const CompositableOperation& op) {
    MOZ_ASSERT(!Finished(), "forgot BeginTransaction?");
    mOperations.push_back(op);
  }

  OpVector mOperations;
  OpDestroyVector mDestroyedActors;

  bool mFinished;
};

struct AutoEndTransaction final {
  explicit AutoEndTransaction(CompositableTransaction* aTxn) : mTxn(aTxn) {}
  ~AutoEndTransaction() { mTxn->End(); }
  CompositableTransaction* mTxn;
};

void ImageBridgeChild::UseTextures(
    CompositableClient* aCompositable,
    const nsTArray<TimedTextureClient>& aTextures) {
  MOZ_ASSERT(aCompositable);
  MOZ_ASSERT(aCompositable->GetIPCHandle());
  MOZ_ASSERT(aCompositable->IsConnected());

  AutoTArray<TimedTexture, 4> textures;

  for (auto& t : aTextures) {
    MOZ_ASSERT(t.mTextureClient);
    MOZ_ASSERT(t.mTextureClient->GetIPDLActor());

    if (!t.mTextureClient->IsSharedWithCompositor()) {
      return;
    }

    bool readLocked = t.mTextureClient->OnForwardedToHost();

    auto fenceFd = t.mTextureClient->GetInternalData()->GetAcquireFence();
    if (fenceFd.IsValid()) {
      mTxn->AddNoSwapEdit(CompositableOperation(
          aCompositable->GetIPCHandle(),
          OpDeliverAcquireFence(nullptr, t.mTextureClient->GetIPDLActor(),
                                fenceFd)));
    }

    textures.AppendElement(
        TimedTexture(nullptr, t.mTextureClient->GetIPDLActor(), t.mTimeStamp,
                     t.mPictureRect, t.mFrameID, t.mProducerID, readLocked));

    // Wait end of usage on host side if TextureFlags::RECYCLE is set
    HoldUntilCompositableRefReleasedIfNecessary(t.mTextureClient);
  }
  mTxn->AddNoSwapEdit(CompositableOperation(aCompositable->GetIPCHandle(),
                                            OpUseTexture(textures)));
}

void ImageBridgeChild::HoldUntilCompositableRefReleasedIfNecessary(
    TextureClient* aClient) {
  if (!aClient) {
    return;
  }

#ifdef MOZ_WIDGET_ANDROID
  auto bufferId = aClient->GetInternalData()->GetBufferId();
  if (bufferId.isSome()) {
    MOZ_ASSERT(aClient->GetFlags() & TextureFlags::WAIT_HOST_USAGE_END);
    AndroidHardwareBufferManager::Get()->HoldUntilNotifyNotUsed(
        bufferId.ref(), GetFwdTransactionId(), /* aUsesImageBridge */ true);
  }
#endif

  // Wait ReleaseCompositableRef only when TextureFlags::RECYCLE or
  // TextureFlags::WAIT_HOST_USAGE_END is set on ImageBridge.
  bool waitNotifyNotUsed =
      aClient->GetFlags() & TextureFlags::RECYCLE ||
      aClient->GetFlags() & TextureFlags::WAIT_HOST_USAGE_END;
  if (!waitNotifyNotUsed) {
    return;
  }

  aClient->SetLastFwdTransactionId(GetFwdTransactionId());
  mTexturesWaitingNotifyNotUsed.emplace(aClient->GetSerial(), aClient);
}

void ImageBridgeChild::NotifyNotUsed(uint64_t aTextureId,
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

void ImageBridgeChild::CancelWaitForNotifyNotUsed(uint64_t aTextureId) {
  MOZ_ASSERT(InImageBridgeChildThread());
  mTexturesWaitingNotifyNotUsed.erase(aTextureId);
}

// Singleton
static StaticMutex sImageBridgeSingletonLock MOZ_UNANNOTATED;
static StaticRefPtr<ImageBridgeChild> sImageBridgeChildSingleton;
static StaticRefPtr<nsIThread> sImageBridgeChildThread;

// dispatched function
void ImageBridgeChild::ShutdownStep1(SynchronousTask* aTask) {
  AutoCompleteTask complete(aTask);

  MOZ_ASSERT(InImageBridgeChildThread(),
             "Should be in ImageBridgeChild thread.");

  MediaSystemResourceManager::Shutdown();

  // Force all managed protocols to shut themselves down cleanly
  nsTArray<PTextureChild*> textures;
  ManagedPTextureChild(textures);
  for (int i = textures.Length() - 1; i >= 0; --i) {
    RefPtr<TextureClient> client = TextureClient::AsTextureClient(textures[i]);
    if (client) {
      client->Destroy();
    }
  }

  if (mCanSend) {
    SendWillClose();
  }
  MarkShutDown();

  // From now on, no message can be sent through the image bridge from the
  // client side except the final Stop message.
}

// dispatched function
void ImageBridgeChild::ShutdownStep2(SynchronousTask* aTask) {
  AutoCompleteTask complete(aTask);

  MOZ_ASSERT(InImageBridgeChildThread(),
             "Should be in ImageBridgeChild thread.");
  if (!mDestroyed) {
    Close();
  }
}

void ImageBridgeChild::ActorDestroy(ActorDestroyReason aWhy) {
  mCanSend = false;
  mDestroyed = true;
  {
    MutexAutoLock lock(mContainerMapLock);
    mImageContainerListeners.clear();
  }
}

void ImageBridgeChild::ActorDealloc() { this->Release(); }

void ImageBridgeChild::CreateImageClientSync(SynchronousTask* aTask,
                                             RefPtr<ImageClient>* result,
                                             CompositableType aType,
                                             ImageContainer* aImageContainer) {
  AutoCompleteTask complete(aTask);
  *result = CreateImageClientNow(aType, aImageContainer);
}

ImageBridgeChild::ImageBridgeChild(uint32_t aNamespace)
    : mNamespace(aNamespace),
      mCanSend(false),
      mDestroyed(false),
      mFwdTransactionId(0),
      mContainerMapLock("ImageBridgeChild.mContainerMapLock") {
  MOZ_ASSERT(mNamespace);
  MOZ_ASSERT(NS_IsMainThread());

  mTxn = new CompositableTransaction();
}

ImageBridgeChild::~ImageBridgeChild() { delete mTxn; }

void ImageBridgeChild::MarkShutDown() {
  mTexturesWaitingNotifyNotUsed.clear();

  mCanSend = false;
}

void ImageBridgeChild::Connect(CompositableClient* aCompositable,
                               ImageContainer* aImageContainer) {
  MOZ_ASSERT(aCompositable);
  MOZ_ASSERT(InImageBridgeChildThread());
  MOZ_ASSERT(CanSend());

  // Note: this is static, rather than per-IBC, so IDs are not re-used across
  // ImageBridgeChild instances. This is relevant for the GPU process, where
  // we don't want old IDs to potentially leak into a recreated ImageBridge.
  static uint64_t sNextID = 1;
  uint64_t id = sNextID++;

  // ImageClient of ImageContainer provides aImageContainer.
  // But offscreen canvas does not provide it.
  if (aImageContainer) {
    MutexAutoLock lock(mContainerMapLock);
    MOZ_ASSERT(mImageContainerListeners.find(id) ==
               mImageContainerListeners.end());
    mImageContainerListeners.emplace(
        id, aImageContainer->GetImageContainerListener());
  }

  CompositableHandle handle(id);
  aCompositable->InitIPDL(handle);
  SendNewCompositable(handle, aCompositable->GetTextureInfo());
}

void ImageBridgeChild::ForgetImageContainer(const CompositableHandle& aHandle) {
  MutexAutoLock lock(mContainerMapLock);
  mImageContainerListeners.erase(aHandle.Value());
}

/* static */
RefPtr<ImageBridgeChild> ImageBridgeChild::GetSingleton() {
  StaticMutexAutoLock lock(sImageBridgeSingletonLock);
  return sImageBridgeChildSingleton;
}

void ImageBridgeChild::UpdateImageClient(RefPtr<ImageContainer> aContainer) {
  if (!aContainer) {
    return;
  }

  if (!InImageBridgeChildThread()) {
    RefPtr<Runnable> runnable =
        WrapRunnable(RefPtr<ImageBridgeChild>(this),
                     &ImageBridgeChild::UpdateImageClient, aContainer);
    GetThread()->Dispatch(runnable.forget());
    return;
  }

  if (!CanSend()) {
    return;
  }

  RefPtr<ImageClient> client = aContainer->GetImageClient();
  if (NS_WARN_IF(!client)) {
    return;
  }

  // If the client has become disconnected before this event was dispatched,
  // early return now.
  if (!client->IsConnected()) {
    return;
  }

  BeginTransaction();
  client->UpdateImage(aContainer, Layer::CONTENT_OPAQUE);
  EndTransaction();
}

void ImageBridgeChild::FlushAllImagesSync(SynchronousTask* aTask,
                                          ImageClient* aClient,
                                          ImageContainer* aContainer) {
  AutoCompleteTask complete(aTask);

  if (!CanSend()) {
    return;
  }

  MOZ_ASSERT(aClient);
  BeginTransaction();
  if (aContainer) {
    aContainer->ClearImagesFromImageBridge();
  }
  aClient->FlushAllImages();
  EndTransaction();
}

void ImageBridgeChild::FlushAllImages(ImageClient* aClient,
                                      ImageContainer* aContainer) {
  MOZ_ASSERT(aClient);
  MOZ_ASSERT(!InImageBridgeChildThread());

  if (InImageBridgeChildThread()) {
    NS_ERROR(
        "ImageBridgeChild::FlushAllImages() is called on ImageBridge thread.");
    return;
  }

  SynchronousTask task("FlushAllImages Lock");

  // RefPtrs on arguments are not needed since this dispatches synchronously.
  RefPtr<Runnable> runnable = WrapRunnable(
      RefPtr<ImageBridgeChild>(this), &ImageBridgeChild::FlushAllImagesSync,
      &task, aClient, aContainer);
  GetThread()->Dispatch(runnable.forget());

  task.Wait();
}

void ImageBridgeChild::BeginTransaction() {
  MOZ_ASSERT(CanSend());
  MOZ_ASSERT(mTxn->Finished(), "uncommitted txn?");
  UpdateFwdTransactionId();
  mTxn->Begin();
}

void ImageBridgeChild::EndTransaction() {
  MOZ_ASSERT(CanSend());
  MOZ_ASSERT(!mTxn->Finished(), "forgot BeginTransaction?");

  AutoEndTransaction _(mTxn);

  if (mTxn->IsEmpty()) {
    return;
  }

  AutoTArray<CompositableOperation, 10> cset;
  cset.SetCapacity(mTxn->mOperations.size());
  if (!mTxn->mOperations.empty()) {
    cset.AppendElements(&mTxn->mOperations.front(), mTxn->mOperations.size());
  }

  if (!SendUpdate(cset, mTxn->mDestroyedActors, GetFwdTransactionId())) {
    NS_WARNING("could not send async texture transaction");
    return;
  }
}

bool ImageBridgeChild::InitForContent(Endpoint<PImageBridgeChild>&& aEndpoint,
                                      uint32_t aNamespace) {
  MOZ_ASSERT(NS_IsMainThread());

  gfxPlatform::GetPlatform();

  if (!sImageBridgeChildThread) {
    nsCOMPtr<nsIThread> thread;
    nsresult rv = NS_NewNamedThread("ImageBridgeChld", getter_AddRefs(thread));
    MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv),
                       "Failed to start ImageBridgeChild thread!");
    sImageBridgeChildThread = thread.forget();
  }

  RefPtr<ImageBridgeChild> child = new ImageBridgeChild(aNamespace);

  child->GetThread()->Dispatch(NS_NewRunnableFunction(
      "layers::ImageBridgeChild::Bind",
      [child, endpoint = std::move(aEndpoint)]() mutable {
        child->Bind(std::move(endpoint));
      }));

  // Assign this after so other threads can't post messages before we connect to
  // IPDL.
  {
    StaticMutexAutoLock lock(sImageBridgeSingletonLock);
    sImageBridgeChildSingleton = child;
  }

  return true;
}

bool ImageBridgeChild::ReinitForContent(Endpoint<PImageBridgeChild>&& aEndpoint,
                                        uint32_t aNamespace) {
  MOZ_ASSERT(NS_IsMainThread());

  // Note that at this point, ActorDestroy may not have been called yet,
  // meaning mCanSend is still true. In this case we will try to send a
  // synchronous WillClose message to the parent, and will certainly get a
  // false result and a MsgDropped processing error. This is okay.
  ShutdownSingleton();

  return InitForContent(std::move(aEndpoint), aNamespace);
}

void ImageBridgeChild::Bind(Endpoint<PImageBridgeChild>&& aEndpoint) {
  if (!aEndpoint.Bind(this)) {
    return;
  }

  // This reference is dropped in DeallocPImageBridgeChild.
  this->AddRef();

  mCanSend = true;
}

void ImageBridgeChild::BindSameProcess(RefPtr<ImageBridgeParent> aParent) {
  Open(aParent, aParent->GetThread(), mozilla::ipc::ChildSide);

  // This reference is dropped in DeallocPImageBridgeChild.
  this->AddRef();

  mCanSend = true;
}

/* static */
void ImageBridgeChild::ShutDown() {
  MOZ_ASSERT(NS_IsMainThread());

  ShutdownSingleton();

  if (sImageBridgeChildThread) {
    sImageBridgeChildThread->Shutdown();
    sImageBridgeChildThread = nullptr;
  }
}

/* static */
void ImageBridgeChild::ShutdownSingleton() {
  MOZ_ASSERT(NS_IsMainThread());

  if (RefPtr<ImageBridgeChild> child = GetSingleton()) {
    child->WillShutdown();

    StaticMutexAutoLock lock(sImageBridgeSingletonLock);
    sImageBridgeChildSingleton = nullptr;
  }
}

void ImageBridgeChild::WillShutdown() {
  {
    SynchronousTask task("ImageBridge ShutdownStep1 lock");

    RefPtr<Runnable> runnable =
        WrapRunnable(RefPtr<ImageBridgeChild>(this),
                     &ImageBridgeChild::ShutdownStep1, &task);
    GetThread()->Dispatch(runnable.forget());

    task.Wait();
  }

  {
    SynchronousTask task("ImageBridge ShutdownStep2 lock");

    RefPtr<Runnable> runnable =
        WrapRunnable(RefPtr<ImageBridgeChild>(this),
                     &ImageBridgeChild::ShutdownStep2, &task);
    GetThread()->Dispatch(runnable.forget());

    task.Wait();
  }
}

void ImageBridgeChild::InitSameProcess(uint32_t aNamespace) {
  NS_ASSERTION(NS_IsMainThread(), "Should be on the main Thread!");

  MOZ_ASSERT(!sImageBridgeChildSingleton);
  MOZ_ASSERT(!sImageBridgeChildThread);

  nsCOMPtr<nsIThread> thread;
  nsresult rv = NS_NewNamedThread("ImageBridgeChld", getter_AddRefs(thread));
  MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv),
                     "Failed to start ImageBridgeChild thread!");
  sImageBridgeChildThread = thread.forget();

  RefPtr<ImageBridgeChild> child = new ImageBridgeChild(aNamespace);
  RefPtr<ImageBridgeParent> parent = ImageBridgeParent::CreateSameProcess();

  RefPtr<Runnable> runnable =
      WrapRunnable(child, &ImageBridgeChild::BindSameProcess, parent);
  child->GetThread()->Dispatch(runnable.forget());

  // Assign this after so other threads can't post messages before we connect to
  // IPDL.
  {
    StaticMutexAutoLock lock(sImageBridgeSingletonLock);
    sImageBridgeChildSingleton = child;
  }
}

/* static */
void ImageBridgeChild::InitWithGPUProcess(
    Endpoint<PImageBridgeChild>&& aEndpoint, uint32_t aNamespace) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!sImageBridgeChildSingleton);
  MOZ_ASSERT(!sImageBridgeChildThread);

  nsCOMPtr<nsIThread> thread;
  nsresult rv = NS_NewNamedThread("ImageBridgeChld", getter_AddRefs(thread));
  MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv),
                     "Failed to start ImageBridgeChild thread!");
  sImageBridgeChildThread = thread.forget();

  RefPtr<ImageBridgeChild> child = new ImageBridgeChild(aNamespace);

  child->GetThread()->Dispatch(NS_NewRunnableFunction(
      "layers::ImageBridgeChild::Bind",
      [child, endpoint = std::move(aEndpoint)]() mutable {
        child->Bind(std::move(endpoint));
      }));

  // Assign this after so other threads can't post messages before we connect to
  // IPDL.
  {
    StaticMutexAutoLock lock(sImageBridgeSingletonLock);
    sImageBridgeChildSingleton = child;
  }
}

bool InImageBridgeChildThread() {
  return sImageBridgeChildThread &&
         sImageBridgeChildThread->IsOnCurrentThread();
}

nsISerialEventTarget* ImageBridgeChild::GetThread() const {
  return sImageBridgeChildThread;
}

/* static */
void ImageBridgeChild::IdentifyCompositorTextureHost(
    const TextureFactoryIdentifier& aIdentifier) {
  if (RefPtr<ImageBridgeChild> child = GetSingleton()) {
    child->UpdateTextureFactoryIdentifier(aIdentifier);
  }
}

void ImageBridgeChild::UpdateTextureFactoryIdentifier(
    const TextureFactoryIdentifier& aIdentifier) {
  IdentifyTextureHost(aIdentifier);
}

RefPtr<ImageClient> ImageBridgeChild::CreateImageClient(
    CompositableType aType, ImageContainer* aImageContainer) {
  if (InImageBridgeChildThread()) {
    return CreateImageClientNow(aType, aImageContainer);
  }

  SynchronousTask task("CreateImageClient Lock");

  RefPtr<ImageClient> result = nullptr;

  RefPtr<Runnable> runnable = WrapRunnable(
      RefPtr<ImageBridgeChild>(this), &ImageBridgeChild::CreateImageClientSync,
      &task, &result, aType, aImageContainer);
  GetThread()->Dispatch(runnable.forget());

  task.Wait();

  return result;
}

RefPtr<ImageClient> ImageBridgeChild::CreateImageClientNow(
    CompositableType aType, ImageContainer* aImageContainer) {
  MOZ_ASSERT(InImageBridgeChildThread());
  if (!CanSend()) {
    return nullptr;
  }

  RefPtr<ImageClient> client =
      ImageClient::CreateImageClient(aType, this, TextureFlags::NO_FLAGS);
  MOZ_ASSERT(client, "failed to create ImageClient");
  if (client) {
    client->Connect(aImageContainer);
  }
  return client;
}

bool ImageBridgeChild::AllocUnsafeShmem(
    size_t aSize, ipc::SharedMemory::SharedMemoryType aType,
    ipc::Shmem* aShmem) {
  if (!InImageBridgeChildThread()) {
    return DispatchAllocShmemInternal(aSize, aType, aShmem,
                                      true);  // true: unsafe
  }

  if (!CanSend()) {
    return false;
  }
  return PImageBridgeChild::AllocUnsafeShmem(aSize, aType, aShmem);
}

bool ImageBridgeChild::AllocShmem(size_t aSize,
                                  ipc::SharedMemory::SharedMemoryType aType,
                                  ipc::Shmem* aShmem) {
  if (!InImageBridgeChildThread()) {
    return DispatchAllocShmemInternal(aSize, aType, aShmem,
                                      false);  // false: unsafe
  }

  if (!CanSend()) {
    return false;
  }
  return PImageBridgeChild::AllocShmem(aSize, aType, aShmem);
}

void ImageBridgeChild::ProxyAllocShmemNow(SynchronousTask* aTask, size_t aSize,
                                          SharedMemory::SharedMemoryType aType,
                                          ipc::Shmem* aShmem, bool aUnsafe,
                                          bool* aSuccess) {
  AutoCompleteTask complete(aTask);

  if (!CanSend()) {
    return;
  }

  bool ok = false;
  if (aUnsafe) {
    ok = AllocUnsafeShmem(aSize, aType, aShmem);
  } else {
    ok = AllocShmem(aSize, aType, aShmem);
  }
  *aSuccess = ok;
}

bool ImageBridgeChild::DispatchAllocShmemInternal(
    size_t aSize, SharedMemory::SharedMemoryType aType, ipc::Shmem* aShmem,
    bool aUnsafe) {
  SynchronousTask task("AllocatorProxy alloc");

  bool success = false;
  RefPtr<Runnable> runnable = WrapRunnable(
      RefPtr<ImageBridgeChild>(this), &ImageBridgeChild::ProxyAllocShmemNow,
      &task, aSize, aType, aShmem, aUnsafe, &success);
  GetThread()->Dispatch(runnable.forget());

  task.Wait();

  return success;
}

void ImageBridgeChild::ProxyDeallocShmemNow(SynchronousTask* aTask,
                                            ipc::Shmem* aShmem, bool* aResult) {
  AutoCompleteTask complete(aTask);

  if (!CanSend()) {
    return;
  }
  *aResult = DeallocShmem(*aShmem);
}

bool ImageBridgeChild::DeallocShmem(ipc::Shmem& aShmem) {
  if (InImageBridgeChildThread()) {
    if (!CanSend()) {
      return false;
    }
    return PImageBridgeChild::DeallocShmem(aShmem);
  }

  // If we can't post a task, then we definitely cannot send, so there's
  // no reason to queue up this send.
  if (!CanPostTask()) {
    return false;
  }

  SynchronousTask task("AllocatorProxy Dealloc");
  bool result = false;

  RefPtr<Runnable> runnable = WrapRunnable(
      RefPtr<ImageBridgeChild>(this), &ImageBridgeChild::ProxyDeallocShmemNow,
      &task, &aShmem, &result);
  GetThread()->Dispatch(runnable.forget());

  task.Wait();
  return result;
}

PTextureChild* ImageBridgeChild::AllocPTextureChild(
    const SurfaceDescriptor&, ReadLockDescriptor&, const LayersBackend&,
    const TextureFlags&, const uint64_t& aSerial,
    const wr::MaybeExternalImageId& aExternalImageId) {
  MOZ_ASSERT(CanSend());
  return TextureClient::CreateIPDLActor();
}

bool ImageBridgeChild::DeallocPTextureChild(PTextureChild* actor) {
  return TextureClient::DestroyIPDLActor(actor);
}

PMediaSystemResourceManagerChild*
ImageBridgeChild::AllocPMediaSystemResourceManagerChild() {
  MOZ_ASSERT(CanSend());
  return new mozilla::media::MediaSystemResourceManagerChild();
}

bool ImageBridgeChild::DeallocPMediaSystemResourceManagerChild(
    PMediaSystemResourceManagerChild* aActor) {
  MOZ_ASSERT(aActor);
  delete static_cast<mozilla::media::MediaSystemResourceManagerChild*>(aActor);
  return true;
}

mozilla::ipc::IPCResult ImageBridgeChild::RecvParentAsyncMessages(
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
#ifdef MOZ_WIDGET_ANDROID
        const OpDeliverReleaseFence& op = message.get_OpDeliverReleaseFence();
        ipc::FileDescriptor fenceFd;
        if (op.fenceFd().isSome()) {
          fenceFd = *op.fenceFd();
        }
        AndroidHardwareBufferManager::Get()->NotifyNotUsed(
            std::move(fenceFd), op.bufferId(), op.fwdTransactionId(),
            op.usesImageBridge());
#else
        MOZ_ASSERT_UNREACHABLE("unexpected to be called");
#endif
        break;
      }
      default:
        NS_ERROR("unknown AsyncParentMessageData type");
        return IPC_FAIL_NO_REASON(this);
    }
  }
  return IPC_OK();
}

RefPtr<ImageContainerListener> ImageBridgeChild::FindListener(
    const CompositableHandle& aHandle) {
  RefPtr<ImageContainerListener> listener;
  MutexAutoLock lock(mContainerMapLock);
  auto it = mImageContainerListeners.find(aHandle.Value());
  if (it != mImageContainerListeners.end()) {
    listener = it->second;
  }
  return listener;
}

mozilla::ipc::IPCResult ImageBridgeChild::RecvDidComposite(
    nsTArray<ImageCompositeNotification>&& aNotifications) {
  for (auto& n : aNotifications) {
    RefPtr<ImageContainerListener> listener = FindListener(n.compositable());
    if (listener) {
      listener->NotifyComposite(n);
    }
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult ImageBridgeChild::RecvReportFramesDropped(
    const CompositableHandle& aHandle, const uint32_t& aFrames) {
  RefPtr<ImageContainerListener> listener = FindListener(aHandle);
  if (listener) {
    listener->NotifyDropped(aFrames);
  }

  return IPC_OK();
}

PTextureChild* ImageBridgeChild::CreateTexture(
    const SurfaceDescriptor& aSharedData, ReadLockDescriptor&& aReadLock,
    LayersBackend aLayersBackend, TextureFlags aFlags, uint64_t aSerial,
    wr::MaybeExternalImageId& aExternalImageId) {
  MOZ_ASSERT(CanSend());
  return SendPTextureConstructor(aSharedData, std::move(aReadLock),
                                 aLayersBackend, aFlags, aSerial,
                                 aExternalImageId);
}

static bool IBCAddOpDestroy(CompositableTransaction* aTxn,
                            const OpDestroy& op) {
  if (aTxn->Finished()) {
    return false;
  }

  aTxn->mDestroyedActors.AppendElement(op);
  return true;
}

bool ImageBridgeChild::DestroyInTransaction(PTextureChild* aTexture) {
  return IBCAddOpDestroy(mTxn, OpDestroy(aTexture));
}

bool ImageBridgeChild::DestroyInTransaction(const CompositableHandle& aHandle) {
  return IBCAddOpDestroy(mTxn, OpDestroy(aHandle));
}

void ImageBridgeChild::RemoveTextureFromCompositable(
    CompositableClient* aCompositable, TextureClient* aTexture) {
  MOZ_ASSERT(CanSend());
  MOZ_ASSERT(aTexture);
  MOZ_ASSERT(aTexture->IsSharedWithCompositor());
  MOZ_ASSERT(aCompositable->IsConnected());
  if (!aTexture || !aTexture->IsSharedWithCompositor() ||
      !aCompositable->IsConnected()) {
    return;
  }

  mTxn->AddNoSwapEdit(CompositableOperation(
      aCompositable->GetIPCHandle(),
      OpRemoveTexture(nullptr, aTexture->GetIPDLActor())));
}

bool ImageBridgeChild::IsSameProcess() const {
  return OtherPid() == base::GetCurrentProcId();
}

bool ImageBridgeChild::CanPostTask() const {
  // During shutdown, the cycle collector may free objects that are holding a
  // reference to ImageBridgeChild. Since this happens on the main thread,
  // ImageBridgeChild will attempt to post a task to the ImageBridge thread.
  // However the thread manager has already been shut down, so the task cannot
  // post.
  //
  // It's okay if this races. We only care about the shutdown case where
  // everything's happening on the main thread. Even if it races outside of
  // shutdown, it's still harmless to post the task, since the task must
  // check CanSend().
  return !mDestroyed;
}

void ImageBridgeChild::ReleaseCompositable(const CompositableHandle& aHandle) {
  if (!InImageBridgeChildThread()) {
    // If we can't post a task, then we definitely cannot send, so there's
    // no reason to queue up this send.
    if (!CanPostTask()) {
      return;
    }

    RefPtr<Runnable> runnable =
        WrapRunnable(RefPtr<ImageBridgeChild>(this),
                     &ImageBridgeChild::ReleaseCompositable, aHandle);
    GetThread()->Dispatch(runnable.forget());
    return;
  }

  if (!CanSend()) {
    return;
  }

  if (!DestroyInTransaction(aHandle)) {
    SendReleaseCompositable(aHandle);
  }

  {
    MutexAutoLock lock(mContainerMapLock);
    mImageContainerListeners.erase(aHandle.Value());
  }
}

bool ImageBridgeChild::CanSend() const {
  MOZ_ASSERT(InImageBridgeChildThread());
  return mCanSend;
}

void ImageBridgeChild::HandleFatalError(const char* aMsg) const {
  dom::ContentChild::FatalErrorIfNotUsingGPUProcess(aMsg, OtherPid());
}

wr::MaybeExternalImageId ImageBridgeChild::GetNextExternalImageId() {
  static uint32_t sNextID = 1;
  ++sNextID;
  MOZ_RELEASE_ASSERT(sNextID != UINT32_MAX);

  uint64_t imageId = mNamespace;
  imageId = imageId << 32 | sNextID;
  return Some(wr::ToExternalImageId(imageId));
}

}  // namespace layers
}  // namespace mozilla
