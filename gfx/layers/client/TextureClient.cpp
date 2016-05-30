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
#include "mozilla/layers/AsyncTransactionTracker.h"
#include "mozilla/layers/CompositableForwarder.h"
#include "mozilla/layers/ISurfaceAllocator.h"
#include "mozilla/layers/ImageDataSerializer.h"
#include "mozilla/layers/TextureClientRecycleAllocator.h"
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

#ifdef XP_WIN
#include "mozilla/layers/TextureD3D9.h"
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

#ifdef MOZ_WIDGET_GONK
#include <cutils/properties.h>
#include "mozilla/layers/GrallocTextureClient.h"
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
  RefPtr<ClientIPCAllocator> allocator;
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
class TextureChild final : public ChildActor<PTextureChild>
{
  ~TextureChild()
  {
    // We should have deallocated mTextureData in ActorDestroy
    MOZ_ASSERT(!mTextureData);
  }
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(TextureChild)

  TextureChild()
  : mForwarder(nullptr)
  , mTextureClient(nullptr)
  , mTextureData(nullptr)
  , mDestroyed(false)
  , mMainThreadOnly(false)
  , mIPCOpen(false)
  , mOwnsTextureData(false)
  {}

  bool Recv__delete__() override { return true; }

  bool RecvCompositorRecycle() override
  {
    RECYCLE_LOG("[CLIENT] Receive recycle %p (%p)\n", mTextureClient, mWaitForRecycle.get());
    mWaitForRecycle = nullptr;
    return true;
  }

  void WaitForCompositorRecycle()
  {
    Lock();
    mWaitForRecycle = mDestroyed ? nullptr : mTextureClient;
    Unlock();

    RECYCLE_LOG("[CLIENT] Wait for recycle %p\n", mWaitForRecycle.get());
    MOZ_ASSERT(CanSend());
    SendClientRecycle();
  }

  void CancelWaitForCompositorRecycle()
  {
    RECYCLE_LOG("[CLIENT] Cancelling wait for recycle %p\n", mWaitForRecycle.get());

    Lock();
    mWaitForRecycle = nullptr;
    Unlock();
  }

  CompositableForwarder* GetForwarder() { return mForwarder; }

  ClientIPCAllocator* GetAllocator() { return mForwarder; }

  void ActorDestroy(ActorDestroyReason why) override;

  bool IPCOpen() const { return mIPCOpen; }

  void Lock() const { mLock.Enter(); }

  void Unlock() const { mLock.Leave(); }

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
    MOZ_ASSERT(mIPCOpen == true);
    mIPCOpen = false;
    Release();
  }

  mutable gfx::CriticalSection mLock;

  RefPtr<CompositableForwarder> mForwarder;
  RefPtr<TextureClient> mWaitForRecycle;

  TextureClient* mTextureClient;
  TextureData* mTextureData;
  Atomic<bool> mDestroyed;
  bool mMainThreadOnly;
  bool mIPCOpen;
  bool mOwnsTextureData;

  friend class TextureClient;
  friend void DeallocateTextureClient(TextureDeallocParams params);
};


static void DestroyTextureData(TextureData* aTextureData, ClientIPCAllocator* aAllocator,
                               bool aDeallocate, bool aMainThreadOnly)
{
  if (!aTextureData) {
    return;
  }

  if (aMainThreadOnly && !NS_IsMainThread()) {
    RefPtr<ClientIPCAllocator> allocatorRef = aAllocator;
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
  mWaitForRecycle = nullptr;

  if (mTextureData) {
    DestroyTextureData(mTextureData, GetAllocator(), mOwnsTextureData, mMainThreadOnly);
    mTextureData = nullptr;
  }
}

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
    ipdlMsgLoop = params.allocator->AsClientAllocator()->GetMessageLoop();
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
    // the IPDL thread and use the ClientIPCAllocator.
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
    // our data. This is specific to the gralloc SharedSurface.
    bool shouldDeallocate = !params.workAroundSharedSurfaceOwnershipIssue;
    DestroyTextureData(params.data, params.allocator,
                       shouldDeallocate,
                       false);  // main-thread deallocation
    return;
  }

  if (!actor->IPCOpen()) {
    // The actor is already deallocated which probably means there was a shutdown
    // race causing this function to be called concurrently which is bad!
    gfxCriticalError() << "Racy texture deallocation";
    return;
  }

  if (params.syncDeallocation) {
    MOZ_PERFORMANCE_WARNING("gfx",
      "TextureClient/Host pair requires synchronous deallocation");
    actor->DestroySynchronously(actor->GetForwarder());
    DestroyTextureData(params.data, params.allocator, params.clientDeallocation,
                       actor->mMainThreadOnly);
  } else {
    actor->mTextureData = params.data;
    actor->mOwnsTextureData = params.clientDeallocation;
    actor->Destroy(actor->GetForwarder());
    // DestroyTextureData will be called by TextureChild::ActorDestroy
  }
}

