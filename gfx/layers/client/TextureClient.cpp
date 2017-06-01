/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/TextureClient.h"
#include <stdint.h>                     // for uint8_t, uint32_t, etc
#include "Layers.h"                     // for Layer, etc
#include "gfx2DGlue.h"
#include "gfxPlatform.h"                // for gfxPlatform
#include "mozilla/Atomics.h"
#include "mozilla/ipc/SharedMemory.h"   // for SharedMemory, etc
#include "mozilla/layers/CompositableForwarder.h"
#include "mozilla/layers/ISurfaceAllocator.h"
#include "mozilla/layers/ImageBridgeChild.h"
#include "mozilla/layers/ImageDataSerializer.h"
#include "mozilla/layers/TextureClientRecycleAllocator.h"
#include "mozilla/Mutex.h"
#include "nsDebug.h"                    // for NS_ASSERTION, NS_WARNING, etc
#include "nsISupportsImpl.h"            // for MOZ_COUNT_CTOR, etc
#include "ImageContainer.h"             // for PlanarYCbCrData, etc
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Logging.h"        // for gfxDebug
#include "mozilla/layers/TextureClientOGL.h"
#include "mozilla/layers/PTextureChild.h"
#include "mozilla/gfx/DataSurfaceHelpers.h" // for CreateDataSourceSurfaceByCloning
#include "nsPrintfCString.h"            // for nsPrintfCString
#include "LayersLogging.h"              // for AppendToString
#include "gfxUtils.h"                   // for gfxUtils::GetAsLZ4Base64Str
#include "IPDLActor.h"
#include "BufferTexture.h"
#include "gfxPrefs.h"
#include "mozilla/layers/ShadowLayers.h"
#include "mozilla/ipc/CrossProcessSemaphore.h"

#ifdef XP_WIN
#include "mozilla/gfx/DeviceManagerDx.h"
#include "mozilla/layers/TextureD3D11.h"
#include "mozilla/layers/TextureDIB.h"
#include "gfxWindowsPlatform.h"
#include "gfx2DGlue.h"
#endif
#ifdef MOZ_X11
#include "mozilla/layers/TextureClientX11.h"
#ifdef GL_PROVIDER_GLX
#include "GLXLibrary.h"
#endif
#endif

#ifdef XP_MACOSX
#include "mozilla/layers/MacIOSurfaceTextureClientOGL.h"
#endif

#if 0
#define RECYCLE_LOG(...) printf_stderr(__VA_ARGS__)
#else
#define RECYCLE_LOG(...) do { } while (0)
#endif

namespace mozilla {
namespace layers {

using namespace mozilla::ipc;
using namespace mozilla::gl;
using namespace mozilla::gfx;

struct TextureDeallocParams
{
  TextureData* data;
  RefPtr<TextureChild> actor;
  RefPtr<LayersIPCChannel> allocator;
  bool clientDeallocation;
  bool syncDeallocation;
  bool workAroundSharedSurfaceOwnershipIssue;
};

void DeallocateTextureClient(TextureDeallocParams params);

/**
 * TextureChild is the content-side incarnation of the PTexture IPDL actor.
 *
 * TextureChild is used to synchronize a texture client and its corresponding
 * TextureHost if needed (a TextureClient that is not shared with the compositor
 * does not have a TextureChild)
 *
 * During the deallocation phase, a TextureChild may hold its recently destroyed
 * TextureClient's data until the compositor side confirmed that it is safe to
 * deallocte or recycle the it.
 */
class TextureChild final : PTextureChild
{
  ~TextureChild()
  {
    // We should have deallocated mTextureData in ActorDestroy
    MOZ_ASSERT(!mTextureData);
    MOZ_ASSERT_IF(!mOwnerCalledDestroy, !mTextureClient);
  }
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(TextureChild)

  TextureChild()
  : mCompositableForwarder(nullptr)
  , mTextureForwarder(nullptr)
  , mTextureClient(nullptr)
  , mTextureData(nullptr)
  , mDestroyed(false)
  , mMainThreadOnly(false)
  , mIPCOpen(false)
  , mOwnsTextureData(false)
  , mOwnerCalledDestroy(false)
  {}

  mozilla::ipc::IPCResult Recv__delete__() override { return IPC_OK(); }

  LayersIPCChannel* GetAllocator() { return mTextureForwarder; }

  void ActorDestroy(ActorDestroyReason why) override;

  bool IPCOpen() const { return mIPCOpen; }

  void Lock() const { if (mCompositableForwarder && mCompositableForwarder->GetTextureForwarder()->UsesImageBridge()) { mLock.Enter(); } }

  void Unlock() const { if (mCompositableForwarder && mCompositableForwarder->GetTextureForwarder()->UsesImageBridge()) { mLock.Leave(); } }

private:

  // AddIPDLReference and ReleaseIPDLReference are only to be called by CreateIPDLActor
  // and DestroyIPDLActor, respectively. We intentionally make them private to prevent misuse.
  // The purpose of these methods is to be aware of when the IPC system around this
  // actor goes down: mIPCOpen is then set to false.
  void AddIPDLReference() {
    MOZ_ASSERT(mIPCOpen == false);
    mIPCOpen = true;
    AddRef();
  }
  void ReleaseIPDLReference() {
    MOZ_ASSERT(mIPCOpen == false);
    Release();
  }

  /// The normal way to destroy the actor.
  ///
  /// This will asynchronously send a Destroy message to the parent actor, whom
  /// will send the delete message.
  void Destroy(const TextureDeallocParams& aParams);

  // This lock is used order to prevent several threads to access the
  // TextureClient's data concurrently. In particular, it prevents shutdown
  // code to destroy a texture while another thread is reading or writing into
  // it.
  // In most places, the lock is held in short and bounded scopes in which we
  // don't block on any other resource. There are few exceptions to this, which
  // are discussed below.
  //
  // The locking pattern of TextureClient may in some case upset deadlock detection
  // tools such as TSan.
  // Typically our tile rendering code will lock all of its tiles, render into them
  // and unlock them all right after that, which looks something like:
  //
  // Lock tile A
  // Lock tile B
  // Lock tile C
  // Apply drawing commands to tiles A, B and C 
  // Unlock tile A
  // Unlock tile B
  // Unlock tile C
  //
  // And later, we may end up rendering a tile buffer that has the same tiles,
  // in a different order, for example:
  //
  // Lock tile B
  // Lock tile A
  // Lock tile D
  // Apply drawing commands to tiles A, B and D
  // Unlock tile B
  // Unlock tile A
  // Unlock tile D
  //
  // This is because textures being expensive to create, we recycle them as much
  // as possible and they may reappear in the tile buffer in a different order.
  //
  // Unfortunately this is not very friendly to TSan's analysis, which will see
  // that B was once locked while A was locked, and then A locked while B was
  // locked. TSan identifies this as a potential dead-lock which would be the
  // case if this kind of inconsistent and dependent locking order was happening
  // concurrently.
  // In the case of TextureClient, dependent locking only ever happens on the
  // thread that draws into the texture (let's call it the producer thread). Other
  // threads may call into a method that can lock the texture in a short and
  // bounded scope inside of which it is not allowed to do anything that could
  // cause the thread to block. A given texture can only have one producer thread.
  //
  // Another example of TSan-unfriendly locking pattern is when copying a texture
  // into another, which also never happens outside of the producer thread.
  // Copying A into B looks like this:
  //
  // Lock texture B
  // Lock texture A
  // Copy A into B
  // Unlock A
  // Unlock B
  //
  // In a given frame we may need to copy A into B and in another frame copy
  // B into A. For example A and B can be the Front and Back buffers, alternating
  // roles and the copy is needed to avoid the cost of re-drawing the valid
  // region.
  //
  // The important rule is that all of the dependent locking must occur only
  // in the texture's producer thread to avoid deadlocks.
  mutable gfx::CriticalSection mLock;

