/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImageBridgeChild.h"
#include <vector>                       // for vector
#include "ImageBridgeParent.h"          // for ImageBridgeParent
#include "ImageContainer.h"             // for ImageContainer
#include "Layers.h"                     // for Layer, etc
#include "ShadowLayers.h"               // for ShadowLayerForwarder
#include "base/message_loop.h"          // for MessageLoop
#include "base/platform_thread.h"       // for PlatformThread
#include "base/process.h"               // for ProcessId
#include "base/task.h"                  // for NewRunnableFunction, etc
#include "base/thread.h"                // for Thread
#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
#include "mozilla/Monitor.h"            // for Monitor, MonitorAutoLock
#include "mozilla/ReentrantMonitor.h"   // for ReentrantMonitor, etc
#include "mozilla/ipc/MessageChannel.h" // for MessageChannel, etc
#include "mozilla/ipc/Transport.h"      // for Transport
#include "mozilla/gfx/Point.h"          // for IntSize
#include "mozilla/layers/AsyncCanvasRenderer.h"
#include "mozilla/media/MediaSystemResourceManager.h" // for MediaSystemResourceManager
#include "mozilla/media/MediaSystemResourceManagerChild.h" // for MediaSystemResourceManagerChild
#include "mozilla/layers/CompositableChild.h"
#include "mozilla/layers/CompositableClient.h"  // for CompositableChild, etc
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/layers/ISurfaceAllocator.h"  // for ISurfaceAllocator
#include "mozilla/layers/ImageClient.h"  // for ImageClient
#include "mozilla/layers/ImageContainerChild.h"
#include "mozilla/layers/LayersMessages.h"  // for CompositableOperation
#include "mozilla/layers/PCompositableChild.h"  // for PCompositableChild
#include "mozilla/layers/TextureClient.h"  // for TextureClient
#include "mozilla/mozalloc.h"           // for operator new, etc
#include "mtransport/runnable_utils.h"
#include "nsContentUtils.h"
#include "nsISupportsImpl.h"            // for ImageContainer::AddRef, etc
#include "nsTArray.h"                   // for AutoTArray, nsTArray, etc
#include "nsTArrayForwardDeclare.h"     // for AutoTArray
#include "nsThreadUtils.h"              // for NS_IsMainThread
#include "nsXULAppAPI.h"                // for XRE_GetIOMessageLoop
#include "mozilla/StaticMutex.h"
#include "mozilla/StaticPtr.h"          // for StaticRefPtr
#include "mozilla/layers/TextureClient.h"

namespace mozilla {
namespace ipc {
class Shmem;
} // namespace ipc

namespace layers {

using base::Thread;
using base::ProcessId;
using namespace mozilla::ipc;
using namespace mozilla::gfx;
using namespace mozilla::media;

typedef std::vector<CompositableOperation> OpVector;
typedef nsTArray<OpDestroy> OpDestroyVector;

namespace {
class ImageBridgeThread : public Thread {
public:

  ImageBridgeThread() : Thread("ImageBridgeChild") {
  }

protected:

  MOZ_IS_CLASS_INIT
  void Init() {
#ifdef MOZ_ENABLE_PROFILER_SPS
    mPseudoStackHack = mozilla_get_pseudo_stack();
#endif
  }

  void CleanUp() {
#ifdef MOZ_ENABLE_PROFILER_SPS
    mPseudoStackHack = nullptr;
#endif
  }

private:

#ifdef MOZ_ENABLE_PROFILER_SPS
  // This is needed to avoid a spurious leak report.  There's no other
  // use for it.  See bug 1239504 and bug 1215265.
  MOZ_INIT_OUTSIDE_CTOR PseudoStack* mPseudoStackHack;
#endif
};
}

struct CompositableTransaction
{
  CompositableTransaction()
  : mSwapRequired(false)
  , mFinished(true)
  {}
  ~CompositableTransaction()
  {
    End();
  }
  bool Finished() const
  {
    return mFinished;
  }
  void Begin()
  {
    MOZ_ASSERT(mFinished);
    mFinished = false;
  }
  void End()
  {
    mFinished = true;
    mSwapRequired = false;
    mOperations.clear();
    mDestroyedActors.Clear();
  }
  bool IsEmpty() const
  {
    return mOperations.empty() && mDestroyedActors.IsEmpty();
  }
  void AddNoSwapEdit(const CompositableOperation& op)
  {
    MOZ_ASSERT(!Finished(), "forgot BeginTransaction?");
    mOperations.push_back(op);
  }
  void AddEdit(const CompositableOperation& op)
  {
    AddNoSwapEdit(op);
    MarkSyncTransaction();
  }
  void MarkSyncTransaction()
  {
    mSwapRequired = true;
  }

