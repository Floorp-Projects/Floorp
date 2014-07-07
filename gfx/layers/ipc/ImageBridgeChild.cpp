/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImageBridgeChild.h"
#include <vector>                       // for vector
#include "CompositorParent.h"           // for CompositorParent
#include "ImageBridgeParent.h"          // for ImageBridgeParent
#include "ImageContainer.h"             // for ImageContainer
#include "Layers.h"                     // for Layer, etc
#include "ShadowLayers.h"               // for ShadowLayerForwarder
#include "base/message_loop.h"          // for MessageLoop
#include "base/platform_thread.h"       // for PlatformThread
#include "base/process.h"               // for ProcessHandle
#include "base/process_util.h"          // for OpenProcessHandle
#include "base/task.h"                  // for NewRunnableFunction, etc
#include "base/thread.h"                // for Thread
#include "base/tracked.h"               // for FROM_HERE
#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
#include "mozilla/Monitor.h"            // for Monitor, MonitorAutoLock
#include "mozilla/ReentrantMonitor.h"   // for ReentrantMonitor, etc
#include "mozilla/ipc/MessageChannel.h" // for MessageChannel, etc
#include "mozilla/ipc/Transport.h"      // for Transport
#include "mozilla/gfx/Point.h"          // for IntSize
#include "mozilla/layers/CompositableClient.h"  // for CompositableChild, etc
#include "mozilla/layers/ISurfaceAllocator.h"  // for ISurfaceAllocator
#include "mozilla/layers/ImageClient.h"  // for ImageClient
#include "mozilla/layers/LayersMessages.h"  // for CompositableOperation
#include "mozilla/layers/PCompositableChild.h"  // for PCompositableChild
#include "mozilla/layers/TextureClient.h"  // for TextureClient
#include "mozilla/mozalloc.h"           // for operator new, etc
#include "nsAutoPtr.h"                  // for nsRefPtr
#include "nsISupportsImpl.h"            // for ImageContainer::AddRef, etc
#include "nsTArray.h"                   // for nsAutoTArray, nsTArray, etc
#include "nsTArrayForwardDeclare.h"     // for AutoInfallibleTArray
#include "nsThreadUtils.h"              // for NS_IsMainThread
#include "nsXULAppAPI.h"                // for XRE_GetIOMessageLoop
#include "mozilla/StaticPtr.h"          // for StaticRefPtr
#include "mozilla/layers/TextureClient.h"

struct nsIntRect;

namespace mozilla {
namespace ipc {
class Shmem;
}

namespace layers {

using base::Thread;
using base::ProcessHandle;
using namespace mozilla::ipc;
using namespace mozilla::gfx;

typedef std::vector<CompositableOperation> OpVector;

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
  }
  bool IsEmpty() const
  {
    return mOperations.empty();
  }
  void AddNoSwapEdit(const CompositableOperation& op)
  {
    NS_ABORT_IF_FALSE(!Finished(), "forgot BeginTransaction?");
    mOperations.push_back(op);
  }
  void AddEdit(const CompositableOperation& op)
  {
    AddNoSwapEdit(op);
    mSwapRequired = true;
  }

  OpVector mOperations;
  bool mSwapRequired;
  bool mFinished;
};

struct AutoEndTransaction {
  AutoEndTransaction(CompositableTransaction* aTxn) : mTxn(aTxn) {}
  ~AutoEndTransaction() { mTxn->End(); }
  CompositableTransaction* mTxn;
};

void
ImageBridgeChild::UseTexture(CompositableClient* aCompositable,
                             TextureClient* aTexture)
{
  MOZ_ASSERT(aCompositable);
  MOZ_ASSERT(aTexture);
  MOZ_ASSERT(aCompositable->GetIPDLActor());
  MOZ_ASSERT(aTexture->GetIPDLActor());
  mTxn->AddNoSwapEdit(OpUseTexture(nullptr, aCompositable->GetIPDLActor(),
                                   nullptr, aTexture->GetIPDLActor()));
}