  RefPtr<CompositableForwarder> mCompositableForwarder;
  RefPtr<TextureForwarder> mTextureForwarder;

  TextureClient* mTextureClient;
  TextureData* mTextureData;
  Atomic<bool> mDestroyed;
  bool mMainThreadOnly;
  bool mIPCOpen;
  bool mOwnsTextureData;
  bool mOwnerCalledDestroy;

  friend class TextureClient;
  friend void DeallocateTextureClient(TextureDeallocParams params);
};


static void DestroyTextureData(TextureData* aTextureData, LayersIPCChannel* aAllocator,
                               bool aDeallocate, bool aMainThreadOnly)
{
  if (!aTextureData) {
    return;
  }

  if (aMainThreadOnly && !NS_IsMainThread()) {
    RefPtr<LayersIPCChannel> allocatorRef = aAllocator;
    NS_DispatchToMainThread(NS_NewRunnableFunction([aTextureData, allocatorRef, aDeallocate]() -> void {
      DestroyTextureData(aTextureData, allocatorRef, aDeallocate, true);
    }));
    return;
  }

  if (aDeallocate) {
    aTextureData->Deallocate(aAllocator);
  } else {
    aTextureData->Forget(aAllocator);
  }
  delete aTextureData;
}

void
TextureChild::ActorDestroy(ActorDestroyReason why)
{
  PROFILER_LABEL_FUNC(js::ProfileEntry::Category::GRAPHICS);
  MOZ_ASSERT(mIPCOpen);
  mIPCOpen = false;

  if (mTextureData) {
    DestroyTextureData(mTextureData, GetAllocator(), mOwnsTextureData, mMainThreadOnly);
    mTextureData = nullptr;
  }
}

void
TextureChild::Destroy(const TextureDeallocParams& aParams)
{
  MOZ_ASSERT(!mOwnerCalledDestroy);
  if (mOwnerCalledDestroy) {
    return;
  }

  mOwnerCalledDestroy = true;

  if (!IPCOpen()) {
    DestroyTextureData(
      aParams.data,
      aParams.allocator,
      aParams.clientDeallocation,
      mMainThreadOnly);
    return;
  }

  // DestroyTextureData will be called by TextureChild::ActorDestroy
  mTextureData = aParams.data;
  mOwnsTextureData = aParams.clientDeallocation;

  if (!mCompositableForwarder ||
      !mCompositableForwarder->DestroyInTransaction(this))
  {
    this->SendDestroy();
  }
}

/* static */ Atomic<uint64_t> TextureClient::sSerialCounter(0);

void DeallocateTextureClientSyncProxy(TextureDeallocParams params,
                                        ReentrantMonitor* aBarrier, bool* aDone)
{
  DeallocateTextureClient(params);
  ReentrantMonitorAutoEnter autoMon(*aBarrier);
  *aDone = true;
  aBarrier->NotifyAll();
}

/// The logic for synchronizing a TextureClient's deallocation goes here.
///
/// This funciton takes care of dispatching work to the right thread using
/// a synchronous proxy if needed, and handles client/host deallocation.
void
DeallocateTextureClient(TextureDeallocParams params)
{
  if (!params.actor && !params.data) {
    // Nothing to do
    return;
  }

  TextureChild* actor = params.actor;
  MessageLoop* ipdlMsgLoop = nullptr;

  if (params.allocator) {
    ipdlMsgLoop = params.allocator->GetMessageLoop();
    if (!ipdlMsgLoop) {
      // An allocator with no message loop means we are too late in the shutdown
      // sequence.
      gfxCriticalError() << "Texture deallocated too late during shutdown";
      return;
    }
  }

  // First make sure that the work is happening on the IPDL thread.
  if (ipdlMsgLoop && MessageLoop::current() != ipdlMsgLoop) {
    if (params.syncDeallocation) {
      bool done = false;
      ReentrantMonitor barrier("DeallocateTextureClient");
      ReentrantMonitorAutoEnter autoMon(barrier);
      ipdlMsgLoop->PostTask(NewRunnableFunction(DeallocateTextureClientSyncProxy,
                                                params, &barrier, &done));
      while (!done) {
        barrier.Wait();
      }
    } else {
      ipdlMsgLoop->PostTask(NewRunnableFunction(DeallocateTextureClient,
                                                params));
    }
    // The work has been forwarded to the IPDL thread, we are done.
    return;
  }

  // Below this line, we are either in the IPDL thread or ther is no IPDL
  // thread anymore.

  if (!ipdlMsgLoop) {
    // If we don't have a message loop we can't know for sure that we are in
    // the IPDL thread and use the LayersIPCChannel.
    // This should ideally not happen outside of gtest, but some shutdown raciness
    // could put us in this situation.
    params.allocator = nullptr;
  }

  if (!actor) {
    // We don't have an IPDL actor, probably because we destroyed the TextureClient
    // before sharing it with the compositor. It means the data cannot be owned by
    // the TextureHost since we never created the TextureHost...
    // ..except if the lovely mWorkaroundAnnoyingSharedSurfaceOwnershipIssues member
    // is set to true. In this case we are in a special situation where this
    // TextureClient is in wrapped into another TextureClient which assumes it owns
    // our data.
    bool shouldDeallocate = !params.workAroundSharedSurfaceOwnershipIssue;
    DestroyTextureData(params.data, params.allocator,
                       shouldDeallocate,
                       false);  // main-thread deallocation
    return;
  }

  actor->Destroy(params);
}

void TextureClient::Destroy()
{
  if (mActor && !mIsLocked) {
    mActor->Lock();
  }

  mBorrowedDrawTarget = nullptr;
  mReadLock = nullptr;

  RefPtr<TextureChild> actor = mActor;
  mActor = nullptr;

  if (actor && !actor->mDestroyed.compareExchange(false, true)) {
    actor->Unlock();
    actor = nullptr;
  }

  TextureData* data = mData;
  if (!mWorkaroundAnnoyingSharedSurfaceLifetimeIssues) {
    mData = nullptr;
  }

  if (data || actor) {
    TextureDeallocParams params;
    params.actor = actor;
    params.allocator = mAllocator;
    params.clientDeallocation = !!(mFlags & TextureFlags::DEALLOCATE_CLIENT);
    params.workAroundSharedSurfaceOwnershipIssue = mWorkaroundAnnoyingSharedSurfaceOwnershipIssues;
    if (mWorkaroundAnnoyingSharedSurfaceLifetimeIssues) {
      params.data = nullptr;
    } else {
      params.data = data;
    }
    // At the moment we always deallocate synchronously when deallocating on the
    // client side, but having asynchronous deallocate in some of the cases will
    // be a worthwhile optimization.
    params.syncDeallocation = !!(mFlags & TextureFlags::DEALLOCATE_CLIENT);

    // Release the lock before calling DeallocateTextureClient because the latter
    // may wait for the main thread which could create a dead-lock.

    if (actor) {
      actor->Unlock();
    }

    DeallocateTextureClient(params);
  }
}

void
TextureClient::LockActor() const
{
  if (mActor) {
    mActor->Lock();
  }
}

void
TextureClient::UnlockActor() const
{
  if (mActor) {
    mActor->Unlock();
  }
}

bool
TextureClient::IsReadLocked() const
{
  if (!mReadLock) {
    return false;
  }
  MOZ_ASSERT(mReadLock->AsNonBlockingLock(), "Can only check locked for non-blocking locks!");
  return mReadLock->AsNonBlockingLock()->GetReadCount() > 1;
}

bool
TextureClient::TryReadLock()
{
  if (!mReadLock || mIsReadLocked) {
    return true;
  }

  if (mReadLock->AsNonBlockingLock()) {
    if (IsReadLocked()) {
      return false;
    }
  }

  if (!mReadLock->TryReadLock(TimeDuration::FromMilliseconds(500))) {
    return false;
  }

  mIsReadLocked = true;
  return true;
}

void
TextureClient::ReadUnlock()
{
  if (!mIsReadLocked) {
    return;
  }
  MOZ_ASSERT(mReadLock);
  mReadLock->ReadUnlock();
  mIsReadLocked = false;
 }

bool
TextureClient::Lock(OpenMode aMode)
{
  MOZ_ASSERT(IsValid());
  MOZ_ASSERT(!mIsLocked);
  if (!IsValid()) {
    return false;
  }
  if (mIsLocked) {
    return mOpenMode == aMode;
  }

  if (aMode & OpenMode::OPEN_WRITE && !TryReadLock()) {
    NS_WARNING("Attempt to Lock a texture that is being read by the compositor!");
    return false;
  }

  LockActor();

  mIsLocked = mData->Lock(aMode);
  mOpenMode = aMode;

  auto format = GetFormat();
  if (mIsLocked && CanExposeDrawTarget() &&
      aMode == OpenMode::OPEN_READ_WRITE &&
      NS_IsMainThread() &&
      // the formats that we apparently expect, in the cairo backend. Any other
      // format will trigger an assertion in GfxFormatToCairoFormat.
      (format == SurfaceFormat::A8R8G8B8_UINT32 ||
      format == SurfaceFormat::X8R8G8B8_UINT32 ||
      format == SurfaceFormat::A8 ||
      format == SurfaceFormat::R5G6B5_UINT16)) {
    if (!BorrowDrawTarget()) {
      // Failed to get a DrawTarget, means we won't be able to write into the
      // texture, might as well fail now.
      Unlock();
      return false;
    }
  }

  if (!mIsLocked) {
    UnlockActor();
    ReadUnlock();
  }

  return mIsLocked;
}

void
TextureClient::Unlock()
{
  MOZ_ASSERT(IsValid());
  MOZ_ASSERT(mIsLocked);
  if (!IsValid() || !mIsLocked) {
    return;
  }

  if (mBorrowedDrawTarget) {
    if (mOpenMode & OpenMode::OPEN_WRITE) {
      mBorrowedDrawTarget->Flush();
      if (mReadbackSink && !mData->ReadBack(mReadbackSink)) {
        // Fallback implementation for reading back, because mData does not
        // have a backend-specific implementation and returned false.
        RefPtr<SourceSurface> snapshot = mBorrowedDrawTarget->Snapshot();
        RefPtr<DataSourceSurface> dataSurf = snapshot->GetDataSurface();
        mReadbackSink->ProcessReadback(dataSurf);
      }
    }

    mBorrowedDrawTarget->DetachAllSnapshots();
    // If this assertion is hit, it means something is holding a strong reference
    // to our DrawTarget externally, which is not allowed.
    MOZ_ASSERT(mBorrowedDrawTarget->refCount() <= mExpectedDtRefs);

    mBorrowedDrawTarget = nullptr;
  }

  if (mOpenMode & OpenMode::OPEN_WRITE) {
    mUpdated = true;
  }

  if (mData) {
    mData->Unlock();
  }
  mIsLocked = false;
  mOpenMode = OpenMode::OPEN_NONE;

  UnlockActor();
  ReadUnlock();
}

void
TextureClient::EnableReadLock()
{
  if (!mReadLock) {
    mReadLock = NonBlockingTextureReadLock::Create(mAllocator);
  }
}

bool
TextureClient::SerializeReadLock(ReadLockDescriptor& aDescriptor)
{
  if (mReadLock && mUpdated) {
    // Take a read lock on behalf of the TextureHost. The latter will unlock
    // after the shared data is available again for drawing.
    mReadLock->ReadLock();
    mUpdated = false;
    if (mReadLock->Serialize(aDescriptor, GetAllocator()->GetParentPid())) {
      return true;
    }
  }

  aDescriptor = null_t();
  return false;
}

TextureClient::~TextureClient()
{
  mReadLock = nullptr;
  Destroy();
}

void
TextureClient::UpdateFromSurface(gfx::SourceSurface* aSurface)
{
  MOZ_ASSERT(IsValid());
  MOZ_ASSERT(mIsLocked);
  MOZ_ASSERT(aSurface);
  // If you run into this assertion, make sure the texture was locked write-only
  // rather than read-write.
  MOZ_ASSERT(!mBorrowedDrawTarget);

  // XXX - It would be better to first try the DrawTarget approach and fallback
  // to the backend-specific implementation because the latter will usually do
  // an expensive read-back + cpu-side copy if the texture is on the gpu.
  // There is a bug with the DrawTarget approach, though specific to reading back
  // from WebGL (where R and B channel end up inverted) to figure out first.
  if (mData->UpdateFromSurface(aSurface)) {
    return;
  }
  if (CanExposeDrawTarget() && NS_IsMainThread()) {
    RefPtr<DrawTarget> dt = BorrowDrawTarget();

    MOZ_ASSERT(dt);
    if (dt) {
      dt->CopySurface(aSurface,
                      gfx::IntRect(gfx::IntPoint(0, 0), aSurface->GetSize()),
                      gfx::IntPoint(0, 0));
      return;
    }
  }
  NS_WARNING("TextureClient::UpdateFromSurface failed");
}


already_AddRefed<TextureClient>
TextureClient::CreateSimilar(LayersBackend aLayersBackend, TextureFlags aFlags, TextureAllocationFlags aAllocFlags) const
{
  MOZ_ASSERT(IsValid());

  MOZ_ASSERT(!mIsLocked);
  if (mIsLocked) {
    return nullptr;
  }

  LockActor();
  TextureData* data = mData->CreateSimilar(mAllocator, aLayersBackend, aFlags, aAllocFlags);
  UnlockActor();

  if (!data) {
    return nullptr;
  }

  return MakeAndAddRef<TextureClient>(data, aFlags, mAllocator);
}

gfx::DrawTarget*
TextureClient::BorrowDrawTarget()
{
  MOZ_ASSERT(IsValid());
  MOZ_ASSERT(mIsLocked);
  // TODO- We can't really assert that at the moment because there is code that Borrows
  // the DrawTarget, just to get a snapshot, which is legit in term of OpenMode
  // but we should have a way to get a SourceSurface directly instead.
  //MOZ_ASSERT(mOpenMode & OpenMode::OPEN_WRITE);

  if (!IsValid() || !mIsLocked) {
    return nullptr;
  }

  if (!NS_IsMainThread()) {
    return nullptr;
  }

  if (!mBorrowedDrawTarget) {
    mBorrowedDrawTarget = mData->BorrowDrawTarget();
#ifdef DEBUG
    mExpectedDtRefs = mBorrowedDrawTarget ? mBorrowedDrawTarget->refCount() : 0;
#endif
  }

  return mBorrowedDrawTarget;
}

bool
TextureClient::BorrowMappedData(MappedTextureData& aMap)
{
  MOZ_ASSERT(IsValid());

  // TODO - SharedRGBImage just accesses the buffer without properly locking
  // the texture. It's bad.
  //MOZ_ASSERT(mIsLocked);
  //if (!mIsLocked) {
  //  return nullptr;
  //}

  return mData ? mData->BorrowMappedData(aMap) : false;
}

bool
TextureClient::BorrowMappedYCbCrData(MappedYCbCrTextureData& aMap)
{
  MOZ_ASSERT(IsValid());

  return mData ? mData->BorrowMappedYCbCrData(aMap) : false;
}

bool
TextureClient::ToSurfaceDescriptor(SurfaceDescriptor& aOutDescriptor)
{
  MOZ_ASSERT(IsValid());

  return mData ? mData->Serialize(aOutDescriptor) : false;
}

// static
PTextureChild*
TextureClient::CreateIPDLActor()
{
  TextureChild* c = new TextureChild();
  c->AddIPDLReference();
  return c;
}

// static
bool
TextureClient::DestroyIPDLActor(PTextureChild* actor)
{
  static_cast<TextureChild*>(actor)->ReleaseIPDLReference();
  return true;
}

// static
already_AddRefed<TextureClient>
TextureClient::AsTextureClient(PTextureChild* actor)
{
  if (!actor) {
    return nullptr;
  }

  TextureChild* tc = static_cast<TextureChild*>(actor);

  tc->Lock();

  // Since TextureClient may be destroyed asynchronously with respect to its
  // IPDL actor, we must acquire a reference within a lock. The mDestroyed bit
  // tells us whether or not the main thread has disconnected the TextureClient
  // from its actor.
  if (tc->mDestroyed) {
    tc->Unlock();
    return nullptr;
  }

  RefPtr<TextureClient> texture = tc->mTextureClient;
  tc->Unlock();

  return texture.forget();
}

bool
TextureClient::IsSharedWithCompositor() const {
  return mActor && mActor->IPCOpen();
}

void
TextureClient::AddFlags(TextureFlags aFlags)
{
  MOZ_ASSERT(!IsSharedWithCompositor() ||
             ((GetFlags() & TextureFlags::RECYCLE) && !IsAddedToCompositableClient()));
  mFlags |= aFlags;
}

void
TextureClient::RemoveFlags(TextureFlags aFlags)
{
  MOZ_ASSERT(!IsSharedWithCompositor() ||
             ((GetFlags() & TextureFlags::RECYCLE) && !IsAddedToCompositableClient()));
  mFlags &= ~aFlags;
}

void
TextureClient::RecycleTexture(TextureFlags aFlags)
{
  MOZ_ASSERT(GetFlags() & TextureFlags::RECYCLE);
  MOZ_ASSERT(!mIsLocked);

  mAddedToCompositableClient = false;
  if (mFlags != aFlags) {
    mFlags = aFlags;
  }
}

void
TextureClient::SetAddedToCompositableClient()
{
  if (!mAddedToCompositableClient) {
    mAddedToCompositableClient = true;
    if(!(GetFlags() & TextureFlags::RECYCLE)) {
      return;
    }
    MOZ_ASSERT(!mIsLocked);
    LockActor();
    if (IsValid() && mActor && !mActor->mDestroyed && mActor->IPCOpen()) {
      mActor->SendRecycleTexture(mFlags);
    }
    UnlockActor();
  }
}

void CancelTextureClientRecycle(uint64_t aTextureId, LayersIPCChannel* aAllocator)
{
  if (!aAllocator) {
    return;
  }
  MessageLoop* msgLoop = nullptr;
  msgLoop = aAllocator->GetMessageLoop();
  if (!msgLoop) {
    return;
  }
  if (MessageLoop::current() == msgLoop) {
    aAllocator->CancelWaitForRecycle(aTextureId);
  } else {
    msgLoop->PostTask(NewRunnableFunction(CancelTextureClientRecycle,
                                          aTextureId, aAllocator));
  }
}

void
TextureClient::CancelWaitForRecycle()
{
  if (GetFlags() & TextureFlags::RECYCLE) {
    CancelTextureClientRecycle(mSerial, GetAllocator());
    return;
  }
}

/* static */ void
TextureClient::TextureClientRecycleCallback(TextureClient* aClient, void* aClosure)
{
  MOZ_ASSERT(aClient->GetRecycleAllocator());
  aClient->GetRecycleAllocator()->RecycleTextureClient(aClient);
}

void
TextureClient::SetRecycleAllocator(ITextureClientRecycleAllocator* aAllocator)
{
  mRecycleAllocator = aAllocator;
  if (aAllocator) {
    SetRecycleCallback(TextureClientRecycleCallback, nullptr);
  } else {
    ClearRecycleCallback();
  }
}

bool
TextureClient::InitIPDLActor(CompositableForwarder* aForwarder)
{
  MOZ_ASSERT(aForwarder && aForwarder->GetTextureForwarder()->GetMessageLoop() == mAllocator->GetMessageLoop());

  if (mActor && !mActor->IPCOpen()) {
    return false;
  }

  if (mActor && !mActor->mDestroyed) {
    CompositableForwarder* currentFwd = mActor->mCompositableForwarder;
    TextureForwarder* currentTexFwd = mActor->mTextureForwarder;
    if (currentFwd != aForwarder) {
      // It's a bit iffy but right now ShadowLayerForwarder inherits TextureForwarder
      // even though it should not. ShadowLayerForwarder::GetTextureForwarder actually
      // returns a pointer to the CompositorBridgeChild.
      // It's Ok for a texture to move from a ShadowLayerForwarder to another, but
      // not form a CompositorBridgeChild to another (they use different channels).
      if (currentTexFwd && currentTexFwd != aForwarder->GetTextureForwarder()) {
        gfxCriticalError() << "Attempt to move a texture to a different channel CF.";
        return false;
      }
      if (currentFwd && currentFwd->GetCompositorBackendType() != aForwarder->GetCompositorBackendType()) {
        gfxCriticalError() << "Attempt to move a texture to different compositor backend.";
        return false;
      }
      if (ShadowLayerForwarder* forwarder = aForwarder->AsLayerForwarder()) {
        // Do the DOM labeling.
        if (nsIEventTarget* target = forwarder->GetEventTarget()) {
          forwarder->GetCompositorBridgeChild()->ReplaceEventTargetForActor(
            mActor, target);
        }
      }
      mActor->mCompositableForwarder = aForwarder;
    }
    return true;
  }
  MOZ_ASSERT(!mActor || mActor->mDestroyed, "Cannot use a texture on several IPC channels.");

  SurfaceDescriptor desc;
  if (!ToSurfaceDescriptor(desc)) {
    return false;
  }

  // Try external image id allocation.
  mExternalImageId = aForwarder->GetTextureForwarder()->GetNextExternalImageId();

  nsIEventTarget* target = nullptr;
  // Get the layers id if the forwarder is a ShadowLayerForwarder.
  if (ShadowLayerForwarder* forwarder = aForwarder->AsLayerForwarder()) {
    target = forwarder->GetEventTarget();
  }

  PTextureChild* actor = aForwarder->GetTextureForwarder()->CreateTexture(
    desc,
    aForwarder->GetCompositorBackendType(),
    GetFlags(),
    mSerial,
    mExternalImageId,
    target);

  if (!actor) {
    gfxCriticalNote << static_cast<int32_t>(desc.type()) << ", "
                    << static_cast<int32_t>(aForwarder->GetCompositorBackendType()) << ", "
                    << static_cast<uint32_t>(GetFlags())
                    << ", " << mSerial;
    return false;
  }

  mActor = static_cast<TextureChild*>(actor);
  mActor->mCompositableForwarder = aForwarder;
  mActor->mTextureForwarder = aForwarder->GetTextureForwarder();
  mActor->mTextureClient = this;
  mActor->mMainThreadOnly = !!(mFlags & TextureFlags::DEALLOCATE_MAIN_THREAD);

  // If the TextureClient is already locked, we have to lock TextureChild's mutex
  // since it will be unlocked in TextureClient::Unlock.
  if (mIsLocked) {
    LockActor();
  }

  return mActor->IPCOpen();
}

bool
TextureClient::InitIPDLActor(KnowsCompositor* aForwarder)
{
  MOZ_ASSERT(aForwarder && aForwarder->GetTextureForwarder()->GetMessageLoop() == mAllocator->GetMessageLoop());
  TextureForwarder* fwd = aForwarder->GetTextureForwarder();
  if (mActor && !mActor->mDestroyed) {
    CompositableForwarder* currentFwd = mActor->mCompositableForwarder;
    TextureForwarder* currentTexFwd = mActor->mTextureForwarder;

    if (currentFwd) {
      gfxCriticalError() << "Attempt to remove a texture from a CompositableForwarder.";
      return false;
    }

    if (currentTexFwd && currentTexFwd != fwd) {
      gfxCriticalError() << "Attempt to move a texture to a different channel TF.";
      return false;
    }
    mActor->mTextureForwarder = fwd;
    return true;
  }
  MOZ_ASSERT(!mActor || mActor->mDestroyed, "Cannot use a texture on several IPC channels.");

  SurfaceDescriptor desc;
  if (!ToSurfaceDescriptor(desc)) {
    return false;
  }

  // Try external image id allocation.
  mExternalImageId = aForwarder->GetTextureForwarder()->GetNextExternalImageId();

  PTextureChild* actor = fwd->CreateTexture(
    desc,
    aForwarder->GetCompositorBackendType(),
    GetFlags(),
    mSerial,
    mExternalImageId);
  if (!actor) {
    gfxCriticalNote << static_cast<int32_t>(desc.type()) << ", "
                    << static_cast<int32_t>(aForwarder->GetCompositorBackendType()) << ", "
                    << static_cast<uint32_t>(GetFlags())
                    << ", " << mSerial;
    return false;
  }

  mActor = static_cast<TextureChild*>(actor);
  mActor->mTextureForwarder = fwd;
  mActor->mTextureClient = this;
  mActor->mMainThreadOnly = !!(mFlags & TextureFlags::DEALLOCATE_MAIN_THREAD);

  // If the TextureClient is already locked, we have to lock TextureChild's mutex
  // since it will be unlocked in TextureClient::Unlock.
  if (mIsLocked) {
    LockActor();
  }

  return mActor->IPCOpen();
}

PTextureChild*
TextureClient::GetIPDLActor()
{
  return mActor;
}

static inline gfx::BackendType
BackendTypeForBackendSelector(LayersBackend aLayersBackend, BackendSelector aSelector)
{
  switch (aSelector) {
    case BackendSelector::Canvas:
      return gfxPlatform::GetPlatform()->GetPreferredCanvasBackend();
    case BackendSelector::Content:
      return gfxPlatform::GetPlatform()->GetContentBackendFor(aLayersBackend);
    default:
      MOZ_ASSERT_UNREACHABLE("Unknown backend selector");
      return gfx::BackendType::NONE;
  }
};

// static
already_AddRefed<TextureClient>
TextureClient::CreateForDrawing(KnowsCompositor* aAllocator,
                                gfx::SurfaceFormat aFormat,
                                gfx::IntSize aSize,
                                BackendSelector aSelector,
                                TextureFlags aTextureFlags,
                                TextureAllocationFlags aAllocFlags)
{
  LayersBackend layersBackend = aAllocator->GetCompositorBackendType();
  return TextureClient::CreateForDrawing(aAllocator->GetTextureForwarder(),
                                         aFormat, aSize,
                                         layersBackend,
                                         aAllocator->GetMaxTextureSize(),
                                         aSelector,
                                         aTextureFlags,
                                         aAllocFlags);
}

// static
already_AddRefed<TextureClient>
TextureClient::CreateForDrawing(TextureForwarder* aAllocator,
                                gfx::SurfaceFormat aFormat,
                                gfx::IntSize aSize,
                                LayersBackend aLayersBackend,
                                int32_t aMaxTextureSize,
                                BackendSelector aSelector,
                                TextureFlags aTextureFlags,
                                TextureAllocationFlags aAllocFlags)
{
  gfx::BackendType moz2DBackend = BackendTypeForBackendSelector(aLayersBackend, aSelector);

  // also test the validity of aAllocator
  if (!aAllocator || !aAllocator->IPCOpen()) {
    return nullptr;
  }

  if (!gfx::Factory::AllowedSurfaceSize(aSize)) {
    return nullptr;
  }

  TextureData* data = nullptr;

#ifdef XP_WIN
  if (aLayersBackend == LayersBackend::LAYERS_D3D11 &&
      (moz2DBackend == gfx::BackendType::DIRECT2D ||
       moz2DBackend == gfx::BackendType::DIRECT2D1_1 ||
       (!!(aAllocFlags & ALLOC_FOR_OUT_OF_BAND_CONTENT) &&
        DeviceManagerDx::Get()->GetContentDevice())) &&
      aSize.width <= aMaxTextureSize &&
      aSize.height <= aMaxTextureSize &&
      !(aAllocFlags & ALLOC_UPDATE_FROM_SURFACE))
  {
    data = DXGITextureData::Create(aSize, aFormat, aAllocFlags);
  }

  if (aLayersBackend != LayersBackend::LAYERS_WR &&
      !data && aFormat == SurfaceFormat::B8G8R8X8 &&
      moz2DBackend == gfx::BackendType::CAIRO &&
      NS_IsMainThread()) {
    data = DIBTextureData::Create(aSize, aFormat, aAllocator);
  }
#endif

#ifdef MOZ_X11
  gfxSurfaceType type =
    gfxPlatform::GetPlatform()->ScreenReferenceSurface()->GetType();

  if (!data && aLayersBackend == LayersBackend::LAYERS_BASIC &&
      moz2DBackend == gfx::BackendType::CAIRO &&
      type == gfxSurfaceType::Xlib)
  {
    data = X11TextureData::Create(aSize, aFormat, aTextureFlags, aAllocator);
  }
#ifdef GL_PROVIDER_GLX
  if (!data && aLayersBackend == LayersBackend::LAYERS_OPENGL &&
      type == gfxSurfaceType::Xlib &&
      aFormat != SurfaceFormat::A8 &&
      gl::sGLXLibrary.UseTextureFromPixmap())
  {
    data = X11TextureData::Create(aSize, aFormat, aTextureFlags, aAllocator);
  }
#endif
#endif

#ifdef XP_MACOSX
  if (!data && gfxPrefs::UseIOSurfaceTextures()) {
    data = MacIOSurfaceTextureData::Create(aSize, aFormat, moz2DBackend);
  }
#endif

  if (data) {
    return MakeAndAddRef<TextureClient>(data, aTextureFlags, aAllocator);
  }

  if (moz2DBackend == BackendType::SKIA && aFormat == SurfaceFormat::B8G8R8X8) {
    // Skia doesn't support RGBX, so ensure we clear the buffer for the proper alpha values.
    aAllocFlags = TextureAllocationFlags(aAllocFlags | ALLOC_CLEAR_BUFFER);
  }

  // Can't do any better than a buffer texture client.
  return TextureClient::CreateForRawBufferAccess(aAllocator, aFormat, aSize,
                                                 moz2DBackend, aLayersBackend,
                                                 aTextureFlags, aAllocFlags);
}

// static
already_AddRefed<TextureClient>
TextureClient::CreateFromSurface(KnowsCompositor* aAllocator,
                                 gfx::SourceSurface* aSurface,
                                 BackendSelector aSelector,
                                 TextureFlags aTextureFlags,
                                 TextureAllocationFlags aAllocFlags)
{
  // also test the validity of aAllocator
  if (!aAllocator || !aAllocator->GetTextureForwarder()->IPCOpen()) {
    return nullptr;
  }

  gfx::IntSize size = aSurface->GetSize();

  if (!gfx::Factory::AllowedSurfaceSize(size)) {
    return nullptr;
  }

  TextureData* data = nullptr;
#if defined(XP_WIN)
  LayersBackend layersBackend = aAllocator->GetCompositorBackendType();
  gfx::BackendType moz2DBackend = BackendTypeForBackendSelector(layersBackend, aSelector);

  int32_t maxTextureSize = aAllocator->GetMaxTextureSize();

  if (layersBackend == LayersBackend::LAYERS_D3D11 &&
    (moz2DBackend == gfx::BackendType::DIRECT2D ||
      moz2DBackend == gfx::BackendType::DIRECT2D1_1 ||
      (!!(aAllocFlags & ALLOC_FOR_OUT_OF_BAND_CONTENT) &&
       DeviceManagerDx::Get()->GetContentDevice())) &&
    size.width <= maxTextureSize &&
    size.height <= maxTextureSize)
  {
    data = D3D11TextureData::Create(aSurface, aAllocFlags);
  }
#endif

  if (data) {
    return MakeAndAddRef<TextureClient>(data, aTextureFlags, aAllocator->GetTextureForwarder());
  }

  // Fall back to using UpdateFromSurface

  TextureAllocationFlags allocFlags = TextureAllocationFlags(aAllocFlags | ALLOC_UPDATE_FROM_SURFACE);
  RefPtr<TextureClient> client = CreateForDrawing(aAllocator, aSurface->GetFormat(), size,
                                                  aSelector, aTextureFlags, allocFlags);
  if (!client) {
    return nullptr;
  }

  TextureClientAutoLock autoLock(client, OpenMode::OPEN_WRITE_ONLY);
  if (!autoLock.Succeeded()) {
    return nullptr;
  }

  client->UpdateFromSurface(aSurface);
  return client.forget();
}

// static
already_AddRefed<TextureClient>
TextureClient::CreateForRawBufferAccess(KnowsCompositor* aAllocator,
                                        gfx::SurfaceFormat aFormat,
                                        gfx::IntSize aSize,
                                        gfx::BackendType aMoz2DBackend,
                                        TextureFlags aTextureFlags,
                                        TextureAllocationFlags aAllocFlags)
{
  return CreateForRawBufferAccess(aAllocator->GetTextureForwarder(),
                                  aFormat, aSize, aMoz2DBackend,
                                  aAllocator->GetCompositorBackendType(),
                                  aTextureFlags, aAllocFlags);
}

// static
already_AddRefed<TextureClient>
TextureClient::CreateForRawBufferAccess(LayersIPCChannel* aAllocator,
                                        gfx::SurfaceFormat aFormat,
                                        gfx::IntSize aSize,
                                        gfx::BackendType aMoz2DBackend,
                                        LayersBackend aLayersBackend,
                                        TextureFlags aTextureFlags,
                                        TextureAllocationFlags aAllocFlags)
{
  // also test the validity of aAllocator
  if (!aAllocator || !aAllocator->IPCOpen()) {
    return nullptr;
  }

  if (aAllocFlags & ALLOC_DISALLOW_BUFFERTEXTURECLIENT) {
    return nullptr;
  }

  if (!gfx::Factory::AllowedSurfaceSize(aSize)) {
    return nullptr;
  }

  // D2D backend does not support CreateDrawTargetForData(). Use CAIRO instead.
  if (aMoz2DBackend == gfx::BackendType::DIRECT2D ||
      aMoz2DBackend == gfx::BackendType::DIRECT2D1_1) {
    aMoz2DBackend = gfx::BackendType::CAIRO;
  }

  TextureData* texData = BufferTextureData::Create(aSize, aFormat, aMoz2DBackend,
                                                   aLayersBackend, aTextureFlags,
                                                   aAllocFlags, aAllocator);
  if (!texData) {
    return nullptr;
  }

  return MakeAndAddRef<TextureClient>(texData, aTextureFlags, aAllocator);
}

// static
already_AddRefed<TextureClient>
TextureClient::CreateForYCbCr(KnowsCompositor* aAllocator,
                              gfx::IntSize aYSize,
                              gfx::IntSize aCbCrSize,
                              StereoMode aStereoMode,
                              YUVColorSpace aYUVColorSpace,
                              TextureFlags aTextureFlags)
{
  if (!aAllocator || !aAllocator->GetLayersIPCActor()->IPCOpen()) {
    return nullptr;
  }

  if (!gfx::Factory::AllowedSurfaceSize(aYSize)) {
    return nullptr;
  }

  TextureData* data = BufferTextureData::CreateForYCbCr(aAllocator, aYSize, aCbCrSize,
                                                        aStereoMode, aYUVColorSpace,
                                                        aTextureFlags);
  if (!data) {
    return nullptr;
  }

  return MakeAndAddRef<TextureClient>(data, aTextureFlags,
                                      aAllocator->GetTextureForwarder());
}

// static
already_AddRefed<TextureClient>
TextureClient::CreateForYCbCrWithBufferSize(KnowsCompositor* aAllocator,
                                            size_t aSize,
                                            YUVColorSpace aYUVColorSpace,
                                            TextureFlags aTextureFlags)
{
  if (!aAllocator || !aAllocator->GetLayersIPCActor()->IPCOpen()) {
    return nullptr;
  }

  TextureData* data =
    BufferTextureData::CreateForYCbCrWithBufferSize(aAllocator, aSize, aYUVColorSpace,
                                                    aTextureFlags);
  if (!data) {
    return nullptr;
  }

  return MakeAndAddRef<TextureClient>(data, aTextureFlags,
                                      aAllocator->GetTextureForwarder());
}

TextureClient::TextureClient(TextureData* aData, TextureFlags aFlags, LayersIPCChannel* aAllocator)
: AtomicRefCountedWithFinalize("TextureClient")
, mAllocator(aAllocator)
, mActor(nullptr)
, mData(aData)
, mFlags(aFlags)
, mOpenMode(OpenMode::OPEN_NONE)
#ifdef DEBUG
, mExpectedDtRefs(0)
#endif
, mIsLocked(false)
, mIsReadLocked(false)
, mUpdated(false)
, mAddedToCompositableClient(false)
, mWorkaroundAnnoyingSharedSurfaceLifetimeIssues(false)
, mWorkaroundAnnoyingSharedSurfaceOwnershipIssues(false)
, mFwdTransactionId(0)
, mSerial(++sSerialCounter)
#ifdef GFX_DEBUG_TRACK_CLIENTS_IN_POOL
, mPoolTracker(nullptr)
#endif
{
  mData->FillInfo(mInfo);
  mFlags |= mData->GetTextureFlags();
}

bool TextureClient::CopyToTextureClient(TextureClient* aTarget,
                                        const gfx::IntRect* aRect,
                                        const gfx::IntPoint* aPoint)
{
  MOZ_ASSERT(IsLocked());
  MOZ_ASSERT(aTarget->IsLocked());

  if (!aTarget->CanExposeDrawTarget() || !CanExposeDrawTarget()) {
    return false;
  }

  RefPtr<DrawTarget> destinationTarget = aTarget->BorrowDrawTarget();
  if (!destinationTarget) {
      gfxWarning() << "TextureClient::CopyToTextureClient (dest) failed in BorrowDrawTarget";
    return false;
  }

  RefPtr<DrawTarget> sourceTarget = BorrowDrawTarget();
  if (!sourceTarget) {
    gfxWarning() << "TextureClient::CopyToTextureClient (src) failed in BorrowDrawTarget";
    return false;
  }

  RefPtr<gfx::SourceSurface> source = sourceTarget->Snapshot();
  destinationTarget->CopySurface(source,
                                 aRect ? *aRect : gfx::IntRect(gfx::IntPoint(0, 0), GetSize()),
                                 aPoint ? *aPoint : gfx::IntPoint(0, 0));
  return true;
}

already_AddRefed<gfx::DataSourceSurface>
TextureClient::GetAsSurface()
{
  if (!Lock(OpenMode::OPEN_READ)) {
    return nullptr;
  }
  RefPtr<gfx::DataSourceSurface> data;
  {  // scope so that the DrawTarget is destroyed before Unlock()
    RefPtr<gfx::DrawTarget> dt = BorrowDrawTarget();
    if (dt) {
      RefPtr<gfx::SourceSurface> surf = dt->Snapshot();
      if (surf) {
        data = surf->GetDataSurface();
      }
    }
  }
  Unlock();
  return data.forget();
}

void
TextureClient::PrintInfo(std::stringstream& aStream, const char* aPrefix)
{
  aStream << aPrefix;
  aStream << nsPrintfCString("TextureClient (0x%p)", this).get();
  AppendToString(aStream, GetSize(), " [size=", "]");
  AppendToString(aStream, GetFormat(), " [format=", "]");
  AppendToString(aStream, mFlags, " [flags=", "]");

#ifdef MOZ_DUMP_PAINTING
  if (gfxPrefs::LayersDumpTexture() ||
      profiler_feature_active(ProfilerFeature::LayersDump)) {
    nsAutoCString pfx(aPrefix);
    pfx += "  ";

    aStream << "\n" << pfx.get() << "Surface: ";
    RefPtr<gfx::DataSourceSurface> dSurf = GetAsSurface();
    if (dSurf) {
      aStream << gfxUtils::GetAsLZ4Base64Str(dSurf).get();
    }
  }
#endif
}

class MemoryTextureReadLock : public NonBlockingTextureReadLock {
public:
  MemoryTextureReadLock();

