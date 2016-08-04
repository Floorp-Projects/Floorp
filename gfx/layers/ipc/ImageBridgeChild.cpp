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
#include "mozilla/layers/CompositableClient.h"  // for CompositableChild, etc
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/layers/ISurfaceAllocator.h"  // for ISurfaceAllocator
#include "mozilla/layers/ImageClient.h"  // for ImageClient
#include "mozilla/layers/LayersMessages.h"  // for CompositableOperation
#include "mozilla/layers/PCompositableChild.h"  // for PCompositableChild
#include "mozilla/layers/PImageContainerChild.h"
#include "mozilla/layers/TextureClient.h"  // for TextureClient
#include "mozilla/mozalloc.h"           // for operator new, etc
#include "nsISupportsImpl.h"            // for ImageContainer::AddRef, etc
#include "nsTArray.h"                   // for AutoTArray, nsTArray, etc
#include "nsTArrayForwardDeclare.h"     // for AutoTArray
#include "nsThreadUtils.h"              // for NS_IsMainThread
#include "nsXULAppAPI.h"                // for XRE_GetIOMessageLoop
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
  PseudoStack* mPseudoStackHack;
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

  void FallbackDestroyActors()
  {
    for (auto& actor : mDestroyedActors) {
      switch (actor.type()) {
      case OpDestroy::TPTextureChild: {
        DebugOnly<bool> ok = TextureClient::DestroyFallback(actor.get_PTextureChild());
        MOZ_ASSERT(ok);
        break;
      }
      case OpDestroy::TPCompositableChild: {
        DebugOnly<bool> ok = CompositableClient::DestroyFallback(actor.get_PCompositableChild());
        MOZ_ASSERT(ok);
        break;
      }
      default:
        MOZ_CRASH("GFX: IBC Fallback destroy actors");
      }
    }
    mDestroyedActors.Clear();
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

/* static */
Atomic<bool> ImageBridgeChild::sIsShutDown(false);

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
                                        t.mFrameID, t.mProducerID, t.mInputFrameID));

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
static StaticRefPtr<ImageBridgeChild> sImageBridgeChildSingleton;
static Thread *sImageBridgeChildThread = nullptr;

void
ImageBridgeChild::FallbackDestroyActors() {
  if (mTxn && !mTxn->mDestroyedActors.IsEmpty()) {
    mTxn->FallbackDestroyActors();
  }
}

// dispatched function
static void ImageBridgeShutdownStep1(ReentrantMonitor *aBarrier, bool *aDone)
{
  ReentrantMonitorAutoEnter autoMon(*aBarrier);

  MOZ_ASSERT(InImageBridgeChildThread(),
             "Should be in ImageBridgeChild thread.");

  MediaSystemResourceManager::Shutdown();

  if (sImageBridgeChildSingleton) {
    // Force all managed protocols to shut themselves down cleanly
    InfallibleTArray<PCompositableChild*> compositables;
    sImageBridgeChildSingleton->ManagedPCompositableChild(compositables);
    for (int i = compositables.Length() - 1; i >= 0; --i) {
      auto compositable = CompositableClient::FromIPDLActor(compositables[i]);
      if (compositable) {
        compositable->Destroy();
      }
    }
    InfallibleTArray<PTextureChild*> textures;
    sImageBridgeChildSingleton->ManagedPTextureChild(textures);
    for (int i = textures.Length() - 1; i >= 0; --i) {
      RefPtr<TextureClient> client = TextureClient::AsTextureClient(textures[i]);
      if (client) {
        client->Destroy();
      }
    }
    sImageBridgeChildSingleton->FallbackDestroyActors();

    sImageBridgeChildSingleton->SendWillClose();
    sImageBridgeChildSingleton->MarkShutDown();
    // From now on, no message can be sent through the image bridge from the
    // client side except the final Stop message.
  }

  *aDone = true;
  aBarrier->NotifyAll();
}

// dispatched function
static void ImageBridgeShutdownStep2(ReentrantMonitor *aBarrier, bool *aDone)
{
  ReentrantMonitorAutoEnter autoMon(*aBarrier);

  MOZ_ASSERT(InImageBridgeChildThread(),
             "Should be in ImageBridgeChild thread.");

  sImageBridgeChildSingleton->Close();

  *aDone = true;
  aBarrier->NotifyAll();
}