void
ImageBridgeChild::UseComponentAlphaTextures(CompositableClient* aCompositable,
                                            TextureClient* aTextureOnBlack,
                                            TextureClient* aTextureOnWhite)
{
  MOZ_ASSERT(aCompositable);
  MOZ_ASSERT(aTextureOnWhite);
  MOZ_ASSERT(aTextureOnBlack);
  MOZ_ASSERT(aCompositable->GetIPDLActor());
  MOZ_ASSERT(aTextureOnWhite->GetIPDLActor());
  MOZ_ASSERT(aTextureOnBlack->GetIPDLActor());
  MOZ_ASSERT(aTextureOnBlack->GetSize() == aTextureOnWhite->GetSize());
  mTxn->AddNoSwapEdit(OpUseComponentAlphaTextures(nullptr, aCompositable->GetIPDLActor(),
                                                  nullptr, aTextureOnBlack->GetIPDLActor(),
                                                  nullptr, aTextureOnWhite->GetIPDLActor()));
}

void
ImageBridgeChild::UpdatedTexture(CompositableClient* aCompositable,
                                 TextureClient* aTexture,
                                 nsIntRegion* aRegion)
{
  MOZ_ASSERT(aCompositable);
  MOZ_ASSERT(aTexture);
  MOZ_ASSERT(aCompositable->GetIPDLActor());
  MOZ_ASSERT(aTexture->GetIPDLActor());
  MaybeRegion region = aRegion ? MaybeRegion(*aRegion)
                               : MaybeRegion(null_t());
  mTxn->AddNoSwapEdit(OpUpdateTexture(nullptr, aCompositable->GetIPDLActor(),
                                      nullptr, aTexture->GetIPDLActor(),
                                      region));
}

void
ImageBridgeChild::SendFenceHandle(AsyncTransactionTracker* aTracker,
                                  PTextureChild* aTexture,
                                  const FenceHandle& aFence)
{
  HoldUntilComplete(aTracker);
  InfallibleTArray<AsyncChildMessageData> messages;
  messages.AppendElement(OpDeliverFenceFromChild(aTracker->GetId(),
                                                 nullptr, aTexture,
                                                 FenceHandleFromChild(aFence)));
  SendChildAsyncMessages(messages);
}

void
ImageBridgeChild::UpdatePictureRect(CompositableClient* aCompositable,
                                    const nsIntRect& aRect)
{
  MOZ_ASSERT(aCompositable);
  MOZ_ASSERT(aCompositable->GetIPDLActor());
  mTxn->AddNoSwapEdit(OpUpdatePictureRect(nullptr, aCompositable->GetIPDLActor(), aRect));
}

// Singleton
static StaticRefPtr<ImageBridgeChild> sImageBridgeChildSingleton;
static StaticRefPtr<ImageBridgeParent> sImageBridgeParentSingleton;
static Thread *sImageBridgeChildThread = nullptr;

void ReleaseImageBridgeParentSingleton() {
  sImageBridgeParentSingleton = nullptr;
}


