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
#include "mozilla/layers/ImageDataSerializer.h"
#include "mozilla/layers/YCbCrImageDataSerializer.h"
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

#ifdef MOZ_WIDGET_ANDROID
#  include "gfxReusableImageSurfaceWrapper.h"
#else
#  include "gfxReusableSharedImageSurfaceWrapper.h"
#  include "gfxSharedImageSurface.h"
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

struct ReleaseKeepAlive : public nsRunnable
{
  NS_IMETHOD Run()
  {
    mKeep = nullptr;
    return NS_OK;
  }

  UniquePtr<KeepAlive> mKeep;
};

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
    if (mKeep && mMainThreadOnly && !NS_IsMainThread()) {
      RefPtr<ReleaseKeepAlive> release = new ReleaseKeepAlive();
      release->mKeep = Move(mKeep);
      NS_DispatchToMainThread(release);
    }
  }
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(TextureChild)

  TextureChild()
  : mForwarder(nullptr)
  , mMonitor("TextureChild")
  , mTextureClient(nullptr)
  , mDestroyed(false)
  , mMainThreadOnly(false)
  , mIPCOpen(false)
  {
  }

  bool Recv__delete__() override;

  bool RecvCompositorRecycle() override
  {
    RECYCLE_LOG("[CLIENT] Receive recycle %p (%p)\n", mTextureClient, mWaitForRecycle.get());
    mWaitForRecycle = nullptr;
    return true;
  }

  void WaitForCompositorRecycle()
  {
    {
      MonitorAutoLock mon(mMonitor);
      mWaitForRecycle = mDestroyed ? nullptr : mTextureClient;
    }
    RECYCLE_LOG("[CLIENT] Wait for recycle %p\n", mWaitForRecycle.get());
    MOZ_ASSERT(CanSend());
    SendClientRecycle();
  }

  CompositableForwarder* GetForwarder() { return mForwarder; }

  ISurfaceAllocator* GetAllocator() { return mForwarder; }

  void ActorDestroy(ActorDestroyReason why) override;

  bool IPCOpen() const { return mIPCOpen; }

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

  void SetTextureClient(TextureClient* aTextureClient) {
    MonitorAutoLock mon(mMonitor);
    mTextureClient = aTextureClient;
  }

  RefPtr<CompositableForwarder> mForwarder;
  RefPtr<TextureClient> mWaitForRecycle;

  // Monitor protecting mTextureClient.
  Monitor mMonitor;
  TextureClient* mTextureClient;
  UniquePtr<KeepAlive> mKeep;
  Atomic<bool> mDestroyed;
  bool mMainThreadOnly;
  bool mIPCOpen;

  friend class TextureClient;
};

bool
TextureChild::Recv__delete__()
{
  return true;
}

void
TextureChild::ActorDestroy(ActorDestroyReason why)
{
  if (mTextureClient) {
    mTextureClient->mActor = nullptr;
    mTextureClient->mAllocator = nullptr;
  }
  mWaitForRecycle = nullptr;
  mKeep = nullptr;
}

ClientTexture::ClientTexture(TextureData* aData, TextureFlags aFlags, ISurfaceAllocator* aAllocator)
: TextureClient(aAllocator, aFlags)
, mData(aData)
, mOpenMode(OpenMode::OPEN_NONE)
#ifdef DEBUG
, mExpectedDtRefs(0)
#endif
, mIsLocked(false)
{}

bool
ClientTexture::Lock(OpenMode aMode)
{
  MOZ_ASSERT(mValid);
  MOZ_ASSERT(!mIsLocked);
  if (mIsLocked) {
    return mOpenMode == aMode;
  }

  mIsLocked = mData->Lock(aMode, mReleaseFenceHandle.IsValid() ? &mReleaseFenceHandle : nullptr);
  mOpenMode = aMode;

  return mIsLocked;
}

void
ClientTexture::Unlock()
{
  MOZ_ASSERT(mValid);
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
}

bool
ClientTexture::HasInternalBuffer() const
{
  MOZ_ASSERT(mValid);
  return mData->HasInternalBuffer();
}

gfx::IntSize
ClientTexture::GetSize() const
{
  MOZ_ASSERT(mValid);
  return mData->GetSize();
}

gfx::SurfaceFormat
ClientTexture::GetFormat() const
{
  MOZ_ASSERT(mValid);
  return mData->GetFormat();
}

ClientTexture::~ClientTexture()
{
  if (ShouldDeallocateInDestructor()) {
    mData->Deallocate(mAllocator);
  } else {
    mData->Forget(mAllocator);
  }
  delete mData;
}