// dispatched function
static void CreateImageClientSync(RefPtr<ImageClient>* result,
                                  ReentrantMonitor* barrier,
                                  CompositableType aType,
                                  ImageContainer* aImageContainer,
                                  bool *aDone)
{
  ReentrantMonitorAutoEnter autoMon(*barrier);
  *result = sImageBridgeChildSingleton->CreateImageClientNow(
      aType, aImageContainer);
  *aDone = true;
  barrier->NotifyAll();
}

// dispatched function
static void CreateCanvasClientSync(ReentrantMonitor* aBarrier,
                                   CanvasClient::CanvasClientType aType,
                                   TextureFlags aFlags,
                                   RefPtr<CanvasClient>* const outResult,
                                   bool* aDone)
{
  ReentrantMonitorAutoEnter autoMon(*aBarrier);
  *outResult = sImageBridgeChildSingleton->CreateCanvasClientNow(aType, aFlags);
  *aDone = true;
  aBarrier->NotifyAll();
}

static void ConnectImageBridge(ImageBridgeChild * child, ImageBridgeParent * parent)
{
  MessageLoop *parentMsgLoop = parent->GetMessageLoop();
  ipc::MessageChannel *parentChannel = parent->GetIPCChannel();
  child->Open(parentChannel, parentMsgLoop, mozilla::ipc::ChildSide);
}

ImageBridgeChild::ImageBridgeChild()
  : mShuttingDown(false)
  , mFwdTransactionId(0)
#ifdef MOZ_WIDGET_GONK
  , mWaitingFenceHandleMutex("ImageBridgeChild::mWaitingFenceHandleMutex")
#endif
{
  MOZ_ASSERT(NS_IsMainThread());

  mTxn = new CompositableTransaction();
}

ImageBridgeChild::~ImageBridgeChild()
{
  delete mTxn;
}

void
ImageBridgeChild::MarkShutDown()
{
  MOZ_ASSERT(!mShuttingDown);
  mTexturesWaitingRecycled.Clear();
  mTrackersHolder.DestroyAsyncTransactionTrackersHolder();

  mShuttingDown = true;
}

void
ImageBridgeChild::Connect(CompositableClient* aCompositable,
                          ImageContainer* aImageContainer)
{
  MOZ_ASSERT(aCompositable);
  MOZ_ASSERT(!mShuttingDown);
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
  MOZ_ASSERT(!mShuttingDown);
  return CompositableClient::CreateIPDLActor();
}

bool
ImageBridgeChild::DeallocPCompositableChild(PCompositableChild* aActor)
{
  return CompositableClient::DestroyIPDLActor(aActor);
}


Thread* ImageBridgeChild::GetThread() const
{
  return sImageBridgeChildThread;
}

ImageBridgeChild* ImageBridgeChild::GetSingleton()
{
  return sImageBridgeChildSingleton;
}

bool ImageBridgeChild::IsCreated()
{
  return GetSingleton() != nullptr;
}

static void ReleaseImageClientNow(ImageClient* aClient,
                                  PImageContainerChild* aChild)
{
  MOZ_ASSERT(InImageBridgeChildThread());
  if (aClient) {
    aClient->Release();
  }

  if (aChild) {
    ImageContainer::AsyncDestroyActor(aChild);
  }
}

// static
void ImageBridgeChild::DispatchReleaseImageClient(ImageClient* aClient,
                                                  PImageContainerChild* aChild)
{
  if (!aClient && !aChild) {
    return;
  }

  if (!IsCreated()) {
    if (aClient) {
      // CompositableClient::Release should normally happen in the ImageBridgeChild
      // thread because it usually generate some IPDL messages.
      // However, if we take this branch it means that the ImageBridgeChild
      // has already shut down, along with the CompositableChild, which means no
      // message will be sent and it is safe to run this code from any thread.
      MOZ_ASSERT(aClient->GetIPDLActor() == nullptr);
      aClient->Release();
    }
    delete aChild;
    return;
  }

  sImageBridgeChildSingleton->GetMessageLoop()->PostTask(
    NewRunnableFunction(&ReleaseImageClientNow, aClient, aChild));
}