// dispatched function
static void ImageBridgeShutdownStep1(ReentrantMonitor *aBarrier, bool *aDone)
{
  ReentrantMonitorAutoEnter autoMon(*aBarrier);

  NS_ABORT_IF_FALSE(InImageBridgeChildThread(),
                    "Should be in ImageBridgeChild thread.");
  if (sImageBridgeChildSingleton) {
    // Force all managed protocols to shut themselves down cleanly
    InfallibleTArray<PCompositableChild*> compositables;
    sImageBridgeChildSingleton->ManagedPCompositableChild(compositables);
    for (int i = compositables.Length() - 1; i >= 0; --i) {
      CompositableClient::FromIPDLActor(compositables[i])->Destroy();
    }
    InfallibleTArray<PTextureChild*> textures;
    sImageBridgeChildSingleton->ManagedPTextureChild(textures);
    for (int i = textures.Length() - 1; i >= 0; --i) {
      TextureClient::AsTextureClient(textures[i])->ForceRemove();
    }
    sImageBridgeChildSingleton->SendWillStop();
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

  NS_ABORT_IF_FALSE(InImageBridgeChildThread(),
                    "Should be in ImageBridgeChild thread.");

  sImageBridgeChildSingleton->SendStop();

  *aDone = true;
  aBarrier->NotifyAll();
}

// dispatched function
static void CreateImageClientSync(RefPtr<ImageClient>* result,
                                  ReentrantMonitor* barrier,
                                  CompositableType aType,
                                  bool *aDone)
{
  ReentrantMonitorAutoEnter autoMon(*barrier);
  *result = sImageBridgeChildSingleton->CreateImageClientNow(aType);
  *aDone = true;
  barrier->NotifyAll();
}

static void ConnectImageBridge(ImageBridgeChild * child, ImageBridgeParent * parent)
{
  MessageLoop *parentMsgLoop = parent->GetMessageLoop();
  ipc::MessageChannel *parentChannel = parent->GetIPCChannel();
  child->Open(parentChannel, parentMsgLoop, mozilla::ipc::ChildSide);
}

ImageBridgeChild::ImageBridgeChild()
  : mShuttingDown(false)
{
  MOZ_ASSERT(NS_IsMainThread());

  mTxn = new CompositableTransaction();
}
ImageBridgeChild::~ImageBridgeChild()
{
  MOZ_ASSERT(NS_IsMainThread());

  delete mTxn;
}

void
ImageBridgeChild::MarkShutDown()
{
  MOZ_ASSERT(!mShuttingDown);
  mShuttingDown = true;
}

void
ImageBridgeChild::Connect(CompositableClient* aCompositable)
{
  MOZ_ASSERT(aCompositable);
  MOZ_ASSERT(!mShuttingDown);
  uint64_t id = 0;
  PCompositableChild* child =
    SendPCompositableConstructor(aCompositable->GetTextureInfo(), &id);
  MOZ_ASSERT(child);
  aCompositable->InitIPDLActor(child, id);
}

PCompositableChild*
ImageBridgeChild::AllocPCompositableChild(const TextureInfo& aInfo, uint64_t* aID)
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

void ImageBridgeChild::StartUp()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on the main Thread!");
  ImageBridgeChild::StartUpOnThread(new Thread("ImageBridgeChild"));
}

#ifdef MOZ_NUWA_PROCESS
#include "ipc/Nuwa.h"
#endif

static void
ConnectImageBridgeInChildProcess(Transport* aTransport,
                                 ProcessHandle aOtherProcess)
{
  // Bind the IPC channel to the image bridge thread.
  sImageBridgeChildSingleton->Open(aTransport, aOtherProcess,
                                   XRE_GetIOMessageLoop(),
                                   ipc::ChildSide);
#ifdef MOZ_NUWA_PROCESS
  if (IsNuwaProcess()) {
    sImageBridgeChildThread
      ->message_loop()->PostTask(FROM_HERE,
                                 NewRunnableFunction(NuwaMarkCurrentThread,
                                                     (void (*)(void *))nullptr,
                                                     (void *)nullptr));
  }
#endif
}

static void ReleaseImageClientNow(ImageClient* aClient)
{
  MOZ_ASSERT(InImageBridgeChildThread());
  aClient->Release();
}

// static
void ImageBridgeChild::DispatchReleaseImageClient(ImageClient* aClient)
{
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
    FROM_HERE,
    NewRunnableFunction(&ReleaseImageClientNow, aClient));
}

static void ReleaseTextureClientNow(TextureClient* aClient)
{
  MOZ_ASSERT(InImageBridgeChildThread());
  aClient->Release();
}

// static
void ImageBridgeChild::DispatchReleaseTextureClient(TextureClient* aClient)
{
  if (!IsCreated()) {
    // TextureClient::Release should normally happen in the ImageBridgeChild
    // thread because it usually generate some IPDL messages.
    // However, if we take this branch it means that the ImageBridgeChild
    // has already shut down, along with the TextureChild, which means no
    // message will be sent and it is safe to run this code from any thread.
    MOZ_ASSERT(aClient->GetIPDLActor() == nullptr);
    aClient->Release();
    return;
  }

  sImageBridgeChildSingleton->GetMessageLoop()->PostTask(
    FROM_HERE,
    NewRunnableFunction(&ReleaseTextureClientNow, aClient));
}

