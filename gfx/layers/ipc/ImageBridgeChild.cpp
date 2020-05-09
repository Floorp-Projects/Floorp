/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImageBridgeChild.h"
#include <vector>                        // for vector
#include "ImageBridgeParent.h"           // for ImageBridgeParent
#include "ImageContainer.h"              // for ImageContainer
#include "Layers.h"                      // for Layer, etc
#include "ShadowLayers.h"                // for ShadowLayerForwarder
#include "base/platform_thread.h"        // for PlatformThread
#include "base/process.h"                // for ProcessId
#include "base/task.h"                   // for NewRunnableFunction, etc
#include "base/thread.h"                 // for Thread
#include "mozilla/Assertions.h"          // for MOZ_ASSERT, etc
#include "mozilla/Monitor.h"             // for Monitor, MonitorAutoLock
#include "mozilla/ReentrantMonitor.h"    // for ReentrantMonitor, etc
#include "mozilla/ipc/MessageChannel.h"  // for MessageChannel, etc
#include "mozilla/ipc/Transport.h"       // for Transport
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/gfx/Point.h"  // for IntSize
#include "mozilla/layers/AsyncCanvasRenderer.h"
#include "mozilla/media/MediaSystemResourceManager.h"  // for MediaSystemResourceManager
#include "mozilla/media/MediaSystemResourceManagerChild.h"  // for MediaSystemResourceManagerChild
#include "mozilla/layers/CompositableClient.h"  // for CompositableChild, etc
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/layers/ISurfaceAllocator.h"  // for ISurfaceAllocator
#include "mozilla/layers/ImageClient.h"        // for ImageClient
#include "mozilla/layers/LayersMessages.h"     // for CompositableOperation
#include "mozilla/layers/TextureClient.h"      // for TextureClient
#include "mozilla/dom/ContentChild.h"
#include "mozilla/mozalloc.h"  // for operator new, etc
#include "mtransport/runnable_utils.h"
#include "nsContentUtils.h"
#include "nsISupportsImpl.h"         // for ImageContainer::AddRef, etc
#include "nsTArray.h"                // for AutoTArray, nsTArray, etc
#include "nsTArrayForwardDeclare.h"  // for AutoTArray
#include "nsThread.h"
#include "nsThreadUtils.h"  // for NS_IsMainThread
#include "mozilla/StaticMutex.h"
#include "mozilla/StaticPtr.h"  // for StaticRefPtr
#include "mozilla/layers/TextureClient.h"
#include "SynchronousTask.h"

#if defined(XP_WIN)
#  include "mozilla/gfx/DeviceManagerDx.h"
#endif

namespace mozilla {
namespace ipc {
class Shmem;
}  // namespace ipc

namespace layers {

using base::ProcessId;
using base::Thread;
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
    const nsTArray<TimedTextureClient>& aTextures,
    const Maybe<wr::RenderRoot>& aRenderRoot) {
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
    textures.AppendElement(
        TimedTexture(nullptr, t.mTextureClient->GetIPDLActor(), t.mTimeStamp,
                     t.mPictureRect, t.mFrameID, t.mProducerID, readLocked));

    // Wait end of usage on host side if TextureFlags::RECYCLE is set
    HoldUntilCompositableRefReleasedIfNecessary(t.mTextureClient);
  }
  mTxn->AddNoSwapEdit(CompositableOperation(aCompositable->GetIPCHandle(),
                                            OpUseTexture(textures)));
}

void ImageBridgeChild::UseComponentAlphaTextures(
    CompositableClient* aCompositable, TextureClient* aTextureOnBlack,
    TextureClient* aTextureOnWhite) {
  MOZ_CRASH("should not be called");
}