  OpVector mOperations;
  OpDestroyVector mDestroyedActors;
  bool mSwapRequired;
  bool mFinished;
};

struct AutoEndTransaction {
  explicit AutoEndTransaction(CompositableTransaction* aTxn) : mTxn(aTxn) {}
  ~AutoEndTransaction() { mTxn->End(); }
  CompositableTransaction* mTxn;
};

void
ImageBridgeChild::UseTextures(CompositableClient* aCompositable,
                              const nsTArray<TimedTextureClient>& aTextures)
{
  MOZ_ASSERT(aCompositable);
  MOZ_ASSERT(aCompositable->GetIPDLActor());
  MOZ_ASSERT(aCompositable->IsConnected());

  AutoTArray<TimedTexture,4> textures;

  for (auto& t : aTextures) {
    MOZ_ASSERT(t.mTextureClient);
    MOZ_ASSERT(t.mTextureClient->GetIPDLActor());

    if (!t.mTextureClient->IsSharedWithCompositor()) {
      return;
    }

    ReadLockDescriptor readLock;
    t.mTextureClient->SerializeReadLock(readLock);

    FenceHandle fence = t.mTextureClient->GetAcquireFenceHandle();
    textures.AppendElement(TimedTexture(nullptr, t.mTextureClient->GetIPDLActor(),
                                        readLock,
                                        fence.IsValid() ? MaybeFence(fence) : MaybeFence(null_t()),
                                        t.mTimeStamp, t.mPictureRect,
                                        t.mFrameID, t.mProducerID));

    // Wait end of usage on host side if TextureFlags::RECYCLE is set or GrallocTextureData case
    HoldUntilCompositableRefReleasedIfNecessary(t.mTextureClient);
  }
  mTxn->AddNoSwapEdit(CompositableOperation(nullptr, aCompositable->GetIPDLActor(),
                                            OpUseTexture(textures)));
}

void
ImageBridgeChild::UseComponentAlphaTextures(CompositableClient* aCompositable,
                                            TextureClient* aTextureOnBlack,
                                            TextureClient* aTextureOnWhite)
{
  MOZ_ASSERT(aCompositable);
  MOZ_ASSERT(aTextureOnWhite);
  MOZ_ASSERT(aTextureOnBlack);
  MOZ_ASSERT(aCompositable->IsConnected());
  MOZ_ASSERT(aTextureOnWhite->GetIPDLActor());
  MOZ_ASSERT(aTextureOnBlack->GetIPDLActor());
  MOZ_ASSERT(aTextureOnBlack->GetSize() == aTextureOnWhite->GetSize());

  ReadLockDescriptor readLockW;
  ReadLockDescriptor readLockB;
  aTextureOnBlack->SerializeReadLock(readLockB);
  aTextureOnWhite->SerializeReadLock(readLockW);

  HoldUntilCompositableRefReleasedIfNecessary(aTextureOnBlack);
  HoldUntilCompositableRefReleasedIfNecessary(aTextureOnWhite);

  mTxn->AddNoSwapEdit(
    CompositableOperation(
      nullptr,
      aCompositable->GetIPDLActor(),
      OpUseComponentAlphaTextures(
        nullptr, aTextureOnBlack->GetIPDLActor(),
        nullptr, aTextureOnWhite->GetIPDLActor(),
        readLockB, readLockW
      )
    )
  );
}

#ifdef MOZ_WIDGET_GONK
void
ImageBridgeChild::UseOverlaySource(CompositableClient* aCompositable,
                                   const OverlaySource& aOverlay,
                                   const nsIntRect& aPictureRect)
{
  MOZ_ASSERT(aCompositable);
  MOZ_ASSERT(aCompositable->IsConnected());

  CompositableOperation op(
    nullptr,
    aCompositable->GetIPDLActor(),
    OpUseOverlaySource(aOverlay, aPictureRect));

  mTxn->AddEdit(op);
}
#endif

void
ImageBridgeChild::HoldUntilCompositableRefReleasedIfNecessary(TextureClient* aClient)
{
  // Wait ReleaseCompositableRef only when TextureFlags::RECYCLE is set on ImageBridge.
  if (!aClient ||
      !(aClient->GetFlags() & TextureFlags::RECYCLE)) {
    return;
  }
  aClient->SetLastFwdTransactionId(GetFwdTransactionId());
  mTexturesWaitingRecycled.Put(aClient->GetSerial(), aClient);
}

void
ImageBridgeChild::NotifyNotUsed(uint64_t aTextureId, uint64_t aFwdTransactionId)
{
  RefPtr<TextureClient> client = mTexturesWaitingRecycled.Get(aTextureId);
  if (!client) {
    return;
  }
  if (aFwdTransactionId < client->GetLastFwdTransactionId()) {
    // Released on host side, but client already requested newer use texture.
    return;
  }
  mTexturesWaitingRecycled.Remove(aTextureId);
}

void
ImageBridgeChild::DeliverFence(uint64_t aTextureId, FenceHandle& aReleaseFenceHandle)
{
  RefPtr<TextureClient> client = mTexturesWaitingRecycled.Get(aTextureId);
  if (!client) {
    return;
  }
  client->SetReleaseFenceHandle(aReleaseFenceHandle);
}

void
ImageBridgeChild::HoldUntilFenceHandleDelivery(TextureClient* aClient, uint64_t aTransactionId)
{
  MOZ_ASSERT(NS_IsMainThread());
  // XXX Re-enable fence handling
  return;

#ifdef MOZ_WIDGET_GONK
  if (!aClient) {
    return;
  }
  MutexAutoLock lock(mWaitingFenceHandleMutex);
  aClient->SetLastFwdTransactionId(aTransactionId);
  aClient->WaitFenceHandleOnImageBridge(mWaitingFenceHandleMutex);
  mTexturesWaitingFenceHandle.Put(aClient->GetSerial(), aClient);
#else
  NS_RUNTIMEABORT("not reached");
#endif
}

void
ImageBridgeChild::DeliverFenceToNonRecycle(uint64_t aTextureId, FenceHandle& aReleaseFenceHandle)
{
  // XXX Re-enable fence handling
  return;

#ifdef MOZ_WIDGET_GONK
  MutexAutoLock lock(mWaitingFenceHandleMutex);
  TextureClient* client = mTexturesWaitingFenceHandle.Get(aTextureId).get();
  if (!client) {
    return;
  }
  MOZ_ASSERT(aTextureId == client->GetSerial());
  client->SetReleaseFenceHandle(aReleaseFenceHandle);
#else
  NS_RUNTIMEABORT("not reached");
#endif
}

void
ImageBridgeChild::NotifyNotUsedToNonRecycle(uint64_t aTextureId, uint64_t aTransactionId)
{
  // XXX Re-enable fence handling
  return;

#ifdef MOZ_WIDGET_GONK
  MutexAutoLock lock(mWaitingFenceHandleMutex);

  RefPtr<TextureClient> client = mTexturesWaitingFenceHandle.Get(aTextureId);
  if (!client) {
    return;
  }
  if (aTransactionId < client->GetLastFwdTransactionId()) {
    return;
  }

  MOZ_ASSERT(aTextureId == client->GetSerial());
  client->ClearWaitFenceHandleOnImageBridge(mWaitingFenceHandleMutex);
  mTexturesWaitingFenceHandle.Remove(aTextureId);

  // Release TextureClient on allocator's message loop.
  RefPtr<TextureClientReleaseTask> task =
    MakeAndAddRef<TextureClientReleaseTask>(client);
  RefPtr<ClientIPCAllocator> allocator = client->GetAllocator();
  client = nullptr;
  allocator->AsClientAllocator()->GetMessageLoop()->PostTask(task.forget());
#else
  NS_RUNTIMEABORT("not reached");
#endif
}

void
ImageBridgeChild::CancelWaitFenceHandle(TextureClient* aClient)
{
  // XXX Re-enable fence handling
  return;

#ifdef MOZ_WIDGET_GONK
  MutexAutoLock lock(mWaitingFenceHandleMutex);
  aClient->ClearWaitFenceHandleOnImageBridge(mWaitingFenceHandleMutex);
  mTexturesWaitingFenceHandle.Remove(aClient->GetSerial());
#else
  NS_RUNTIMEABORT("not reached");
#endif
}

void
ImageBridgeChild::CancelWaitForRecycle(uint64_t aTextureId)
{
  MOZ_ASSERT(InImageBridgeChildThread());

  RefPtr<TextureClient> client = mTexturesWaitingRecycled.Get(aTextureId);
  if (!client) {
    return;
  }
  mTexturesWaitingRecycled.Remove(aTextureId);
}

// Singleton
static StaticMutex sImageBridgeSingletonLock;
static StaticRefPtr<ImageBridgeChild> sImageBridgeChildSingleton;
static Thread *sImageBridgeChildThread = nullptr;

// Helper that creates a monitor and a "done" flag, then enters the monitor.
// This can go away when we switch ImageBridge to an XPCOM thread.
class MOZ_STACK_CLASS SynchronousTask
{
  friend class AutoCompleteTask;

public:
  explicit SynchronousTask(const char* name)
   : mMonitor(name),
     mAutoEnter(mMonitor),
     mDone(false)
  {}