static void UpdateImageClientNow(ImageClient* aClient, ImageContainer* aContainer)
{
  MOZ_ASSERT(aClient);
  MOZ_ASSERT(aContainer);
  sImageBridgeChildSingleton->BeginTransaction();
  aClient->UpdateImage(aContainer, Layer::CONTENT_OPAQUE);
  aClient->OnTransaction();
  sImageBridgeChildSingleton->EndTransaction();
}

//static
void ImageBridgeChild::DispatchImageClientUpdate(ImageClient* aClient,
                                                 ImageContainer* aContainer)
{
  if (!IsCreated()) {
    return;
  }

  if (InImageBridgeChildThread()) {
    UpdateImageClientNow(aClient, aContainer);
    return;
  }
  sImageBridgeChildSingleton->GetMessageLoop()->PostTask(
    FROM_HERE,
    NewRunnableFunction<
      void (*)(ImageClient*, ImageContainer*),
      ImageClient*,
      nsRefPtr<ImageContainer> >(&UpdateImageClientNow, aClient, aContainer));
}

static void FlushAllImagesSync(ImageClient* aClient, ImageContainer* aContainer, bool aExceptFront, AsyncTransactionTracker* aStatus)
{
  MOZ_ASSERT(aClient);
  sImageBridgeChildSingleton->BeginTransaction();
  if (aContainer && !aExceptFront) {
    aContainer->ClearCurrentImage();
  }
  aClient->FlushAllImages(aExceptFront, aStatus);
  aClient->OnTransaction();
  sImageBridgeChildSingleton->EndTransaction();
}

//static
void ImageBridgeChild::FlushAllImages(ImageClient* aClient, ImageContainer* aContainer, bool aExceptFront)
{
  if (!IsCreated()) {
    return;
  }
  MOZ_ASSERT(aClient);
  MOZ_ASSERT(!sImageBridgeChildSingleton->mShuttingDown);
  MOZ_ASSERT(!InImageBridgeChildThread());
  if (InImageBridgeChildThread()) {
    NS_ERROR("ImageBridgeChild::FlushAllImages() is called on ImageBridge thread.");
     return;
   }

  RefPtr<AsyncTransactionTracker> status = aClient->PrepareFlushAllImages();

  sImageBridgeChildSingleton->GetMessageLoop()->PostTask(
    FROM_HERE,
    NewRunnableFunction(&FlushAllImagesSync, aClient, aContainer, aExceptFront, status));

  status->WaitComplete();
}

void
ImageBridgeChild::BeginTransaction()
{
  MOZ_ASSERT(!mShuttingDown);
  MOZ_ASSERT(mTxn->Finished(), "uncommitted txn?");
  mTxn->Begin();
}

class MOZ_STACK_CLASS AutoRemoveTextures
{
public:
  AutoRemoveTextures(ImageBridgeChild* aImageBridge)
    : mImageBridge(aImageBridge) {}

  ~AutoRemoveTextures()
  {
    mImageBridge->RemoveTexturesIfNecessary();
  }
private:
  ImageBridgeChild* mImageBridge;
};