void ImageBridgeChild::HoldUntilCompositableRefReleasedIfNecessary(
    TextureClient* aClient) {
  if (!aClient) {
    return;
  }
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
static StaticMutex sImageBridgeSingletonLock;
static StaticRefPtr<ImageBridgeChild> sImageBridgeChildSingleton;
// sImageBridgeChildThread cannot be converted to use a generic
// nsISerialEventTarget (which may be backed by a threadpool) until bug 1634846
// is addressed. Therefore we keep it as an nsIThread here.
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

// dispatched function
void ImageBridgeChild::CreateCanvasClientSync(
    SynchronousTask* aTask, CanvasClient::CanvasClientType aType,
    TextureFlags aFlags, RefPtr<CanvasClient>* const outResult) {
  AutoCompleteTask complete(aTask);
  *outResult = CreateCanvasClientNow(aType, aFlags);
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
  SendNewCompositable(handle, aCompositable->GetTextureInfo(),
                      GetCompositorBackendType());
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
  client->UpdateImage(aContainer, Layer::CONTENT_OPAQUE, Nothing());
  EndTransaction();
}

void ImageBridgeChild::UpdateAsyncCanvasRendererSync(
    SynchronousTask* aTask, AsyncCanvasRenderer* aWrapper) {
  AutoCompleteTask complete(aTask);

  UpdateAsyncCanvasRendererNow(aWrapper);
}

void ImageBridgeChild::UpdateAsyncCanvasRenderer(
    AsyncCanvasRenderer* aWrapper) {
  aWrapper->GetCanvasClient()->UpdateAsync(aWrapper);

  if (InImageBridgeChildThread()) {
    UpdateAsyncCanvasRendererNow(aWrapper);
    return;
  }

  SynchronousTask task("UpdateAsyncCanvasRenderer Lock");

  RefPtr<Runnable> runnable = WrapRunnable(
      RefPtr<ImageBridgeChild>(this),
      &ImageBridgeChild::UpdateAsyncCanvasRendererSync, &task, aWrapper);
  GetThread()->Dispatch(runnable.forget());

  task.Wait();
}

void ImageBridgeChild::UpdateAsyncCanvasRendererNow(
    AsyncCanvasRenderer* aWrapper) {
  MOZ_ASSERT(aWrapper);

  if (!CanSend()) {
    return;
  }

  BeginTransaction();
  // TODO wr::RenderRoot::Unknown
  aWrapper->GetCanvasClient()->Updated(wr::RenderRoot::Default);
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

  if (!IsSameProcess()) {
    ShadowLayerForwarder::PlatformSyncBeforeUpdate();
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

  RefPtr<Runnable> runnable = NewRunnableMethod<Endpoint<PImageBridgeChild>&&>(
      "layers::ImageBridgeChild::Bind", child, &ImageBridgeChild::Bind,
      std::move(aEndpoint));
  child->GetThread()->Dispatch(runnable.forget());

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
  ipc::MessageChannel* parentChannel = aParent->GetIPCChannel();
  Open(parentChannel, aParent->GetThread(), mozilla::ipc::ChildSide);

  // This reference is dropped in DeallocPImageBridgeChild.
  this->AddRef();

  mCanSend = true;
}

/* static */
void ImageBridgeChild::ShutDown() {
  MOZ_ASSERT(NS_IsMainThread());

  ShutdownSingleton();

  sImageBridgeChildThread = nullptr;
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

  child->GetThread()->Dispatch(NewRunnableMethod<Endpoint<PImageBridgeChild>&&>(
      "layers::ImageBridgeChild::Bind", child, &ImageBridgeChild::Bind,
      std::move(aEndpoint)));

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
  // ImageHost is incompatible between WebRender enabled and WebRender disabled.
  // Then drop all ImageContainers' ImageClients during disabling WebRender.
  bool disablingWebRender =
      GetCompositorBackendType() == LayersBackend::LAYERS_WR &&
      aIdentifier.mParentBackend != LayersBackend::LAYERS_WR;

  // Do not update TextureFactoryIdentifier if aIdentifier is going to disable
  // WebRender, but gecko is still using WebRender. Since gecko uses different
  // incompatible ImageHost and TextureHost between WebRender and non-WebRender.
  //
  // Even when WebRender is still in use, if non-accelerated widget is opened,
  // aIdentifier disables WebRender at ImageBridgeChild.
  if (disablingWebRender && gfxVars::UseWebRender()) {
    return;
  }

  // D3DTexture might become obsolte. To prevent to use obsoleted D3DTexture,
  // drop all ImageContainers' ImageClients.

  // During re-creating GPU process, there was a period that ImageBridgeChild
  // was re-created, but ImageBridgeChild::UpdateTextureFactoryIdentifier() was
  // not called yet. In the period, if ImageBridgeChild::CreateImageClient() is
  // called, ImageBridgeParent creates incompatible ImageHost than
  // WebRenderImageHost.
  bool initializingWebRender =
      GetCompositorBackendType() != LayersBackend::LAYERS_WR &&
      aIdentifier.mParentBackend == LayersBackend::LAYERS_WR;

  bool needsDrop = disablingWebRender || initializingWebRender;

#if defined(XP_WIN)
  RefPtr<ID3D11Device> device = gfx::DeviceManagerDx::Get()->GetImageDevice();
  needsDrop |= !!mImageDevice && mImageDevice != device &&
               GetCompositorBackendType() == LayersBackend::LAYERS_D3D11;
  mImageDevice = device;
#endif

  IdentifyTextureHost(aIdentifier);
  if (needsDrop) {
    nsTArray<RefPtr<ImageContainerListener> > listeners;
    {
      MutexAutoLock lock(mContainerMapLock);
      for (const auto& entry : mImageContainerListeners) {
        listeners.AppendElement(entry.second);
      }
    }
    // Drop ImageContainer's ImageClient whithout holding mContainerMapLock to
    // avoid deadlock.
    for (auto container : listeners) {
      container->DropImageClient();
    }
  }
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

already_AddRefed<CanvasClient> ImageBridgeChild::CreateCanvasClient(
    CanvasClient::CanvasClientType aType, TextureFlags aFlag) {
  if (InImageBridgeChildThread()) {
    return CreateCanvasClientNow(aType, aFlag);
  }

  SynchronousTask task("CreateCanvasClient Lock");

  // RefPtrs on arguments are not needed since this dispatches synchronously.
  RefPtr<CanvasClient> result = nullptr;
  RefPtr<Runnable> runnable = WrapRunnable(
      RefPtr<ImageBridgeChild>(this), &ImageBridgeChild::CreateCanvasClientSync,
      &task, aType, aFlag, &result);
  GetThread()->Dispatch(runnable.forget());

  task.Wait();

  return result.forget();
}

already_AddRefed<CanvasClient> ImageBridgeChild::CreateCanvasClientNow(
    CanvasClient::CanvasClientType aType, TextureFlags aFlag) {
  RefPtr<CanvasClient> client =
      CanvasClient::CreateCanvasClient(aType, this, aFlag);
  MOZ_ASSERT(client, "failed to create CanvasClient");
  if (client) {
    client->Connect();
  }
  return client.forget();
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
    const SurfaceDescriptor&, const ReadLockDescriptor&, const LayersBackend&,
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
    const SurfaceDescriptor& aSharedData, const ReadLockDescriptor& aReadLock,
    LayersBackend aLayersBackend, TextureFlags aFlags, uint64_t aSerial,
    wr::MaybeExternalImageId& aExternalImageId, nsIEventTarget* aTarget) {
  MOZ_ASSERT(CanSend());
  return SendPTextureConstructor(aSharedData, aReadLock, aLayersBackend, aFlags,
                                 aSerial, aExternalImageId);
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
    CompositableClient* aCompositable, TextureClient* aTexture,
    const Maybe<wr::RenderRoot>& aRenderRoot) {
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