  void Wait() {
    while (!mDone) {
      mMonitor.Wait();
    }
  }

private:
  void Complete() {
    mDone = true;
    mMonitor.NotifyAll();
  }

private:
  ReentrantMonitor mMonitor;
  ReentrantMonitorAutoEnter mAutoEnter;
  bool mDone;
};

class MOZ_STACK_CLASS AutoCompleteTask
{
public:
  explicit AutoCompleteTask(SynchronousTask* aTask)
   : mTask(aTask),
     mAutoEnter(aTask->mMonitor)
  {
  }
  ~AutoCompleteTask() {
    mTask->Complete();
  }

private:
  SynchronousTask* mTask;
  ReentrantMonitorAutoEnter mAutoEnter;
};

// dispatched function
void
ImageBridgeChild::ShutdownStep1(SynchronousTask* aTask)
{
  AutoCompleteTask complete(aTask);

  MOZ_ASSERT(InImageBridgeChildThread(),
             "Should be in ImageBridgeChild thread.");

  MediaSystemResourceManager::Shutdown();

  // Force all managed protocols to shut themselves down cleanly
  InfallibleTArray<PCompositableChild*> compositables;
  ManagedPCompositableChild(compositables);
  for (int i = compositables.Length() - 1; i >= 0; --i) {
    auto compositable = CompositableClient::FromIPDLActor(compositables[i]);
    if (compositable) {
      compositable->Destroy();
    }
  }
  InfallibleTArray<PTextureChild*> textures;
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
void
ImageBridgeChild::ShutdownStep2(SynchronousTask* aTask)
{
  AutoCompleteTask complete(aTask);

  MOZ_ASSERT(InImageBridgeChildThread(),
             "Should be in ImageBridgeChild thread.");

  if (!mCalledClose) {
    Close();
    mCalledClose = true;
  }
}

void
ImageBridgeChild::ActorDestroy(ActorDestroyReason aWhy)
{
  mCanSend = false;
  mCalledClose = true;
}

void
ImageBridgeChild::DeallocPImageBridgeChild()
{
  this->Release();
}

void
ImageBridgeChild::CreateImageClientSync(SynchronousTask* aTask,
                                        RefPtr<ImageClient>* result,
                                        CompositableType aType,
                                        ImageContainer* aImageContainer,
                                        ImageContainerChild* aContainerChild)
{
  AutoCompleteTask complete(aTask);
  *result = CreateImageClientNow(aType, aImageContainer, aContainerChild);
}

// dispatched function
void
ImageBridgeChild::CreateCanvasClientSync(SynchronousTask* aTask,
                                         CanvasClient::CanvasClientType aType,
                                         TextureFlags aFlags,
                                         RefPtr<CanvasClient>* const outResult)
{
  AutoCompleteTask complete(aTask);
  *outResult = CreateCanvasClientNow(aType, aFlags);
}

ImageBridgeChild::ImageBridgeChild()
  : mCanSend(false)
  , mCalledClose(false)
  , mFwdTransactionId(0)
#ifdef MOZ_WIDGET_GONK
  , mWaitingFenceHandleMutex("ImageBridgeChild::mWaitingFenceHandleMutex")
#endif
{
  MOZ_ASSERT(NS_IsMainThread());

  mTxn = new CompositableTransaction();
  mShutdownObserver = new ShutdownObserver(this);
}

ImageBridgeChild::~ImageBridgeChild()
{
  // We should have already removed this in WillShutdown, since it must be
  // removed on the main thread.
  MOZ_ASSERT(!mShutdownObserver);

  delete mTxn;
}

void
ImageBridgeChild::MarkShutDown()
{
  mTexturesWaitingRecycled.Clear();
  mTrackersHolder.DestroyAsyncTransactionTrackersHolder();

  mCanSend = false;
}

void
ImageBridgeChild::Connect(CompositableClient* aCompositable,
                          ImageContainer* aImageContainer)
{
  MOZ_ASSERT(aCompositable);
  MOZ_ASSERT(InImageBridgeChildThread());
  MOZ_ASSERT(CanSend());

  uint64_t id = 0;

  PImageContainerChild* imageContainerChild = nullptr;
  if (aImageContainer)
    imageContainerChild = aImageContainer->GetPImageContainerChild();

  PCompositableChild* child =
    SendPCompositableConstructor(aCompositable->GetTextureInfo(),
                                 imageContainerChild, &id);
  MOZ_ASSERT(child);
  aCompositable->InitIPDLActor(child, id);
}

PCompositableChild*
ImageBridgeChild::AllocPCompositableChild(const TextureInfo& aInfo,
                                          PImageContainerChild* aChild, uint64_t* aID)
{
  MOZ_ASSERT(CanSend());
  return AsyncCompositableChild::CreateActor();
}

bool
ImageBridgeChild::DeallocPCompositableChild(PCompositableChild* aActor)
{
  AsyncCompositableChild::DestroyActor(aActor);
  return true;
}


Thread* ImageBridgeChild::GetThread() const
{
  return sImageBridgeChildThread;
}

/* static */ RefPtr<ImageBridgeChild>
ImageBridgeChild::GetSingleton()
{
  StaticMutexAutoLock lock(sImageBridgeSingletonLock);
  return sImageBridgeChildSingleton;
}

void
ImageBridgeChild::ReleaseImageContainer(RefPtr<ImageContainerChild> aChild)
{
  if (!aChild) {
    return;
  }

  if (!InImageBridgeChildThread()) {
    RefPtr<Runnable> runnable = WrapRunnable(
      RefPtr<ImageBridgeChild>(this),
      &ImageBridgeChild::ReleaseImageContainer,
      aChild);
    GetMessageLoop()->PostTask(runnable.forget());
    return;
  }

  aChild->SendAsyncDelete();
}

void
ImageBridgeChild::ReleaseTextureClientNow(TextureClient* aClient)
{
  MOZ_ASSERT(InImageBridgeChildThread());
  RELEASE_MANUALLY(aClient);
}

/* static */ void
ImageBridgeChild::DispatchReleaseTextureClient(TextureClient* aClient)
{
  if (!aClient) {
    return;
  }

  RefPtr<ImageBridgeChild> imageBridge = ImageBridgeChild::GetSingleton();
  if (!imageBridge) {
    // TextureClient::Release should normally happen in the ImageBridgeChild
    // thread because it usually generate some IPDL messages.
    // However, if we take this branch it means that the ImageBridgeChild
    // has already shut down, along with the TextureChild, which means no
    // message will be sent and it is safe to run this code from any thread.
    MOZ_ASSERT(aClient->GetIPDLActor() == nullptr);
    RELEASE_MANUALLY(aClient);
    return;
  }

  RefPtr<Runnable> runnable = WrapRunnable(
    imageBridge,
    &ImageBridgeChild::ReleaseTextureClientNow,
    aClient);
  imageBridge->GetMessageLoop()->PostTask(runnable.forget());
}

void
ImageBridgeChild::UpdateImageClient(RefPtr<ImageClient> aClient, RefPtr<ImageContainer> aContainer)
{
  if (!aClient || !aContainer) {
    return;
  }

  if (!InImageBridgeChildThread()) {
    RefPtr<Runnable> runnable = WrapRunnable(
      RefPtr<ImageBridgeChild>(this),
      &ImageBridgeChild::UpdateImageClient,
      aClient,
      aContainer);
    GetMessageLoop()->PostTask(runnable.forget());
    return;
  }

  if (!CanSend()) {
    return;
  }

  // If the client has become disconnected before this event was dispatched,
  // early return now.
  if (!aClient->IsConnected()) {
    return;
  }

  BeginTransaction();
  aClient->UpdateImage(aContainer, Layer::CONTENT_OPAQUE);
  EndTransaction();
}

void
ImageBridgeChild::UpdateAsyncCanvasRendererSync(SynchronousTask* aTask, AsyncCanvasRenderer* aWrapper)
{
  AutoCompleteTask complete(aTask);

  UpdateAsyncCanvasRendererNow(aWrapper);
}

void
ImageBridgeChild::UpdateAsyncCanvasRenderer(AsyncCanvasRenderer* aWrapper)
{
  aWrapper->GetCanvasClient()->UpdateAsync(aWrapper);

  if (InImageBridgeChildThread()) {
    UpdateAsyncCanvasRendererNow(aWrapper);
    return;
  }

  SynchronousTask task("UpdateAsyncCanvasRenderer Lock");

  RefPtr<Runnable> runnable = WrapRunnable(
    RefPtr<ImageBridgeChild>(this),
    &ImageBridgeChild::UpdateAsyncCanvasRendererSync,
    &task,
    aWrapper);
  GetMessageLoop()->PostTask(runnable.forget());

  task.Wait();
}

void
ImageBridgeChild::UpdateAsyncCanvasRendererNow(AsyncCanvasRenderer* aWrapper)
{
  MOZ_ASSERT(aWrapper);

  if (!CanSend()) {
    return;
  }

  BeginTransaction();
  aWrapper->GetCanvasClient()->Updated();
  EndTransaction();
}

void
ImageBridgeChild::FlushAllImagesSync(SynchronousTask* aTask,
                                     ImageClient* aClient,
                                     ImageContainer* aContainer,
                                     RefPtr<AsyncTransactionWaiter> aWaiter)
{
#ifdef MOZ_WIDGET_GONK
  MOZ_ASSERT(aWaiter);
#else
  MOZ_ASSERT(!aWaiter);
#endif

  AutoCompleteTask complete(aTask);

  if (!CanSend()) {
#ifdef MOZ_WIDGET_GONK
    aWaiter->DecrementWaitCount();
#endif
    return;
  }

  MOZ_ASSERT(aClient);
  BeginTransaction();
  if (aContainer) {
    aContainer->ClearImagesFromImageBridge();
  }
  aClient->FlushAllImages(aWaiter);
  EndTransaction();
  // This decrement is balanced by the increment in FlushAllImages.
  // If any AsyncTransactionTrackers were created by FlushAllImages and attached
  // to aWaiter, aWaiter will not complete until those trackers all complete.
  // Otherwise, aWaiter will be ready to complete now.
#ifdef MOZ_WIDGET_GONK
  aWaiter->DecrementWaitCount();
#endif
}

void
ImageBridgeChild::FlushAllImages(ImageClient* aClient, ImageContainer* aContainer)
{
  MOZ_ASSERT(aClient);
  MOZ_ASSERT(!InImageBridgeChildThread());

  if (InImageBridgeChildThread()) {
    NS_ERROR("ImageBridgeChild::FlushAllImages() is called on ImageBridge thread.");
    return;
  }

  SynchronousTask task("FlushAllImages Lock");

  RefPtr<AsyncTransactionWaiter> waiter;
#ifdef MOZ_WIDGET_GONK
  waiter = new AsyncTransactionWaiter();
  // This increment is balanced by the decrement in FlushAllImagesSync
  waiter->IncrementWaitCount();
#endif

  // RefPtrs on arguments are not needed since this dispatches synchronously.
  RefPtr<Runnable> runnable = WrapRunnable(
    RefPtr<ImageBridgeChild>(this),
    &ImageBridgeChild::FlushAllImagesSync,
    &task,
    aClient,
    aContainer,
    waiter);
  GetMessageLoop()->PostTask(runnable.forget());

  task.Wait();

#ifdef MOZ_WIDGET_GONK
  waiter->WaitComplete();
#endif
}

void
ImageBridgeChild::BeginTransaction()
{
  MOZ_ASSERT(CanSend());
  MOZ_ASSERT(mTxn->Finished(), "uncommitted txn?");
  UpdateFwdTransactionId();
  mTxn->Begin();
}

void
ImageBridgeChild::EndTransaction()
{
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

  AutoTArray<EditReply, 10> replies;

  if (mTxn->mSwapRequired) {
    if (!SendUpdate(cset, mTxn->mDestroyedActors, GetFwdTransactionId(), &replies)) {
      NS_WARNING("could not send async texture transaction");
      return;
    }
  } else {
    // If we don't require a swap we can call SendUpdateNoSwap which
    // assumes that aReplies is empty (DEBUG assertion)
    if (!SendUpdateNoSwap(cset, mTxn->mDestroyedActors, GetFwdTransactionId())) {
      NS_WARNING("could not send async texture transaction (no swap)");
      return;
    }
  }
  for (nsTArray<EditReply>::size_type i = 0; i < replies.Length(); ++i) {
    NS_RUNTIMEABORT("not reached");
  }
}

void
ImageBridgeChild::SendImageBridgeThreadId()
{
#ifdef MOZ_WIDGET_GONK
  PImageBridgeChild::SendImageBridgeThreadId(gettid());
#endif
}

bool
ImageBridgeChild::InitForContent(Endpoint<PImageBridgeChild>&& aEndpoint)
{
  MOZ_ASSERT(NS_IsMainThread());

  gfxPlatform::GetPlatform();

  if (!sImageBridgeChildThread) {
    sImageBridgeChildThread = new ImageBridgeThread();
    if (!sImageBridgeChildThread->Start()) {
      return false;
    }
  }

  RefPtr<ImageBridgeChild> child = new ImageBridgeChild();

  RefPtr<Runnable> runnable = NewRunnableMethod<Endpoint<PImageBridgeChild>&&>(
    child,
    &ImageBridgeChild::Bind,
    Move(aEndpoint));
  child->GetMessageLoop()->PostTask(runnable.forget());

  // Assign this after so other threads can't post messages before we connect to IPDL.
  {
    StaticMutexAutoLock lock(sImageBridgeSingletonLock);
    sImageBridgeChildSingleton = child;
  }

  return true;
}

bool
ImageBridgeChild::ReinitForContent(Endpoint<PImageBridgeChild>&& aEndpoint)
{
  MOZ_ASSERT(NS_IsMainThread());

  // Note that at this point, ActorDestroy may not have been called yet,
  // meaning mCanSend is still true. In this case we will try to send a
  // synchronous WillClose message to the parent, and will certainly get a
  // false result and a MsgDropped processing error. This is okay.
  ShutdownSingleton();

  return InitForContent(Move(aEndpoint));
}

void
ImageBridgeChild::Bind(Endpoint<PImageBridgeChild>&& aEndpoint)
{
  if (!aEndpoint.Bind(this)) {
    return;
  }

  // This reference is dropped in DeallocPImageBridgeChild.
  this->AddRef();

  mCanSend = true;
  SendImageBridgeThreadId();
}

void
ImageBridgeChild::BindSameProcess(RefPtr<ImageBridgeParent> aParent)
{
  MessageLoop *parentMsgLoop = aParent->GetMessageLoop();
  ipc::MessageChannel *parentChannel = aParent->GetIPCChannel();
  Open(parentChannel, parentMsgLoop, mozilla::ipc::ChildSide);

  // This reference is dropped in DeallocPImageBridgeChild.
  this->AddRef();

  mCanSend = true;
  SendImageBridgeThreadId();
}

/* static */ void
ImageBridgeChild::ShutDown()
{
  MOZ_ASSERT(NS_IsMainThread());

  ShutdownSingleton();

  delete sImageBridgeChildThread;
  sImageBridgeChildThread = nullptr;
}

/* static */ void
ImageBridgeChild::ShutdownSingleton()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (RefPtr<ImageBridgeChild> child = GetSingleton()) {
    child->WillShutdown();

    StaticMutexAutoLock lock(sImageBridgeSingletonLock);
    sImageBridgeChildSingleton = nullptr;
  }
}

void
ImageBridgeChild::WillShutdown()
{
  {
    SynchronousTask task("ImageBridge ShutdownStep1 lock");

    RefPtr<Runnable> runnable = WrapRunnable(
      RefPtr<ImageBridgeChild>(this),
      &ImageBridgeChild::ShutdownStep1,
      &task);
    GetMessageLoop()->PostTask(runnable.forget());

    task.Wait();
  }

  {
    SynchronousTask task("ImageBridge ShutdownStep2 lock");

    RefPtr<Runnable> runnable = WrapRunnable(
      RefPtr<ImageBridgeChild>(this),
      &ImageBridgeChild::ShutdownStep2,
      &task);
    GetMessageLoop()->PostTask(runnable.forget());

    task.Wait();
  }

  if (mShutdownObserver) {
    mShutdownObserver->Unregister();
    mShutdownObserver = nullptr;
  }
}

void
ImageBridgeChild::InitSameProcess()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on the main Thread!");