void
ImageBridgeChild::EndTransaction()
{
  MOZ_ASSERT(!mShuttingDown);
  MOZ_ASSERT(!mTxn->Finished(), "forgot BeginTransaction?");

  AutoEndTransaction _(mTxn);
  AutoRemoveTextures autoRemoveTextures(this);

  if (mTxn->IsEmpty()) {
    return;
  }

  AutoInfallibleTArray<CompositableOperation, 10> cset;
  cset.SetCapacity(mTxn->mOperations.size());
  if (!mTxn->mOperations.empty()) {
    cset.AppendElements(&mTxn->mOperations.front(), mTxn->mOperations.size());
  }

  if (!IsSameProcess()) {
    ShadowLayerForwarder::PlatformSyncBeforeUpdate();
  }

  AutoInfallibleTArray<EditReply, 10> replies;

  if (mTxn->mSwapRequired) {
    if (!SendUpdate(cset, &replies)) {
      NS_WARNING("could not send async texture transaction");
      return;
    }
  } else {
    // If we don't require a swap we can call SendUpdateNoSwap which
    // assumes that aReplies is empty (DEBUG assertion)
    if (!SendUpdateNoSwap(cset)) {
      NS_WARNING("could not send async texture transaction (no swap)");
      return;
    }
  }
  for (nsTArray<EditReply>::size_type i = 0; i < replies.Length(); ++i) {
    const EditReply& reply = replies[i];
    switch (reply.type()) {
    case EditReply::TOpTextureSwap: {
      const OpTextureSwap& ots = reply.get_OpTextureSwap();

      CompositableClient* compositable =
        CompositableClient::FromIPDLActor(ots.compositableChild());

      MOZ_ASSERT(compositable);

      compositable->SetDescriptorFromReply(ots.textureId(), ots.image());
      break;
    }
    case EditReply::TReturnReleaseFence: {
      const ReturnReleaseFence& rep = reply.get_ReturnReleaseFence();
      FenceHandle fence = rep.fence();
      PTextureChild* child = rep.textureChild();

      if (!fence.IsValid() || !child) {
        break;
      }
      RefPtr<TextureClient> texture = TextureClient::AsTextureClient(child);
      if (texture) {
        texture->SetReleaseFenceHandle(fence);
      }
      break;
    }
    default:
      NS_RUNTIMEABORT("not reached");
    }
  }
  SendPendingAsyncMessge();
}


PImageBridgeChild*
ImageBridgeChild::StartUpInChildProcess(Transport* aTransport,
                                        ProcessId aOtherProcess)
{
  MOZ_ASSERT(NS_IsMainThread());

  gfxPlatform::GetPlatform();

  ProcessHandle processHandle;
  if (!base::OpenProcessHandle(aOtherProcess, &processHandle)) {
    return nullptr;
  }

  sImageBridgeChildThread = new Thread("ImageBridgeChild");
  if (!sImageBridgeChildThread->Start()) {
    return nullptr;
  }

  sImageBridgeChildSingleton = new ImageBridgeChild();
  sImageBridgeChildSingleton->GetMessageLoop()->PostTask(
    FROM_HERE,
    NewRunnableFunction(ConnectImageBridgeInChildProcess,
                        aTransport, processHandle));

  return sImageBridgeChildSingleton;
}