void TextureClient::Destroy(bool aForceSync)
{
  if (mActor) {
    mActor->Lock();
  }

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
    params.syncDeallocation = !!(mFlags & TextureFlags::DEALLOCATE_CLIENT) || aForceSync;

    // Release the lock before calling DeallocateTextureClient because the latter
    // may wait for the main thread which could create a dead-lock.

    if (actor) {
      actor->Unlock();
    }

    DeallocateTextureClient(params);
  }
}

bool
TextureClient::DestroyFallback(PTextureChild* aActor)
{
  // should not end up here so crash debug builds.
  MOZ_ASSERT(false);
  return aActor->SendDestroySync();
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
TextureClient::Lock(OpenMode aMode)
{
  MOZ_ASSERT(IsValid());
  MOZ_ASSERT(!mIsLocked);
  if (mIsLocked) {
    return mOpenMode == aMode;
  }

  if (mRemoveFromCompositableWaiter) {
    mRemoveFromCompositableWaiter->WaitComplete();
    mRemoveFromCompositableWaiter = nullptr;
  }

  LockActor();

  mIsLocked = mData->Lock(aMode, mReleaseFenceHandle.IsValid() ? &mReleaseFenceHandle : nullptr);
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
  }

  return mIsLocked;
}

void
TextureClient::Unlock()
{
  MOZ_ASSERT(IsValid());
  MOZ_ASSERT(mIsLocked);
  if (!mIsLocked) {
    return;
  }

  if (mBorrowedDrawTarget) {
    MOZ_ASSERT(mBorrowedDrawTarget->refCount() <= mExpectedDtRefs);
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
    mBorrowedDrawTarget = nullptr;
  }

  mData->Unlock();
  mIsLocked = false;
  mOpenMode = OpenMode::OPEN_NONE;

  UnlockActor();
}