static void ReleaseCanvasClientNow(CanvasClient* aClient)
{
  MOZ_ASSERT(InImageBridgeChildThread());
  aClient->Release();
}

// static
void ImageBridgeChild::DispatchReleaseCanvasClient(CanvasClient* aClient)
{
  if (!aClient) {
    return;
  }

  if (!IsCreated()) {
    // CompositableClient::Release should normally happen in the ImageBridgeChild
    // thread because it usually generate some IPDL messages.
    // However, if we take this branch it means that the ImageBridgeChild
    // has already shut down, along with the CompositableChild, which means no
    // message will be sent and it is safe to run this code from any thread.
    MOZ_ASSERT(aClient->GetIPDLActor() == nullptr);
    aClient->Release();
    return;
  }

  sImageBridgeChildSingleton->GetMessageLoop()->PostTask(
    NewRunnableFunction(&ReleaseCanvasClientNow, aClient));
}

static void ReleaseTextureClientNow(TextureClient* aClient)
{
  MOZ_ASSERT(InImageBridgeChildThread());
  RELEASE_MANUALLY(aClient);
}

// static
void ImageBridgeChild::DispatchReleaseTextureClient(TextureClient* aClient)
{
  if (!aClient) {
    return;
  }

  if (!IsCreated()) {
    // TextureClient::Release should normally happen in the ImageBridgeChild
    // thread because it usually generate some IPDL messages.
    // However, if we take this branch it means that the ImageBridgeChild
    // has already shut down, along with the TextureChild, which means no
    // message will be sent and it is safe to run this code from any thread.
    MOZ_ASSERT(aClient->GetIPDLActor() == nullptr);
    RELEASE_MANUALLY(aClient);
    return;
  }

  sImageBridgeChildSingleton->GetMessageLoop()->PostTask(
    NewRunnableFunction(&ReleaseTextureClientNow, aClient));
}

static void UpdateImageClientNow(ImageClient* aClient, RefPtr<ImageContainer>&& aContainer)
{
  if (!ImageBridgeChild::IsCreated() || ImageBridgeChild::IsShutDown()) {
    NS_WARNING("Something is holding on to graphics resources after the shutdown"
               "of the graphics subsystem!");
    return;
  }
  MOZ_ASSERT(aClient);
  MOZ_ASSERT(aContainer);
  sImageBridgeChildSingleton->BeginTransaction();
  aClient->UpdateImage(aContainer, Layer::CONTENT_OPAQUE);
  sImageBridgeChildSingleton->EndTransaction();
}

// static
void ImageBridgeChild::DispatchImageClientUpdate(ImageClient* aClient,
                                                 ImageContainer* aContainer)
{
  if (!ImageBridgeChild::IsCreated() || ImageBridgeChild::IsShutDown()) {
    NS_WARNING("Something is holding on to graphics resources after the shutdown"
               "of the graphics subsystem!");
    return;
  }
  if (!aClient || !aContainer || !IsCreated()) {
    return;
  }

  if (InImageBridgeChildThread()) {
    UpdateImageClientNow(aClient, aContainer);
    return;
  }
  sImageBridgeChildSingleton->GetMessageLoop()->PostTask(
    NewRunnableFunction(&UpdateImageClientNow, aClient, RefPtr<ImageContainer>(aContainer)));
}

static void UpdateAsyncCanvasRendererSync(AsyncCanvasRenderer* aWrapper,
                                          ReentrantMonitor* aBarrier,
                                          bool* const outDone)
{
  ImageBridgeChild::UpdateAsyncCanvasRendererNow(aWrapper);

  ReentrantMonitorAutoEnter autoMon(*aBarrier);
  *outDone = true;
  aBarrier->NotifyAll();
}