  MOZ_ASSERT(!sImageBridgeChildSingleton);
  MOZ_ASSERT(!sImageBridgeChildThread);

  sImageBridgeChildThread = new ImageBridgeThread();
  if (!sImageBridgeChildThread->IsRunning()) {
    sImageBridgeChildThread->Start();
  }

  RefPtr<ImageBridgeChild> child = new ImageBridgeChild();
  RefPtr<ImageBridgeParent> parent = ImageBridgeParent::CreateSameProcess();

  RefPtr<Runnable> runnable = WrapRunnable(
    child,
    &ImageBridgeChild::BindSameProcess,
    parent);
  child->GetMessageLoop()->PostTask(runnable.forget());

  // Assign this after so other threads can't post messages before we connect to IPDL.
  {
    StaticMutexAutoLock lock(sImageBridgeSingletonLock);
    sImageBridgeChildSingleton = child;
  }
}

/* static */ void
ImageBridgeChild::InitWithGPUProcess(Endpoint<PImageBridgeChild>&& aEndpoint)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!sImageBridgeChildSingleton);
  MOZ_ASSERT(!sImageBridgeChildThread);

  sImageBridgeChildThread = new ImageBridgeThread();
  if (!sImageBridgeChildThread->IsRunning()) {
    sImageBridgeChildThread->Start();
  }

  RefPtr<ImageBridgeChild> child = new ImageBridgeChild();

  MessageLoop* loop = child->GetMessageLoop();
  loop->PostTask(NewRunnableMethod<Endpoint<PImageBridgeChild>&&>(
    child, &ImageBridgeChild::Bind, Move(aEndpoint)));

  // Assign this after so other threads can't post messages before we connect to IPDL.
  {
    StaticMutexAutoLock lock(sImageBridgeSingletonLock);
    sImageBridgeChildSingleton = child;
  }
}