TextureClient::~TextureClient()
{
  Destroy(false);
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
TextureClient::CreateSimilar(TextureFlags aFlags, TextureAllocationFlags aAllocFlags) const
{
  MOZ_ASSERT(IsValid());

  MOZ_ASSERT(!mIsLocked);
  if (mIsLocked) {
    return nullptr;
  }

  LockActor();
  TextureData* data = mData->CreateSimilar(mAllocator, aFlags, aAllocFlags);
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

  if (!mIsLocked) {
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

void
TextureClient::WaitForBufferOwnership(bool aWaitReleaseFence)
{
  if (mRemoveFromCompositableWaiter) {
    mRemoveFromCompositableWaiter->WaitComplete();
    mRemoveFromCompositableWaiter = nullptr;
  }

#if defined(MOZ_WIDGET_GONK) && ANDROID_VERSION < 21
  if (aWaitReleaseFence && mReleaseFenceHandle.IsValid()) {
    mData->WaitForFence(&mReleaseFenceHandle);
    mReleaseFenceHandle = FenceHandle();
  }
#endif
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

  // TODO: This is not entirely race-free because TextureClient's ref count
  // can reach zero on another thread and AsTextureClient gets called before
  // Destroy.
  tc->Lock();

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
  if (IsValid() && mActor && !mActor->mDestroyed && mActor->IPCOpen()) {
    mActor->SendRecycleTexture(mFlags);
  }
}

void
TextureClient::RemoveFlags(TextureFlags aFlags)
{
  MOZ_ASSERT(!IsSharedWithCompositor() ||
             ((GetFlags() & TextureFlags::RECYCLE) && !IsAddedToCompositableClient()));
  mFlags &= ~aFlags;
  if (IsValid() && mActor && !mActor->mDestroyed && mActor->IPCOpen()) {
    mActor->SendRecycleTexture(mFlags);
  }
}

void
TextureClient::RecycleTexture(TextureFlags aFlags)
{
  MOZ_ASSERT(GetFlags() & TextureFlags::RECYCLE);
  MOZ_ASSERT(!mIsLocked);
  if (mIsLocked) {
    return;
  }

  LockActor();

  mAddedToCompositableClient = false;
  if (mFlags != aFlags) {
    mFlags = aFlags;
    if (IsValid() && mActor && !mActor->mDestroyed && mActor->IPCOpen()) {
      mActor->SendRecycleTexture(mFlags);
    }
  }

  UnlockActor();
}

void
TextureClient::WaitForCompositorRecycle()
{
  if (IsSharedWithCompositor()) {
    mActor->WaitForCompositorRecycle();
  }
}

void
TextureClient::CancelWaitForCompositorRecycle()
{
  if (IsSharedWithCompositor()) {
    mActor->CancelWaitForCompositorRecycle();
  }
}

void
TextureClient::SetAddedToCompositableClient()
{
  if (!mAddedToCompositableClient) {
    mAddedToCompositableClient = true;
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
  MOZ_ASSERT(aForwarder && aForwarder->GetMessageLoop() == mAllocator->AsClientAllocator()->GetMessageLoop());
  if (mActor && !mActor->mDestroyed && mActor->GetForwarder() == aForwarder) {
    return true;
  }
  MOZ_ASSERT(!mActor || mActor->mDestroyed, "Cannot use a texture on several IPC channels.");

  SurfaceDescriptor desc;
  if (!ToSurfaceDescriptor(desc)) {
    return false;
  }

  mActor = static_cast<TextureChild*>(aForwarder->CreateTexture(desc, aForwarder->GetCompositorBackendType(), GetFlags()));
  MOZ_ASSERT(mActor);
  mActor->mForwarder = aForwarder;
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
TextureClient::CreateForDrawing(CompositableForwarder* aAllocator,
                                gfx::SurfaceFormat aFormat,
                                gfx::IntSize aSize,
                                BackendSelector aSelector,
                                TextureFlags aTextureFlags,
                                TextureAllocationFlags aAllocFlags)
{
  // also test the validity of aAllocator
  MOZ_ASSERT(aAllocator && aAllocator->IPCOpen());
  if (!aAllocator || !aAllocator->IPCOpen()) {
    return nullptr;
  }

  if (!gfx::Factory::AllowedSurfaceSize(aSize)) {
    return nullptr;
  }

  LayersBackend parentBackend = aAllocator->GetCompositorBackendType();
  gfx::BackendType moz2DBackend = BackendTypeForBackendSelector(parentBackend, aSelector);

  TextureData* data = nullptr;

#if defined(XP_WIN)
  int32_t maxTextureSize = aAllocator->GetMaxTextureSize();
#endif

#ifdef XP_WIN
  if (parentBackend == LayersBackend::LAYERS_D3D11 &&
      (moz2DBackend == gfx::BackendType::DIRECT2D ||
       moz2DBackend == gfx::BackendType::DIRECT2D1_1 ||
       (!!(aAllocFlags & ALLOC_FOR_OUT_OF_BAND_CONTENT) &&
        gfxWindowsPlatform::GetPlatform()->GetD3D11ContentDevice())) &&
      aSize.width <= maxTextureSize &&
      aSize.height <= maxTextureSize)
  {
    data = DXGITextureData::Create(aSize, aFormat, aAllocFlags);
  }
  if (parentBackend == LayersBackend::LAYERS_D3D9 &&
      moz2DBackend == gfx::BackendType::CAIRO &&
      aAllocator->IsSameProcess() &&
      aSize.width <= maxTextureSize &&
      aSize.height <= maxTextureSize &&
      NS_IsMainThread() &&
      gfxWindowsPlatform::GetPlatform()->GetD3D9Device()) {
    data = D3D9TextureData::Create(aSize, aFormat, aAllocFlags);
  }

  if (!data && aFormat == SurfaceFormat::B8G8R8X8 &&
      moz2DBackend == gfx::BackendType::CAIRO &&
      NS_IsMainThread()) {
    data = DIBTextureData::Create(aSize, aFormat, aAllocator);
  }
#endif

#ifdef MOZ_X11
  gfxSurfaceType type =
    gfxPlatform::GetPlatform()->ScreenReferenceSurface()->GetType();

  if (!data && parentBackend == LayersBackend::LAYERS_BASIC &&
      moz2DBackend == gfx::BackendType::CAIRO &&
      type == gfxSurfaceType::Xlib)
  {
    data = X11TextureData::Create(aSize, aFormat, aTextureFlags, aAllocator);
  }
#ifdef GL_PROVIDER_GLX
  if (!data && parentBackend == LayersBackend::LAYERS_OPENGL &&
      type == gfxSurfaceType::Xlib &&
      aFormat != SurfaceFormat::A8 &&
      gl::sGLXLibrary.UseTextureFromPixmap())
  {
    data = X11TextureData::Create(aSize, aFormat, aTextureFlags, aAllocator);
  }
#endif
#endif

#ifdef MOZ_WIDGET_GONK
  if (!data) {
    data = GrallocTextureData::CreateForDrawing(aSize, aFormat, moz2DBackend,
                                                aAllocator);
  }
#endif

  if (data) {
    return MakeAndAddRef<TextureClient>(data, aTextureFlags, aAllocator);
  }

  // Can't do any better than a buffer texture client.
  return TextureClient::CreateForRawBufferAccess(aAllocator, aFormat, aSize,
                                                 moz2DBackend, aTextureFlags, aAllocFlags);
}

// static
already_AddRefed<TextureClient>
TextureClient::CreateForRawBufferAccess(ClientIPCAllocator* aAllocator,
                                        gfx::SurfaceFormat aFormat,
                                        gfx::IntSize aSize,
                                        gfx::BackendType aMoz2DBackend,
                                        TextureFlags aTextureFlags,
                                        TextureAllocationFlags aAllocFlags)
{
  // also test the validity of aAllocator
  MOZ_ASSERT(aAllocator && aAllocator->IPCOpen());
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
                                                   aTextureFlags, aAllocFlags,
                                                   aAllocator);
  if (!texData) {
    return nullptr;
  }

  return MakeAndAddRef<TextureClient>(texData, aTextureFlags, aAllocator);
}

// static
already_AddRefed<TextureClient>
TextureClient::CreateForYCbCr(ClientIPCAllocator* aAllocator,
                              gfx::IntSize aYSize,
                              gfx::IntSize aCbCrSize,
                              StereoMode aStereoMode,
                              TextureFlags aTextureFlags)
{
  // The only reason we allow aAllocator to be null is for gtests
  MOZ_ASSERT(!aAllocator || aAllocator->IPCOpen());
  if (aAllocator && !aAllocator->IPCOpen()) {
    return nullptr;
  }

  if (!gfx::Factory::AllowedSurfaceSize(aYSize)) {
    return nullptr;
  }

  TextureData* data = BufferTextureData::CreateForYCbCr(aAllocator, aYSize, aCbCrSize,
                                                        aStereoMode, aTextureFlags);
  if (!data) {
    return nullptr;
  }

  return MakeAndAddRef<TextureClient>(data, aTextureFlags, aAllocator);
}

// static
already_AddRefed<TextureClient>
TextureClient::CreateForYCbCrWithBufferSize(ClientIPCAllocator* aAllocator,
                                            gfx::SurfaceFormat aFormat,
                                            size_t aSize,
                                            TextureFlags aTextureFlags)
{
  // also test the validity of aAllocator
  MOZ_ASSERT(aAllocator && aAllocator->IPCOpen());
  if (!aAllocator || !aAllocator->IPCOpen()) {
    return nullptr;
  }

  TextureData* data =
    BufferTextureData::CreateForYCbCrWithBufferSize(aAllocator, aFormat, aSize,
                                                    aTextureFlags);
  if (!data) {
    return nullptr;
  }

  return MakeAndAddRef<TextureClient>(data, aTextureFlags, aAllocator);
}

TextureClient::TextureClient(TextureData* aData, TextureFlags aFlags, ClientIPCAllocator* aAllocator)
: mAllocator(aAllocator)
, mActor(nullptr)
, mData(aData)
, mFlags(aFlags)
, mOpenMode(OpenMode::OPEN_NONE)
#ifdef DEBUG
, mExpectedDtRefs(0)
#endif
, mIsLocked(false)
, mInUse(false)
, mAddedToCompositableClient(false)
, mWorkaroundAnnoyingSharedSurfaceLifetimeIssues(false)
, mWorkaroundAnnoyingSharedSurfaceOwnershipIssues(false)
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

void
TextureClient::RemoveFromCompositable(CompositableClient* aCompositable,
                                      AsyncTransactionWaiter* aWaiter)
{
  MOZ_ASSERT(aCompositable);

#if defined(MOZ_WIDGET_GONK) && ANDROID_VERSION >= 17
  if (mActor && aCompositable->GetIPDLActor()
      && mData->AsGrallocTextureData()) {
    // remove old buffer from CompositableHost
    RefPtr<AsyncTransactionWaiter> waiter = aWaiter ? aWaiter
                                                    : new AsyncTransactionWaiter();
    RefPtr<AsyncTransactionTracker> tracker =
        new RemoveTextureFromCompositableTracker(waiter);
    // Hold TextureClient until transaction complete.
    tracker->SetTextureClient(this);
    mRemoveFromCompositableWaiter = waiter;
    // RemoveTextureFromCompositableAsync() expects CompositorBridgeChild's presence.
    mActor->GetForwarder()->RemoveTextureFromCompositableAsync(tracker, aCompositable, this);
  }
#endif

}

void
TextureClient::SetRemoveFromCompositableWaiter(AsyncTransactionWaiter* aWaiter) {
  mRemoveFromCompositableWaiter = aWaiter;
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
  if (gfxPrefs::LayersDumpTexture() || profiler_feature_active("layersdump")) {
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
SyncObject::CreateSyncObject(SyncHandle aHandle)
{
  if (!aHandle) {
    return nullptr;
  }

#ifdef XP_WIN
  return MakeAndAddRef<SyncObjectD3D11>(aHandle);
#else
  MOZ_ASSERT_UNREACHABLE();
  return nullptr;
#endif
}

already_AddRefed<TextureClient>
TextureClient::CreateWithData(TextureData* aData, TextureFlags aFlags, ClientIPCAllocator* aAllocator)
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