// static
void ImageBridgeChild::UpdateAsyncCanvasRenderer(AsyncCanvasRenderer* aWrapper)
{
  aWrapper->GetCanvasClient()->UpdateAsync(aWrapper);

  if (InImageBridgeChildThread()) {
    UpdateAsyncCanvasRendererNow(aWrapper);
    return;
  }

  ReentrantMonitor barrier("UpdateAsyncCanvasRenderer Lock");
  ReentrantMonitorAutoEnter autoMon(barrier);
  bool done = false;

  sImageBridgeChildSingleton->GetMessageLoop()->PostTask(
    NewRunnableFunction(&UpdateAsyncCanvasRendererSync, aWrapper, &barrier, &done));

  // should stop the thread until the CanvasClient has been created on
  // the other thread
  while (!done) {
    barrier.Wait();
  }
}

// static
void ImageBridgeChild::UpdateAsyncCanvasRendererNow(AsyncCanvasRenderer* aWrapper)
{
  MOZ_ASSERT(aWrapper);
  sImageBridgeChildSingleton->BeginTransaction();
  aWrapper->GetCanvasClient()->Updated();
  sImageBridgeChildSingleton->EndTransaction();
}

static void FlushAllImagesSync(ImageClient* aClient, ImageContainer* aContainer,
                               RefPtr<AsyncTransactionWaiter>&& aWaiter,
                               ReentrantMonitor* aBarrier,
                               bool* const outDone)
{
#ifdef MOZ_WIDGET_GONK
  MOZ_ASSERT(aWaiter);
#else
  MOZ_ASSERT(!aWaiter);
#endif
  ReentrantMonitorAutoEnter autoMon(*aBarrier);

  if (!ImageBridgeChild::IsCreated() || ImageBridgeChild::IsShutDown()) {
    // How sad. If we get into this branch it means that the ImageBridge
    // got destroyed between the time we ImageBridgeChild::FlushAllImage
    // was called on some thread, and the time this function was proxied
    // to the ImageBridge thread. ImageBridge gets destroyed way to late
    // in the shutdown of gecko for this to be happening for a good reason.
    NS_WARNING("Something is holding on to graphics resources after the shutdown"
               "of the graphics subsystem!");
#ifdef MOZ_WIDGET_GONK
    aWaiter->DecrementWaitCount();
#endif

    *outDone = true;
    aBarrier->NotifyAll();
    return;
  }
  MOZ_ASSERT(aClient);
  sImageBridgeChildSingleton->BeginTransaction();
  if (aContainer) {
    aContainer->ClearImagesFromImageBridge();
  }
  aClient->FlushAllImages(aWaiter);
  sImageBridgeChildSingleton->EndTransaction();
  // This decrement is balanced by the increment in FlushAllImages.
  // If any AsyncTransactionTrackers were created by FlushAllImages and attached
  // to aWaiter, aWaiter will not complete until those trackers all complete.
  // Otherwise, aWaiter will be ready to complete now.
#ifdef MOZ_WIDGET_GONK
  aWaiter->DecrementWaitCount();
#endif
  *outDone = true;
  aBarrier->NotifyAll();
}

// static
void ImageBridgeChild::FlushAllImages(ImageClient* aClient,
                                      ImageContainer* aContainer)
{
  if (!IsCreated() || IsShutDown()) {
    return;
  }
  MOZ_ASSERT(aClient);
  MOZ_ASSERT(!sImageBridgeChildSingleton->mShuttingDown);
  MOZ_ASSERT(!InImageBridgeChildThread());
  if (InImageBridgeChildThread()) {
    NS_ERROR("ImageBridgeChild::FlushAllImages() is called on ImageBridge thread.");
    return;
  }

  ReentrantMonitor barrier("FlushAllImages Lock");
  ReentrantMonitorAutoEnter autoMon(barrier);
  bool done = false;

  RefPtr<AsyncTransactionWaiter> waiter;
#ifdef MOZ_WIDGET_GONK
  waiter = new AsyncTransactionWaiter();
  // This increment is balanced by the decrement in FlushAllImagesSync
  waiter->IncrementWaitCount();
#endif
  sImageBridgeChildSingleton->GetMessageLoop()->PostTask(
    NewRunnableFunction(&FlushAllImagesSync, aClient, aContainer, waiter, &barrier, &done));

  while (!done) {
    barrier.Wait();
  }

#ifdef MOZ_WIDGET_GONK
  waiter->WaitComplete();
#endif
}