  ~MemoryTextureReadLock();

  virtual bool ReadLock() override;

  virtual int32_t ReadUnlock() override;

  virtual int32_t GetReadCount() override;

  virtual LockType GetType() override { return TYPE_NONBLOCKING_MEMORY; }

  virtual bool IsValid() const override { return true; };

  virtual bool Serialize(ReadLockDescriptor& aOutput, base::ProcessId aOther) override;

  int32_t mReadCount;
};

// The cross-prcess implementation of TextureReadLock.
//
// Since we don't use cross-process reference counting for the ReadLock objects,
// we use the lock's internal counter as a way to know when to deallocate the
// underlying shmem section: when the counter is equal to 1, it means that the
// lock is not "held" (the texture is writable), when the counter is equal to 0
// it means that we can safely deallocate the shmem section without causing a race
// condition with the other process.
class ShmemTextureReadLock : public NonBlockingTextureReadLock {
public:
  struct ShmReadLockInfo {
    int32_t readCount;
  };

  explicit ShmemTextureReadLock(LayersIPCChannel* aAllocator);

  ~ShmemTextureReadLock();

  virtual bool ReadLock() override;

  virtual int32_t ReadUnlock() override;

  virtual int32_t GetReadCount() override;

  virtual bool IsValid() const override { return mAllocSuccess; };