void ImageBridgeChild::ShutDown()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (ImageBridgeChild::IsCreated()) {
    MOZ_ASSERT(!sImageBridgeChildSingleton->mShuttingDown);

    {
      ReentrantMonitor barrier("ImageBridge ShutdownStep1 lock");
      ReentrantMonitorAutoEnter autoMon(barrier);

      bool done = false;
      sImageBridgeChildSingleton->GetMessageLoop()->PostTask(FROM_HERE,
                      NewRunnableFunction(&ImageBridgeShutdownStep1, &barrier, &done));
      while (!done) {
        barrier.Wait();
      }
    }

    {
      ReentrantMonitor barrier("ImageBridge ShutdownStep2 lock");
      ReentrantMonitorAutoEnter autoMon(barrier);

      bool done = false;
      sImageBridgeChildSingleton->GetMessageLoop()->PostTask(FROM_HERE,
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

bool ImageBridgeChild::StartUpOnThread(Thread* aThread)
{
  NS_ABORT_IF_FALSE(aThread, "ImageBridge needs a thread.");
  if (sImageBridgeChildSingleton == nullptr) {
    sImageBridgeChildThread = aThread;
    if (!aThread->IsRunning()) {
      aThread->Start();
    }
    sImageBridgeChildSingleton = new ImageBridgeChild();
    sImageBridgeParentSingleton = new ImageBridgeParent(
      CompositorParent::CompositorLoop(), nullptr, base::GetProcId(base::GetCurrentProcessHandle()));
    sImageBridgeChildSingleton->ConnectAsync(sImageBridgeParentSingleton);
    return true;
  } else {
    return false;
  }
}

bool InImageBridgeChildThread()
{
  return ImageBridgeChild::IsCreated() &&
    sImageBridgeChildThread->thread_id() == PlatformThread::CurrentId();
}

MessageLoop * ImageBridgeChild::GetMessageLoop() const
{
  return sImageBridgeChildThread->message_loop();
}

void ImageBridgeChild::ConnectAsync(ImageBridgeParent* aParent)
{
  GetMessageLoop()->PostTask(FROM_HERE, NewRunnableFunction(&ConnectImageBridge,
                                                            this, aParent));
}

void
ImageBridgeChild::IdentifyCompositorTextureHost(const TextureFactoryIdentifier& aIdentifier)
{
  if (sImageBridgeChildSingleton) {
    sImageBridgeChildSingleton->IdentifyTextureHost(aIdentifier);
  }
}

TemporaryRef<ImageClient>
ImageBridgeChild::CreateImageClient(CompositableType aType)
{
  if (InImageBridgeChildThread()) {
    return CreateImageClientNow(aType);
  }
  ReentrantMonitor barrier("CreateImageClient Lock");
  ReentrantMonitorAutoEnter autoMon(barrier);
  bool done = false;

  RefPtr<ImageClient> result = nullptr;
  GetMessageLoop()->PostTask(FROM_HERE, NewRunnableFunction(&CreateImageClientSync,
                                                            &result, &barrier, aType, &done));
  // should stop the thread until the ImageClient has been created on
  // the other thread
  while (!done) {
    barrier.Wait();
  }
  return result.forget();
}

TemporaryRef<ImageClient>
ImageBridgeChild::CreateImageClientNow(CompositableType aType)
{
  MOZ_ASSERT(!sImageBridgeChildSingleton->mShuttingDown);
  RefPtr<ImageClient> client
    = ImageClient::CreateImageClient(aType, this, TextureFlags::NO_FLAGS);
  MOZ_ASSERT(client, "failed to create ImageClient");
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
    return PImageBridgeChild::AllocShmem(aSize, aType, aShmem);
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

  if (aParams->mUnsafe) {
    aParams->mSuccess = aParams->mAllocator->AllocUnsafeShmem(aParams->mSize,
                                                              aParams->mType,
                                                              aParams->mShmem);
  } else {
    aParams->mSuccess = aParams->mAllocator->AllocShmem(aParams->mSize,
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

  GetMessageLoop()->PostTask(FROM_HERE,
                             NewRunnableFunction(&ProxyAllocShmemNow,
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

  aAllocator->DeallocShmem(*aShmem);

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
    GetMessageLoop()->PostTask(FROM_HERE,
                               NewRunnableFunction(&ProxyDeallocShmemNow,
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
                                     const TextureFlags&)
{
  MOZ_ASSERT(!mShuttingDown);
  return TextureClient::CreateIPDLActor();
}

bool
ImageBridgeChild::DeallocPTextureChild(PTextureChild* actor)
{
  return TextureClient::DestroyIPDLActor(actor);
}

bool
ImageBridgeChild::RecvParentAsyncMessages(const InfallibleTArray<AsyncParentMessageData>& aMessages)
{
  for (AsyncParentMessageArray::index_type i = 0; i < aMessages.Length(); ++i) {
    const AsyncParentMessageData& message = aMessages[i];

    switch (message.type()) {
      case AsyncParentMessageData::TOpDeliverFence: {
        const OpDeliverFence& op = message.get_OpDeliverFence();
        FenceHandle fence = op.fence();
        PTextureChild* child = op.textureChild();

        RefPtr<TextureClient> texture = TextureClient::AsTextureClient(child);
        if (texture) {
          texture->SetReleaseFenceHandle(fence);
        }
        HoldTransactionsToRespond(op.transactionId());
        break;
      }
      case AsyncParentMessageData::TOpDeliverFenceToTracker: {
        const OpDeliverFenceToTracker& op = message.get_OpDeliverFenceToTracker();
        FenceHandle fence = op.fence();

        AsyncTransactionTrackersHolder::SetReleaseFenceHandle(fence,
                                                              op.destHolderId(),
                                                              op.destTransactionId());
        // Send back a response.
        InfallibleTArray<AsyncChildMessageData> replies;
        replies.AppendElement(OpReplyDeliverFence(op.transactionId()));
        SendChildAsyncMessages(replies);
        break;
      }
      case AsyncParentMessageData::TOpReplyDeliverFence: {
        const OpReplyDeliverFence& op = message.get_OpReplyDeliverFence();
        TransactionCompleteted(op.transactionId());
        break;
      }
      case AsyncParentMessageData::TOpReplyRemoveTexture: {
        const OpReplyRemoveTexture& op = message.get_OpReplyRemoveTexture();

        AsyncTransactionTrackersHolder::TransactionCompleteted(op.holderId(),
                                                               op.transactionId());
        break;
      }
      default:
        NS_ERROR("unknown AsyncParentMessageData type");
        return false;
    }
  }
  return true;
}

PTextureChild*
ImageBridgeChild::CreateTexture(const SurfaceDescriptor& aSharedData,
                                TextureFlags aFlags)
{
  MOZ_ASSERT(!mShuttingDown);
  return SendPTextureConstructor(aSharedData, aFlags);
}

void
ImageBridgeChild::RemoveTextureFromCompositable(CompositableClient* aCompositable,
                                                TextureClient* aTexture)
{
  MOZ_ASSERT(!mShuttingDown);
  if (aTexture->GetFlags() & TextureFlags::DEALLOCATE_CLIENT) {
    mTxn->AddEdit(OpRemoveTexture(nullptr, aCompositable->GetIPDLActor(),
                                  nullptr, aTexture->GetIPDLActor()));
  } else {
    mTxn->AddNoSwapEdit(OpRemoveTexture(nullptr, aCompositable->GetIPDLActor(),
                                        nullptr, aTexture->GetIPDLActor()));
  }
  // Hold texture until transaction complete.
  HoldUntilTransaction(aTexture);
}

void
ImageBridgeChild::RemoveTextureFromCompositableAsync(AsyncTransactionTracker* aAsyncTransactionTracker,
                                                     CompositableClient* aCompositable,
                                                     TextureClient* aTexture)
{
  mTxn->AddNoSwapEdit(OpRemoveTextureAsync(CompositableClient::GetTrackersHolderId(aCompositable->GetIPDLActor()),
                                           aAsyncTransactionTracker->GetId(),
                                           nullptr, aCompositable->GetIPDLActor(),
                                           nullptr, aTexture->GetIPDLActor()));
  // Hold AsyncTransactionTracker until receving reply
  CompositableClient::HoldUntilComplete(aCompositable->GetIPDLActor(),
                                        aAsyncTransactionTracker);
}

static void RemoveTextureSync(TextureClient* aTexture, ReentrantMonitor* aBarrier, bool* aDone)
{
  aTexture->ForceRemove();

  ReentrantMonitorAutoEnter autoMon(*aBarrier);
  *aDone = true;
  aBarrier->NotifyAll();
}

void ImageBridgeChild::RemoveTexture(TextureClient* aTexture)
{
  if (InImageBridgeChildThread()) {
    MOZ_ASSERT(!mShuttingDown);
    aTexture->ForceRemove();
    return;
  }

  ReentrantMonitor barrier("RemoveTexture Lock");
  ReentrantMonitorAutoEnter autoMon(barrier);
  bool done = false;

  sImageBridgeChildSingleton->GetMessageLoop()->PostTask(
    FROM_HERE,
    NewRunnableFunction(&RemoveTextureSync, aTexture, &barrier, &done));

  // should stop the thread until the ImageClient has been created on
  // the other thread
  while (!done) {
    barrier.Wait();
  }
}

bool ImageBridgeChild::IsSameProcess() const
{
  return OtherProcess() == ipc::kInvalidProcessHandle;
}

void ImageBridgeChild::SendPendingAsyncMessge()
{
  if (!IsCreated() ||
      mTransactionsToRespond.empty()) {
    return;
  }
  // Send OpReplyDeliverFence messages
  InfallibleTArray<AsyncChildMessageData> replies;
  replies.SetCapacity(mTransactionsToRespond.size());
  for (size_t i = 0; i < mTransactionsToRespond.size(); i++) {
    replies.AppendElement(OpReplyDeliverFence(mTransactionsToRespond[i]));
  }
  mTransactionsToRespond.clear();
  SendChildAsyncMessages(replies);
}

} // layers
} // mozilla