bool InImageBridgeChildThread()
{
  return sImageBridgeChildThread &&
    sImageBridgeChildThread->thread_id() == PlatformThread::CurrentId();
}

MessageLoop * ImageBridgeChild::GetMessageLoop() const
{
  return sImageBridgeChildThread ? sImageBridgeChildThread->message_loop() : nullptr;
}

/* static */ void
ImageBridgeChild::IdentifyCompositorTextureHost(const TextureFactoryIdentifier& aIdentifier)
{
  if (RefPtr<ImageBridgeChild> child = GetSingleton()) {
    child->IdentifyTextureHost(aIdentifier);
  }
}

RefPtr<ImageClient>
ImageBridgeChild::CreateImageClient(CompositableType aType,
                                    ImageContainer* aImageContainer,
                                    ImageContainerChild* aContainerChild)
{
  if (InImageBridgeChildThread()) {
    return CreateImageClientNow(aType, aImageContainer, aContainerChild);
  }

  SynchronousTask task("CreateImageClient Lock");

  RefPtr<ImageClient> result = nullptr;

  RefPtr<Runnable> runnable = WrapRunnable(
    RefPtr<ImageBridgeChild>(this),
    &ImageBridgeChild::CreateImageClientSync,
    &task,
    &result,
    aType,
    aImageContainer,
    aContainerChild);
  GetMessageLoop()->PostTask(runnable.forget());

  task.Wait();

  return result;
}