void
ImageBridgeChild::BeginTransaction()
{
  MOZ_ASSERT(!mShuttingDown);
  MOZ_ASSERT(mTxn->Finished(), "uncommitted txn?");
  UpdateFwdTransactionId();
  mTxn->Begin();
}

void
ImageBridgeChild::EndTransaction()
{
  MOZ_ASSERT(!mShuttingDown);
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
      mTxn->FallbackDestroyActors();
      return;
    }
  } else {
    // If we don't require a swap we can call SendUpdateNoSwap which
    // assumes that aReplies is empty (DEBUG assertion)
    if (!SendUpdateNoSwap(cset, mTxn->mDestroyedActors, GetFwdTransactionId())) {
      NS_WARNING("could not send async texture transaction (no swap)");
      mTxn->FallbackDestroyActors();
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
  SendImageBridgeThreadId(gettid());
#endif
}

static void CallSendImageBridgeThreadId(ImageBridgeChild* aImageBridgeChild)
{
  MOZ_ASSERT(InImageBridgeChildThread());
  aImageBridgeChild->SendImageBridgeThreadId();
}

bool
ImageBridgeChild::InitForContent(Endpoint<PImageBridgeChild>&& aEndpoint)
{
  MOZ_ASSERT(NS_IsMainThread());

  gfxPlatform::GetPlatform();

  sImageBridgeChildThread = new ImageBridgeThread();
  if (!sImageBridgeChildThread->Start()) {
    return false;
  }

  sImageBridgeChildSingleton = new ImageBridgeChild();

  MessageLoop* loop = sImageBridgeChildSingleton->GetMessageLoop();

  loop->PostTask(NewRunnableMethod<Endpoint<PImageBridgeChild>&&>(
    sImageBridgeChildSingleton, &ImageBridgeChild::Bind, Move(aEndpoint)));
  loop->PostTask(NewRunnableFunction(
    CallSendImageBridgeThreadId, sImageBridgeChildSingleton.get()));

  return sImageBridgeChildSingleton;
}

void
ImageBridgeChild::Bind(Endpoint<PImageBridgeChild>&& aEndpoint)
{
  aEndpoint.Bind(this, nullptr);
}

void ImageBridgeChild::ShutDown()
{
  MOZ_ASSERT(NS_IsMainThread());

  sIsShutDown = true;

  if (ImageBridgeChild::IsCreated()) {
    MOZ_ASSERT(!sImageBridgeChildSingleton->mShuttingDown);

    {
      ReentrantMonitor barrier("ImageBridge ShutdownStep1 lock");
      ReentrantMonitorAutoEnter autoMon(barrier);

      bool done = false;
      sImageBridgeChildSingleton->GetMessageLoop()->PostTask(
                      NewRunnableFunction(&ImageBridgeShutdownStep1, &barrier, &done));
      while (!done) {
        barrier.Wait();
      }
    }

    {
      ReentrantMonitor barrier("ImageBridge ShutdownStep2 lock");
      ReentrantMonitorAutoEnter autoMon(barrier);

      bool done = false;
      sImageBridgeChildSingleton->GetMessageLoop()->PostTask(
                      NewRunnableFunction(&ImageBridgeShutdownStep2, &barrier, &done));
      while (!done) {
        barrier.Wait();
      }
    }

    sImageBridgeChildSingleton = nullptr;

    delete sImageBridgeChildThread;
    sImageBridgeChildThread = nullptr;
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

  sImageBridgeChildSingleton = new ImageBridgeChild();
  RefPtr<ImageBridgeParent> parent = ImageBridgeParent::CreateSameProcess();

  sImageBridgeChildSingleton->ConnectAsync(parent);
  sImageBridgeChildSingleton->GetMessageLoop()->PostTask(
    NewRunnableFunction(CallSendImageBridgeThreadId,
                        sImageBridgeChildSingleton.get()));
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

  sImageBridgeChildSingleton = new ImageBridgeChild();

  MessageLoop* loop = sImageBridgeChildSingleton->GetMessageLoop();
  loop->PostTask(NewRunnableMethod<Endpoint<PImageBridgeChild>&&>(
    sImageBridgeChildSingleton, &ImageBridgeChild::Bind, Move(aEndpoint)));
}

bool InImageBridgeChildThread()
{
  return ImageBridgeChild::IsCreated() &&
    sImageBridgeChildThread->thread_id() == PlatformThread::CurrentId();
}

MessageLoop * ImageBridgeChild::GetMessageLoop() const
{
  return sImageBridgeChildThread ? sImageBridgeChildThread->message_loop() : nullptr;
}

void ImageBridgeChild::ConnectAsync(ImageBridgeParent* aParent)
{
  GetMessageLoop()->PostTask(NewRunnableFunction(&ConnectImageBridge,
                                                 this, aParent));
}

void
ImageBridgeChild::IdentifyCompositorTextureHost(const TextureFactoryIdentifier& aIdentifier)
{
  if (sImageBridgeChildSingleton) {
    sImageBridgeChildSingleton->IdentifyTextureHost(aIdentifier);
  }
}

already_AddRefed<ImageClient>
ImageBridgeChild::CreateImageClient(CompositableType aType,
                                    ImageContainer* aImageContainer)
{
  if (InImageBridgeChildThread()) {
    return CreateImageClientNow(aType, aImageContainer);
  }
  ReentrantMonitor barrier("CreateImageClient Lock");
  ReentrantMonitorAutoEnter autoMon(barrier);
  bool done = false;

  RefPtr<ImageClient> result = nullptr;
  GetMessageLoop()->PostTask(
      NewRunnableFunction(&CreateImageClientSync, &result, &barrier, aType,
                          aImageContainer, &done));
  // should stop the thread until the ImageClient has been created on
  // the other thread
  while (!done) {
    barrier.Wait();
  }
  return result.forget();
}

already_AddRefed<ImageClient>
ImageBridgeChild::CreateImageClientNow(CompositableType aType,
                                       ImageContainer* aImageContainer)
{
  MOZ_ASSERT(!sImageBridgeChildSingleton->mShuttingDown);
  if (aImageContainer) {
    SendPImageContainerConstructor(aImageContainer->GetPImageContainerChild());
  }
  RefPtr<ImageClient> client
    = ImageClient::CreateImageClient(aType, this, TextureFlags::NO_FLAGS);
  MOZ_ASSERT(client, "failed to create ImageClient");
  if (client) {
    client->Connect(aImageContainer);
  }
  return client.forget();
}

already_AddRefed<CanvasClient>
ImageBridgeChild::CreateCanvasClient(CanvasClient::CanvasClientType aType,
                                     TextureFlags aFlag)
{
  if (InImageBridgeChildThread()) {
    return CreateCanvasClientNow(aType, aFlag);
  }
  ReentrantMonitor barrier("CreateCanvasClient Lock");
  ReentrantMonitorAutoEnter autoMon(barrier);
  bool done = false;

  RefPtr<CanvasClient> result = nullptr;
  GetMessageLoop()->PostTask(NewRunnableFunction(&CreateCanvasClientSync,
                                 &barrier, aType, aFlag, &result, &done));
  // should stop the thread until the CanvasClient has been created on the
  // other thread
  while (!done) {
    barrier.Wait();
  }
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
  MOZ_ASSERT(!mShuttingDown);
  if (InImageBridgeChildThread()) {
    return PImageBridgeChild::AllocUnsafeShmem(aSize, aType, aShmem);
  } else {
    return DispatchAllocShmemInternal(aSize, aType, aShmem, true); // true: unsafe
  }
}

bool
ImageBridgeChild::AllocShmem(size_t aSize,
                             ipc::SharedMemory::SharedMemoryType aType,
                             ipc::Shmem* aShmem)
{
  MOZ_ASSERT(!mShuttingDown);
  if (InImageBridgeChildThread()) {
    return PImageBridgeChild::AllocShmem(aSize, aType,
                                         aShmem);
  } else {
    return DispatchAllocShmemInternal(aSize, aType, aShmem, false); // false: unsafe
  }
}

// NewRunnableFunction accepts a limited number of parameters so we need a
// struct here
struct AllocShmemParams {
  RefPtr<ISurfaceAllocator> mAllocator;
  size_t mSize;
  ipc::SharedMemory::SharedMemoryType mType;
  ipc::Shmem* mShmem;
  bool mUnsafe;
  bool mSuccess;
};

static void ProxyAllocShmemNow(AllocShmemParams* aParams,
                               ReentrantMonitor* aBarrier,
                               bool* aDone)
{
  MOZ_ASSERT(aParams);
  MOZ_ASSERT(aDone);
  MOZ_ASSERT(aBarrier);

  auto shmAllocator = aParams->mAllocator->AsShmemAllocator();
  if (aParams->mUnsafe) {
    aParams->mSuccess = shmAllocator->AllocUnsafeShmem(aParams->mSize,
                                                       aParams->mType,
                                                       aParams->mShmem);
  } else {
    aParams->mSuccess = shmAllocator->AllocShmem(aParams->mSize,
                                                 aParams->mType,
                                                 aParams->mShmem);
  }

  ReentrantMonitorAutoEnter autoMon(*aBarrier);
  *aDone = true;
  aBarrier->NotifyAll();
}

bool
ImageBridgeChild::DispatchAllocShmemInternal(size_t aSize,
                                             SharedMemory::SharedMemoryType aType,
                                             ipc::Shmem* aShmem,
                                             bool aUnsafe)
{
  ReentrantMonitor barrier("AllocatorProxy alloc");
  ReentrantMonitorAutoEnter autoMon(barrier);

  AllocShmemParams params = {
    this, aSize, aType, aShmem, aUnsafe, true
  };
  bool done = false;

  GetMessageLoop()->PostTask(NewRunnableFunction(&ProxyAllocShmemNow,
                                                 &params,
                                                 &barrier,
                                                 &done));
  while (!done) {
    barrier.Wait();
  }
  return params.mSuccess;
}

static void ProxyDeallocShmemNow(ISurfaceAllocator* aAllocator,
                                 ipc::Shmem* aShmem,
                                 ReentrantMonitor* aBarrier,
                                 bool* aDone)
{
  MOZ_ASSERT(aShmem);
  MOZ_ASSERT(aDone);
  MOZ_ASSERT(aBarrier);

  aAllocator->AsShmemAllocator()->DeallocShmem(*aShmem);

  ReentrantMonitorAutoEnter autoMon(*aBarrier);
  *aDone = true;
  aBarrier->NotifyAll();
}

void
ImageBridgeChild::DeallocShmem(ipc::Shmem& aShmem)
{
  if (InImageBridgeChildThread()) {
    PImageBridgeChild::DeallocShmem(aShmem);
  } else {
    ReentrantMonitor barrier("AllocatorProxy Dealloc");
    ReentrantMonitorAutoEnter autoMon(barrier);

    bool done = false;
    GetMessageLoop()->PostTask(NewRunnableFunction(&ProxyDeallocShmemNow,
                                                   this,
                                                   &aShmem,
                                                   &barrier,
                                                   &done));
    while (!done) {
      barrier.Wait();
    }
  }
}

PTextureChild*
ImageBridgeChild::AllocPTextureChild(const SurfaceDescriptor&,
                                     const LayersBackend&,
                                     const TextureFlags&,
                                     const uint64_t& aSerial)
{
  MOZ_ASSERT(!mShuttingDown);
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
  MOZ_ASSERT(!mShuttingDown);
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
  ImageContainer::DeallocActor(actor);
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
    ImageContainer::NotifyComposite(n);
  }
  return true;
}

PTextureChild*
ImageBridgeChild::CreateTexture(const SurfaceDescriptor& aSharedData,
                                LayersBackend aLayersBackend,
                                TextureFlags aFlags,
                                uint64_t aSerial)
{
  MOZ_ASSERT(!mShuttingDown);
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
  MOZ_ASSERT(!mShuttingDown);
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
  MOZ_ASSERT(!mShuttingDown);
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

} // namespace layers
} // namespace mozilla