  virtual LockType GetType() override { return TYPE_NONBLOCKING_SHMEM; }

  virtual bool Serialize(ReadLockDescriptor& aOutput, base::ProcessId aOther) override;

  mozilla::layers::ShmemSection& GetShmemSection() { return mShmemSection; }

  explicit ShmemTextureReadLock(const mozilla::layers::ShmemSection& aShmemSection)
    : mShmemSection(aShmemSection)
    , mAllocSuccess(true)
  {
    MOZ_COUNT_CTOR(ShmemTextureReadLock);
  }

  ShmReadLockInfo* GetShmReadLockInfoPtr()
  {
    return reinterpret_cast<ShmReadLockInfo*>
      (mShmemSection.shmem().get<char>() + mShmemSection.offset());
  }

  RefPtr<LayersIPCChannel> mClientAllocator;
  mozilla::layers::ShmemSection mShmemSection;
  bool mAllocSuccess;
};

class CrossProcessSemaphoreReadLock : public TextureReadLock
{
public:
  CrossProcessSemaphoreReadLock()
    : mSemaphore(CrossProcessSemaphore::Create("TextureReadLock", 1))
  {}
  explicit CrossProcessSemaphoreReadLock(CrossProcessSemaphoreHandle aHandle)
    : mSemaphore(CrossProcessSemaphore::Create(aHandle))
  {}