void
ClientTexture::FinalizeOnIPDLThread()
{
  mData->FinalizeOnIPDLThread(this);
}

void
ClientTexture::UpdateFromSurface(gfx::SourceSurface* aSurface)
{
  MOZ_ASSERT(mValid);
  MOZ_ASSERT(mIsLocked);
  MOZ_ASSERT(aSurface);

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
  NS_WARNING("ClientTexture::UpdateFromSurface failed");
}


already_AddRefed<TextureClient>
ClientTexture::CreateSimilar(TextureFlags aFlags, TextureAllocationFlags aAllocFlags) const
{
  MOZ_ASSERT(mValid);
  TextureData* data = mData->CreateSimilar(mAllocator, aFlags, aAllocFlags);
  if (!data) {
    return nullptr;
  }

  return MakeAndAddRef<ClientTexture>(data, aFlags, mAllocator);
}

gfx::DrawTarget*
ClientTexture::BorrowDrawTarget()
{
  MOZ_ASSERT(mValid);
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
ClientTexture::BorrowMappedData(MappedTextureData& aMap)
{
  MOZ_ASSERT(mValid);

  // TODO - SharedRGBImage just accesses the buffer without properly locking
  // the texture. It's bad.
  //MOZ_ASSERT(mIsLocked);
  //if (!mIsLocked) {
  //  return nullptr;
  //}

  return mData->BorrowMappedData(aMap);
}

bool
ClientTexture::BorrowMappedYCbCrData(MappedYCbCrTextureData& aMap)
{
  MOZ_ASSERT(mValid);
  return mData->BorrowMappedYCbCrData(aMap);
}

bool
ClientTexture::ToSurfaceDescriptor(SurfaceDescriptor& aOutDescriptor)
{
  MOZ_ASSERT(mValid);
  return mData->Serialize(aOutDescriptor);
}

void
ClientTexture::WaitForBufferOwnership(bool aWaitReleaseFence)
{
  if (mRemoveFromCompositableWaiter) {
    mRemoveFromCompositableWaiter->WaitComplete();
    mRemoveFromCompositableWaiter = nullptr;
  }

  if (aWaitReleaseFence && mReleaseFenceHandle.IsValid()) {
    mData->WaitForFence(&mReleaseFenceHandle);
    mReleaseFenceHandle = FenceHandle();
  }
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
TextureClient*
TextureClient::AsTextureClient(PTextureChild* actor)
{
  return actor ? static_cast<TextureChild*>(actor)->mTextureClient : nullptr;
}

bool
TextureClient::IsSharedWithCompositor() const {
  return mShared && mActor && mActor->IPCOpen();
}

void
TextureClient::AddFlags(TextureFlags aFlags)
{
  MOZ_ASSERT(!IsSharedWithCompositor() ||
             ((GetFlags() & TextureFlags::RECYCLE) && !IsAddedToCompositableClient()));
  mFlags |= aFlags;
  if (mValid && mActor && !mActor->mDestroyed && mActor->IPCOpen()) {
    mActor->SendRecycleTexture(mFlags);
  }
}

void
TextureClient::RemoveFlags(TextureFlags aFlags)
{
  MOZ_ASSERT(!IsSharedWithCompositor() ||
             ((GetFlags() & TextureFlags::RECYCLE) && !IsAddedToCompositableClient()));
  mFlags &= ~aFlags;
  if (mValid && mActor && !mActor->mDestroyed && mActor->IPCOpen()) {
    mActor->SendRecycleTexture(mFlags);
  }
}

void
TextureClient::RecycleTexture(TextureFlags aFlags)
{
  MOZ_ASSERT(GetFlags() & TextureFlags::RECYCLE);

  mAddedToCompositableClient = false;
  if (mFlags != aFlags) {
    mFlags = aFlags;
    if (mValid && mActor && !mActor->mDestroyed && mActor->IPCOpen()) {
      mActor->SendRecycleTexture(mFlags);
    }
  }
}

void
TextureClient::WaitForCompositorRecycle()
{
  mActor->WaitForCompositorRecycle();
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
TextureClient::SetRecycleAllocator(TextureClientRecycleAllocator* aAllocator)
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
  MOZ_ASSERT(aForwarder && aForwarder->GetMessageLoop() == mAllocator->GetMessageLoop());
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
  mShared = true;
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
  MOZ_ASSERT(aAllocator->IPCOpen());
  if (!aAllocator || !aAllocator->IPCOpen()) {
    return nullptr;
  }

  if (!gfx::Factory::AllowedSurfaceSize(aSize)) {
    return nullptr;
  }

  LayersBackend parentBackend = aAllocator->GetCompositorBackendType();
  gfx::BackendType moz2DBackend = BackendTypeForBackendSelector(parentBackend, aSelector);

  RefPtr<TextureClient> texture;

#if defined(XP_WIN)
  int32_t maxTextureSize = aAllocator->GetMaxTextureSize();
#endif

#ifdef XP_WIN
  if (parentBackend == LayersBackend::LAYERS_D3D11 &&
      (moz2DBackend == gfx::BackendType::DIRECT2D ||
       moz2DBackend == gfx::BackendType::DIRECT2D1_1) &&
      aSize.width <= maxTextureSize &&
      aSize.height <= maxTextureSize)
  {
    texture = CreateDXGITextureClient(aSize, aFormat, aTextureFlags, aAllocFlags, aAllocator);
    if (texture) {
      return texture.forget();
    }
  }
  if (parentBackend == LayersBackend::LAYERS_D3D9 &&
      moz2DBackend == gfx::BackendType::CAIRO &&
      aAllocator->IsSameProcess() &&
      aSize.width <= maxTextureSize &&
      aSize.height <= maxTextureSize &&
      NS_IsMainThread()) {
    if (gfxWindowsPlatform::GetPlatform()->GetD3D9Device()) {
      TextureData* data = D3D9TextureData::Create(aSize, aFormat, aAllocFlags);
      if (data) {
        return MakeAndAddRef<ClientTexture>(data, aTextureFlags, aAllocator);
      }
    }
  }

  if (!texture && aFormat == SurfaceFormat::B8G8R8X8 &&
      aAllocator->IsSameProcess() &&
      moz2DBackend == gfx::BackendType::CAIRO &&
      NS_IsMainThread()) {
    TextureData* data = DIBTextureData::Create(aSize, aFormat, aAllocator);
    if (data) {
      return MakeAndAddRef<ClientTexture>(data, aTextureFlags, aAllocator);
    }
  }
#endif

#ifdef MOZ_X11
  gfxSurfaceType type =
    gfxPlatform::GetPlatform()->ScreenReferenceSurface()->GetType();

  if (parentBackend == LayersBackend::LAYERS_BASIC &&
      moz2DBackend == gfx::BackendType::CAIRO &&
      type == gfxSurfaceType::Xlib)
  {
    texture = CreateX11TextureClient(aSize, aFormat, aTextureFlags, aAllocator);
    if (texture) {
      return texture.forget();
    }
  }
#ifdef GL_PROVIDER_GLX
  if (parentBackend == LayersBackend::LAYERS_OPENGL &&
      type == gfxSurfaceType::Xlib &&
      aFormat != SurfaceFormat::A8 &&
      gl::sGLXLibrary.UseTextureFromPixmap())
  {
    texture = CreateX11TextureClient(aSize, aFormat, aTextureFlags, aAllocator);
    if (texture) {
      return texture.forget();
    }
  }
#endif
#endif

#ifdef MOZ_WIDGET_GONK
  texture = CreateGrallocTextureClientForDrawing(aSize, aFormat, moz2DBackend,
                                                 aTextureFlags, aAllocator);
  if (texture) {
    return texture.forget();
  }
#endif

  MOZ_ASSERT(!texture || texture->CanExposeDrawTarget(), "texture cannot expose a DrawTarget?");

  if (texture && texture->AllocateForSurface(aSize, aAllocFlags)) {
    return texture.forget();
  }

  if (aAllocFlags & ALLOC_DISALLOW_BUFFERTEXTURECLIENT) {
    return nullptr;
  }

  if (texture) {
    NS_WARNING("Failed to allocate a TextureClient, falling back to BufferTextureClient.");
  }

  // Can't do any better than a buffer texture client.
  return TextureClient::CreateForRawBufferAccess(aAllocator, aFormat, aSize,
                                                 moz2DBackend, aTextureFlags, aAllocFlags);
}

// static
already_AddRefed<TextureClient>
TextureClient::CreateForRawBufferAccess(ISurfaceAllocator* aAllocator,
                                        gfx::SurfaceFormat aFormat,
                                        gfx::IntSize aSize,
                                        gfx::BackendType aMoz2DBackend,
                                        TextureFlags aTextureFlags,
                                        TextureAllocationFlags aAllocFlags)
{
  MOZ_ASSERT(aAllocator->IPCOpen());
  if (!aAllocator || !aAllocator->IPCOpen()) {
    return nullptr;
  }

  if (!gfx::Factory::AllowedSurfaceSize(aSize)) {
    return nullptr;
  }

  TextureData* texData = BufferTextureData::Create(aSize, aFormat, aMoz2DBackend,
                                                   aTextureFlags, aAllocFlags,
                                                   aAllocator);
  if (!texData) {
    return nullptr;
  }

  return MakeAndAddRef<ClientTexture>(texData, aTextureFlags, aAllocator);
}

// static
already_AddRefed<TextureClient>
TextureClient::CreateForYCbCr(ISurfaceAllocator* aAllocator,
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

  return MakeAndAddRef<ClientTexture>(data, aTextureFlags, aAllocator);
}

// static
already_AddRefed<TextureClient>
TextureClient::CreateWithBufferSize(ISurfaceAllocator* aAllocator,
                                    gfx::SurfaceFormat aFormat,
                                    size_t aSize,
                                    TextureFlags aTextureFlags)
{
  MOZ_ASSERT(aAllocator->IPCOpen());
  if (!aAllocator || !aAllocator->IPCOpen()) {
    return nullptr;
  }

  TextureData* data = BufferTextureData::CreateWithBufferSize(aAllocator, aFormat, aSize,
                                                              aTextureFlags);
  if (!data) {
    return nullptr;
  }

  return MakeAndAddRef<ClientTexture>(data, aTextureFlags, aAllocator);
}

TextureClient::TextureClient(ISurfaceAllocator* aAllocator, TextureFlags aFlags)
  : mAllocator(aAllocator)
  , mFlags(aFlags)
  , mShared(false)
  , mValid(true)
  , mAddedToCompositableClient(false)
#ifdef GFX_DEBUG_TRACK_CLIENTS_IN_POOL
  , mPoolTracker(nullptr)
#endif
{}

TextureClient::~TextureClient()
{
  // All the destruction code that may lead to virtual method calls must
  // be in Finalize() which is called just before the destructor.
}

void
TextureClient::KeepUntilFullDeallocation(UniquePtr<KeepAlive> aKeep, bool aMainThreadOnly)
{
  MOZ_ASSERT(mActor);
  MOZ_ASSERT(!mActor->mKeep);
  mActor->mKeep = Move(aKeep);
  mActor->mMainThreadOnly = aMainThreadOnly;
}

void TextureClient::ForceRemove(bool sync)
{
  if (mActor && mActor->mDestroyed) {
    mActor = nullptr;
  }
  if (mValid && mActor) {
    FinalizeOnIPDLThread();
    if (mActor->CanSend()) {
      if (sync || GetFlags() & TextureFlags::DEALLOCATE_CLIENT) {
        mActor->DestroySynchronously();
      } else {
        mActor->Destroy();
      }
    }
  }
  MarkInvalid();
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
TextureClient::Finalize()
{
  MOZ_ASSERT(!IsLocked());

  // Always make a temporary strong reference to the actor before we use it,
  // in case TextureChild::ActorDestroy might null mActor concurrently.
  RefPtr<TextureChild> actor = mActor;

  if (actor) {
    if (actor->mDestroyed) {
      actor = nullptr;
      return;
    }
    // The actor has a raw pointer to us, actor->mTextureClient.
    // Null it before RemoveTexture calls to avoid invalid actor->mTextureClient
    // when calling TextureChild::ActorDestroy()
    actor->SetTextureClient(nullptr);

    // `actor->mWaitForRecycle` may not be null, as we may be being called from setting
    // this RefPtr to null! Clearing it here will double-Release() it.

    // this will call ForceRemove in the right thread, using a sync proxy if needed
    if (actor->GetForwarder()) {
      actor->GetForwarder()->RemoveTexture(this);
    }
  }
}

bool
TextureClient::ShouldDeallocateInDestructor() const
{
  if (!IsAllocated()) {
    return false;
  }

  // If we're meant to be deallocated by the host,
  // but we haven't been shared yet or
  // TextureFlags::DEALLOCATE_CLIENT is set, then we should
  // deallocate on the client instead.
  return !mShared || (GetFlags() & TextureFlags::DEALLOCATE_CLIENT);
}

void
TextureClient::SetRemoveFromCompositableWaiter(AsyncTransactionWaiter* aWaiter) {
  mRemoveFromCompositableWaiter = aWaiter;
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
TextureClient::CreateWithData(TextureData* aData, TextureFlags aFlags, ISurfaceAllocator* aAllocator)
{
  if (!aData) {
    return nullptr;
  }
  return MakeAndAddRef<ClientTexture>(aData, aFlags, aAllocator);
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