RefPtr<ImageClient>
ImageBridgeChild::CreateImageClientNow(CompositableType aType,
                                       ImageContainer* aImageContainer,
                                       ImageContainerChild* aContainerChild)
{
  MOZ_ASSERT(InImageBridgeChildThread());

  if (aImageContainer) {
    SendPImageContainerConstructor(aContainerChild);
    aContainerChild->RegisterWithIPDL();
  }

  RefPtr<ImageClient> client = ImageClient::CreateImageClient(aType, this, TextureFlags::NO_FLAGS);
  MOZ_ASSERT(client, "failed to create ImageClient");
  if (client) {
    client->Connect(aImageContainer);
  }
  return client;
}

already_AddRefed<CanvasClient>
ImageBridgeChild::CreateCanvasClient(CanvasClient::CanvasClientType aType,
                                     TextureFlags aFlag)
{
  if (InImageBridgeChildThread()) {
    return CreateCanvasClientNow(aType, aFlag);
  }

  SynchronousTask task("CreateCanvasClient Lock");

  // RefPtrs on arguments are not needed since this dispatches synchronously.
  RefPtr<CanvasClient> result = nullptr;
  RefPtr<Runnable> runnable = WrapRunnable(
    RefPtr<ImageBridgeChild>(this),
    &ImageBridgeChild::CreateCanvasClientSync,
    &task,
    aType,
    aFlag,
    &result);
  GetMessageLoop()->PostTask(runnable.forget());

  task.Wait();

  return result.forget();
}

already_AddRefed<CanvasClient>
ImageBridgeChild::CreateCanvasClientNow(CanvasClient::CanvasClientType aType,
                                        TextureFlags aFlag)
{
  RefPtr<CanvasClient> client
    = CanvasClient::CreateCanvasClient(aType, this, aFlag);
  MOZ_ASSERT(client, "failed to create CanvasClient");
  if (client) {
    client->Connect();
  }
  return client.forget();
}

bool
ImageBridgeChild::AllocUnsafeShmem(size_t aSize,
                                   ipc::SharedMemory::SharedMemoryType aType,
                                   ipc::Shmem* aShmem)
{
  if (!InImageBridgeChildThread()) {
    return DispatchAllocShmemInternal(aSize, aType, aShmem, true); // true: unsafe
  }

  if (!CanSend()) {
    return false;
  }
  return PImageBridgeChild::AllocUnsafeShmem(aSize, aType, aShmem);
}