  virtual bool ReadLock() override
  {
    if (!IsValid()) {
      return false;
    }
    return mSemaphore->Wait();
  }
  virtual bool TryReadLock(TimeDuration aTimeout) override
  {
    if (!IsValid()) {
      return false;
    }
    return mSemaphore->Wait(Some(aTimeout));
  }
  virtual int32_t ReadUnlock() override
  {
    if (!IsValid()) {
      return 1;
    }
    mSemaphore->Signal();
    return 1;
  }
  virtual bool IsValid() const override { return !!mSemaphore; }

  virtual bool Serialize(ReadLockDescriptor& aOutput, base::ProcessId aOther) override;

  virtual LockType GetType() override { return TYPE_CROSS_PROCESS_SEMAPHORE; }

  UniquePtr<CrossProcessSemaphore> mSemaphore;
};

// static
already_AddRefed<TextureReadLock>
TextureReadLock::Deserialize(const ReadLockDescriptor& aDescriptor, ISurfaceAllocator* aAllocator)
{
  switch (aDescriptor.type()) {
    case ReadLockDescriptor::TShmemSection: {
      const ShmemSection& section = aDescriptor.get_ShmemSection();
      MOZ_RELEASE_ASSERT(section.shmem().IsReadable());
      return MakeAndAddRef<ShmemTextureReadLock>(section);
    }
    case ReadLockDescriptor::Tuintptr_t: {
      if (!aAllocator->IsSameProcess()) {
        // Trying to use a memory based lock instead of a shmem based one in
        // the cross-process case is a bad security violation.
        NS_ERROR("A client process may be trying to peek at the host's address space!");
        return nullptr;
      }
      RefPtr<TextureReadLock> lock = reinterpret_cast<MemoryTextureReadLock*>(
        aDescriptor.get_uintptr_t()
      );

      MOZ_ASSERT(lock);
      if (lock) {
        // The corresponding AddRef is in MemoryTextureReadLock::Serialize
        lock.get()->Release();
      }

      return lock.forget();
    }
    case ReadLockDescriptor::TCrossProcessSemaphoreDescriptor: {
      return MakeAndAddRef<CrossProcessSemaphoreReadLock>(aDescriptor.get_CrossProcessSemaphoreDescriptor().sem());
    }
    case ReadLockDescriptor::Tnull_t: {
      return nullptr;
    }
    default: {
      // Invalid descriptor.
      MOZ_DIAGNOSTIC_ASSERT(false);
    }
  }
  return nullptr;
}
// static
already_AddRefed<TextureReadLock>
NonBlockingTextureReadLock::Create(LayersIPCChannel* aAllocator)
{
  if (aAllocator->IsSameProcess()) {
    // If our compositor is in the same process, we can save some cycles by not
    // using shared memory.
    return MakeAndAddRef<MemoryTextureReadLock>();
  }

  return MakeAndAddRef<ShmemTextureReadLock>(aAllocator);
}

MemoryTextureReadLock::MemoryTextureReadLock()
: mReadCount(1)
{
  MOZ_COUNT_CTOR(MemoryTextureReadLock);
}

MemoryTextureReadLock::~MemoryTextureReadLock()
{
  // One read count that is added in constructor.
  MOZ_ASSERT(mReadCount == 1);
  MOZ_COUNT_DTOR(MemoryTextureReadLock);
}

bool
MemoryTextureReadLock::Serialize(ReadLockDescriptor& aOutput, base::ProcessId aOther)
{
  // AddRef here and Release when receiving on the host side to make sure the
  // reference count doesn't go to zero before the host receives the message.
  // see TextureReadLock::Deserialize
  this->AddRef();
  aOutput = ReadLockDescriptor(uintptr_t(this));
  return true;
}

bool
MemoryTextureReadLock::ReadLock()
{
  NS_ASSERT_OWNINGTHREAD(MemoryTextureReadLock);

  PR_ATOMIC_INCREMENT(&mReadCount);
  return true;
}

int32_t
MemoryTextureReadLock::ReadUnlock()
{
  int32_t readCount = PR_ATOMIC_DECREMENT(&mReadCount);
  MOZ_ASSERT(readCount >= 0);

  return readCount;
}

int32_t
MemoryTextureReadLock::GetReadCount()
{
  NS_ASSERT_OWNINGTHREAD(MemoryTextureReadLock);
  return mReadCount;
}

ShmemTextureReadLock::ShmemTextureReadLock(LayersIPCChannel* aAllocator)
  : mClientAllocator(aAllocator)
  , mAllocSuccess(false)
{
  MOZ_COUNT_CTOR(ShmemTextureReadLock);
  MOZ_ASSERT(mClientAllocator);
#define MOZ_ALIGN_WORD(x) (((x) + 3) & ~3)
  if (mClientAllocator->GetTileLockAllocator()->AllocShmemSection(
      MOZ_ALIGN_WORD(sizeof(ShmReadLockInfo)), &mShmemSection)) {
    ShmReadLockInfo* info = GetShmReadLockInfoPtr();
    info->readCount = 1;
    mAllocSuccess = true;
  }
}

ShmemTextureReadLock::~ShmemTextureReadLock()
{
  if (mClientAllocator) {
    // Release one read count that is added in constructor.
    // The count is kept for calling GetReadCount() by TextureClientPool.
    ReadUnlock();
  }
  MOZ_COUNT_DTOR(ShmemTextureReadLock);
}

bool
ShmemTextureReadLock::Serialize(ReadLockDescriptor& aOutput, base::ProcessId aOther)
{
  aOutput = ReadLockDescriptor(GetShmemSection());
  return true;
}

bool
ShmemTextureReadLock::ReadLock() {
  NS_ASSERT_OWNINGTHREAD(ShmemTextureReadLock);
  if (!mAllocSuccess) {
    return false;
  }
  ShmReadLockInfo* info = GetShmReadLockInfoPtr();
  PR_ATOMIC_INCREMENT(&info->readCount);
  return true;
}

int32_t
ShmemTextureReadLock::ReadUnlock() {
  if (!mAllocSuccess) {
    return 0;
  }
  ShmReadLockInfo* info = GetShmReadLockInfoPtr();
  int32_t readCount = PR_ATOMIC_DECREMENT(&info->readCount);
  MOZ_ASSERT(readCount >= 0);
  if (readCount <= 0) {
    if (mClientAllocator && mClientAllocator->GetTileLockAllocator()) {
      mClientAllocator->GetTileLockAllocator()->DeallocShmemSection(mShmemSection);
    } else {
      // we are on the compositor process, or IPC is down.
      FixedSizeSmallShmemSectionAllocator::FreeShmemSection(mShmemSection);
    }
  }
  return readCount;
}

int32_t
ShmemTextureReadLock::GetReadCount() {
  NS_ASSERT_OWNINGTHREAD(ShmemTextureReadLock);
  if (!mAllocSuccess) {
    return 0;
  }
  ShmReadLockInfo* info = GetShmReadLockInfoPtr();
  return info->readCount;
}

bool
CrossProcessSemaphoreReadLock::Serialize(ReadLockDescriptor& aOutput, base::ProcessId aOther)
{
  if (IsValid()) {
    aOutput = ReadLockDescriptor(CrossProcessSemaphoreDescriptor(mSemaphore->ShareToProcess(aOther)));
    return true;
  } else {
    return false;
  }
}

void
TextureClient::EnableBlockingReadLock()
{
  if (!mReadLock) {
    mReadLock = new CrossProcessSemaphoreReadLock();
  }
}

bool
UpdateYCbCrTextureClient(TextureClient* aTexture, const PlanarYCbCrData& aData)
{
  MOZ_ASSERT(aTexture);
  MOZ_ASSERT(aTexture->IsLocked());
  MOZ_ASSERT(aTexture->GetFormat() == gfx::SurfaceFormat::YUV, "This textureClient can only use YCbCr data");
  MOZ_ASSERT(!aTexture->IsImmutable());
  MOZ_ASSERT(aTexture->IsValid());
  MOZ_ASSERT(aData.mCbSkip == aData.mCrSkip);

  MappedYCbCrTextureData mapped;
  if (!aTexture->BorrowMappedYCbCrData(mapped)) {
    NS_WARNING("Failed to extract YCbCr info!");
    return false;
  }

  MappedYCbCrTextureData srcData;
  srcData.y.data = aData.mYChannel;
  srcData.y.size = aData.mYSize;
  srcData.y.stride = aData.mYStride;
  srcData.y.skip = aData.mYSkip;
  srcData.cb.data = aData.mCbChannel;
  srcData.cb.size = aData.mCbCrSize;
  srcData.cb.stride = aData.mCbCrStride;
  srcData.cb.skip = aData.mCbSkip;
  srcData.cr.data = aData.mCrChannel;
  srcData.cr.size = aData.mCbCrSize;
  srcData.cr.stride = aData.mCbCrStride;
  srcData.cr.skip = aData.mCrSkip;
  srcData.metadata = nullptr;

  if (!srcData.CopyInto(mapped)) {
    NS_WARNING("Failed to copy image data!");
    return false;
  }

  if (TextureRequiresLocking(aTexture->GetFlags())) {
    // We don't have support for proper locking yet, so we'll
    // have to be immutable instead.
    aTexture->MarkImmutable();
  }
  return true;
}

already_AddRefed<SyncObject>
SyncObject::CreateSyncObject(SyncHandle aHandle
#ifdef XP_WIN
                             , ID3D11Device* aDevice
#endif
                             )
{
  if (!aHandle) {
    return nullptr;
  }

#ifdef XP_WIN
  return MakeAndAddRef<SyncObjectD3D11>(aHandle, aDevice);
#else
  MOZ_ASSERT_UNREACHABLE();
  return nullptr;
#endif
}

already_AddRefed<TextureClient>
TextureClient::CreateWithData(TextureData* aData, TextureFlags aFlags, LayersIPCChannel* aAllocator)
{
  if (!aData) {
    return nullptr;
  }
  return MakeAndAddRef<TextureClient>(aData, aFlags, aAllocator);
}

bool
MappedYCbCrChannelData::CopyInto(MappedYCbCrChannelData& aDst)
{
  if (!data || !aDst.data || size != aDst.size) {
    return false;
  }

  if (stride == aDst.stride) {
    // fast path!
    // We assume that the padding in the destination is there for alignment
    // purposes and doesn't contain useful data.
    memcpy(aDst.data, data, stride * size.height);
    return true;
  }

  for (int32_t i = 0; i < size.height; ++i) {
    if (aDst.skip == 0 && skip == 0) {
      // fast-ish path
      memcpy(aDst.data + i * aDst.stride,
             data + i * stride,
             size.width);
    } else {
      // slow path
      uint8_t* src = data + i * stride;
      uint8_t* dst = aDst.data + i * aDst.stride;
      for (int32_t j = 0; j < size.width; ++j) {
        *dst = *src;
        src += 1 + skip;
        dst += 1 + aDst.skip;
      }
    }
  }
  return true;
}

} // namespace layers
} // namespace mozilla