bool
ImageBridgeChild::AllocShmem(size_t aSize,
                             ipc::SharedMemory::SharedMemoryType aType,
                             ipc::Shmem* aShmem)
{
  if (!InImageBridgeChildThread()) {
    return DispatchAllocShmemInternal(aSize, aType, aShmem, false); // false: unsafe
  }

  if (!CanSend()) {
    return false;
  }
  return PImageBridgeChild::AllocShmem(aSize, aType, aShmem);
}

// NewRunnableFunction accepts a limited number of parameters so we need a
// struct here
struct AllocShmemParams {
  size_t mSize;
  ipc::SharedMemory::SharedMemoryType mType;
  ipc::Shmem* mShmem;
  bool mUnsafe;
  bool mSuccess;
};

void
ImageBridgeChild::ProxyAllocShmemNow(SynchronousTask* aTask, AllocShmemParams* aParams)
{
  AutoCompleteTask complete(aTask);

  if (!CanSend()) {
    return;
  }

  bool ok = false;
  if (aParams->mUnsafe) {
    ok = AllocUnsafeShmem(aParams->mSize, aParams->mType, aParams->mShmem);
  } else {
    ok = AllocShmem(aParams->mSize, aParams->mType, aParams->mShmem);
  }
  aParams->mSuccess = ok;
}

bool
ImageBridgeChild::DispatchAllocShmemInternal(size_t aSize,
                                             SharedMemory::SharedMemoryType aType,
                                             ipc::Shmem* aShmem,
                                             bool aUnsafe)
{
  SynchronousTask task("AllocatorProxy alloc");

  AllocShmemParams params = {
    aSize, aType, aShmem, aUnsafe, false
  };

  RefPtr<Runnable> runnable = WrapRunnable(
    RefPtr<ImageBridgeChild>(this),
    &ImageBridgeChild::ProxyAllocShmemNow,
    &task,
    &params);
  GetMessageLoop()->PostTask(runnable.forget());

  task.Wait();

  return params.mSuccess;
}

void
ImageBridgeChild::ProxyDeallocShmemNow(SynchronousTask* aTask,
                                       ISurfaceAllocator* aAllocator,
                                       ipc::Shmem* aShmem)
{
  AutoCompleteTask complete(aTask);

  if (!CanSend()) {
    return;
  }
  DeallocShmem(*aShmem);
}

void
ImageBridgeChild::DeallocShmem(ipc::Shmem& aShmem)
{
  if (InImageBridgeChildThread()) {
    if (!CanSend()) {
      return;
    }
    PImageBridgeChild::DeallocShmem(aShmem);
    return;
  }

  SynchronousTask task("AllocatorProxy Dealloc");

  RefPtr<Runnable> runnable = WrapRunnable(
    RefPtr<ImageBridgeChild>(this),
    &ImageBridgeChild::ProxyDeallocShmemNow,
    &task,
    this,
    &aShmem);
  GetMessageLoop()->PostTask(runnable.forget());

  task.Wait();
}

PTextureChild*
ImageBridgeChild::AllocPTextureChild(const SurfaceDescriptor&,
                                     const LayersBackend&,
                                     const TextureFlags&,
                                     const uint64_t& aSerial)
{
  MOZ_ASSERT(CanSend());
  return TextureClient::CreateIPDLActor();
}

bool
ImageBridgeChild::DeallocPTextureChild(PTextureChild* actor)
{
  return TextureClient::DestroyIPDLActor(actor);
}

PMediaSystemResourceManagerChild*
ImageBridgeChild::AllocPMediaSystemResourceManagerChild()
{
  MOZ_ASSERT(CanSend());
  return new mozilla::media::MediaSystemResourceManagerChild();
}

bool
ImageBridgeChild::DeallocPMediaSystemResourceManagerChild(PMediaSystemResourceManagerChild* aActor)
{
  MOZ_ASSERT(aActor);
  delete static_cast<mozilla::media::MediaSystemResourceManagerChild*>(aActor);
  return true;
}

PImageContainerChild*
ImageBridgeChild::AllocPImageContainerChild()
{
  // we always use the "power-user" ctor
  NS_RUNTIMEABORT("not reached");
  return nullptr;
}

bool
ImageBridgeChild::DeallocPImageContainerChild(PImageContainerChild* actor)
{
  static_cast<ImageContainerChild*>(actor)->UnregisterFromIPDL();
  return true;
}

bool
ImageBridgeChild::RecvParentAsyncMessages(InfallibleTArray<AsyncParentMessageData>&& aMessages)
{
  for (AsyncParentMessageArray::index_type i = 0; i < aMessages.Length(); ++i) {
    const AsyncParentMessageData& message = aMessages[i];

    switch (message.type()) {
      case AsyncParentMessageData::TOpDeliverFence: {
        const OpDeliverFence& op = message.get_OpDeliverFence();
        FenceHandle fence = op.fence();
        DeliverFence(op.TextureId(), fence);
        break;
      }
      case AsyncParentMessageData::TOpDeliverFenceToNonRecycle: {
        // Notify ReleaseCompositableRef to a TextureClient that belongs to
        // LayerTransactionChild. It is used only on gonk to deliver fence to
        // a TextureClient that does not have TextureFlags::RECYCLE.
        // In this case, LayerTransactionChild's ipc could not be used to deliver fence.

        const OpDeliverFenceToNonRecycle& op = message.get_OpDeliverFenceToNonRecycle();
        FenceHandle fence = op.fence();
        DeliverFenceToNonRecycle(op.TextureId(), fence);
        break;
      }
      case AsyncParentMessageData::TOpNotifyNotUsed: {
        const OpNotifyNotUsed& op = message.get_OpNotifyNotUsed();
        NotifyNotUsed(op.TextureId(), op.fwdTransactionId());
        break;
      }
      case AsyncParentMessageData::TOpNotifyNotUsedToNonRecycle: {
        // Notify ReleaseCompositableRef to a TextureClient that belongs to
        // LayerTransactionChild. It is used only on gonk to deliver fence to
        // a TextureClient that does not have TextureFlags::RECYCLE.
        // In this case, LayerTransactionChild's ipc could not be used to deliver fence.

        const OpNotifyNotUsedToNonRecycle& op = message.get_OpNotifyNotUsedToNonRecycle();
        NotifyNotUsedToNonRecycle(op.TextureId(), op.fwdTransactionId());
        break;
      }
      case AsyncParentMessageData::TOpReplyRemoveTexture: {
        const OpReplyRemoveTexture& op = message.get_OpReplyRemoveTexture();

        MOZ_ASSERT(mTrackersHolder.GetId() == op.holderId());
        mTrackersHolder.TransactionCompleteted(op.transactionId());
        break;
      }
      default:
        NS_ERROR("unknown AsyncParentMessageData type");
        return false;
    }
  }
  return true;
}

bool
ImageBridgeChild::RecvDidComposite(InfallibleTArray<ImageCompositeNotification>&& aNotifications)
{
  for (auto& n : aNotifications) {
    ImageContainerChild* child =
      static_cast<ImageContainerChild*>(n.imageContainerChild());
    if (child) {
      child->NotifyComposite(n);
    }
  }
  return true;
}

PTextureChild*
ImageBridgeChild::CreateTexture(const SurfaceDescriptor& aSharedData,
                                LayersBackend aLayersBackend,
                                TextureFlags aFlags,
                                uint64_t aSerial)
{
  MOZ_ASSERT(CanSend());
  return SendPTextureConstructor(aSharedData, aLayersBackend, aFlags, aSerial);
}

static bool
IBCAddOpDestroy(CompositableTransaction* aTxn, const OpDestroy& op, bool synchronously)
{
  if (aTxn->Finished()) {
    return false;
  }

  aTxn->mDestroyedActors.AppendElement(op);

  if (synchronously) {
    aTxn->MarkSyncTransaction();
  }

  return true;
}

bool
ImageBridgeChild::DestroyInTransaction(PTextureChild* aTexture, bool synchronously)
{
  return IBCAddOpDestroy(mTxn, OpDestroy(aTexture), synchronously);
}

bool
ImageBridgeChild::DestroyInTransaction(PCompositableChild* aCompositable, bool synchronously)
{
  return IBCAddOpDestroy(mTxn, OpDestroy(aCompositable), synchronously);
}


void
ImageBridgeChild::RemoveTextureFromCompositable(CompositableClient* aCompositable,
                                                TextureClient* aTexture)
{
  MOZ_ASSERT(CanSend());
  MOZ_ASSERT(aTexture);
  MOZ_ASSERT(aTexture->IsSharedWithCompositor());
  MOZ_ASSERT(aCompositable->IsConnected());
  if (!aTexture || !aTexture->IsSharedWithCompositor() || !aCompositable->IsConnected()) {
    return;
  }

  CompositableOperation op(
    nullptr, aCompositable->GetIPDLActor(),
    OpRemoveTexture(nullptr, aTexture->GetIPDLActor()));

  if (aTexture->GetFlags() & TextureFlags::DEALLOCATE_CLIENT) {
    mTxn->AddEdit(op);
  } else {
    mTxn->AddNoSwapEdit(op);
  }
}

void
ImageBridgeChild::RemoveTextureFromCompositableAsync(AsyncTransactionTracker* aAsyncTransactionTracker,
                                                     CompositableClient* aCompositable,
                                                     TextureClient* aTexture)
{
  MOZ_ASSERT(CanSend());
  MOZ_ASSERT(aTexture);
  MOZ_ASSERT(aTexture->IsSharedWithCompositor());
  MOZ_ASSERT(aCompositable->IsConnected());
  if (!aTexture || !aTexture->IsSharedWithCompositor() || !aCompositable->IsConnected()) {
    return;
  }

  CompositableOperation op(
    nullptr, aCompositable->GetIPDLActor(),
    OpRemoveTextureAsync(
      mTrackersHolder.GetId(),
      aAsyncTransactionTracker->GetId(),
      nullptr, aCompositable->GetIPDLActor(),
      nullptr, aTexture->GetIPDLActor()));

  mTxn->AddNoSwapEdit(op);
  // Hold AsyncTransactionTracker until receving reply
  mTrackersHolder.HoldUntilComplete(aAsyncTransactionTracker);
}

bool ImageBridgeChild::IsSameProcess() const
{
  return OtherPid() == base::GetCurrentProcId();
}

void
ImageBridgeChild::Destroy(CompositableChild* aCompositable)
{
  if (!InImageBridgeChildThread()) {
    RefPtr<Runnable> runnable = WrapRunnable(
      RefPtr<ImageBridgeChild>(this),
      &ImageBridgeChild::Destroy,
      RefPtr<CompositableChild>(aCompositable));
    GetMessageLoop()->PostTask(runnable.forget());
    return;
  }
  CompositableForwarder::Destroy(aCompositable);
}

bool
ImageBridgeChild::CanSend() const
{
  MOZ_ASSERT(InImageBridgeChildThread());
  return mCanSend;
}

void
ImageBridgeChild::OnXPCOMShutdown()
{
  // This uses nsIObserverService, so it must be cleaned up. Other threads may
  // hold references to ImageBridgeChild and we may actually be destroyed well
  // after XPCOM shutdown.
  mActiveResourceTracker = nullptr;
}

NS_IMPL_ISUPPORTS(ImageBridgeChild::ShutdownObserver, nsIObserver);

ImageBridgeChild::ShutdownObserver::ShutdownObserver(ImageBridgeChild* aImageBridge)
 : mImageBridge(aImageBridge)
{
  nsContentUtils::RegisterShutdownObserver(this);
}

void
ImageBridgeChild::ShutdownObserver::Unregister()
{
  nsContentUtils::UnregisterShutdownObserver(this);
  mImageBridge = nullptr;
}

NS_IMETHODIMP
ImageBridgeChild::ShutdownObserver::Observe(nsISupports* aSubject,
                                            const char* aTopic,
                                            const char16_t* aData)
{
  if (!strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    mImageBridge->OnXPCOMShutdown();
  }
  return NS_OK;
}

} // namespace layers
} // namespace mozilla
