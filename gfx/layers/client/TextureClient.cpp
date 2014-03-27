/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/TextureClient.h"
#include <stdint.h>                     // for uint8_t, uint32_t, etc
#include "Layers.h"                     // for Layer, etc
#include "gfx2DGlue.h"
#include "gfxContext.h"                 // for gfxContext, etc
#include "gfxPlatform.h"                // for gfxPlatform
#include "gfxPoint.h"                   // for gfxIntSize, gfxSize
#include "gfxReusableSurfaceWrapper.h"  // for gfxReusableSurfaceWrapper
#include "mozilla/gfx/BaseSize.h"       // for BaseSize
#include "mozilla/ipc/SharedMemory.h"   // for SharedMemory, etc
#include "mozilla/layers/CompositableClient.h"  // for CompositableClient
#include "mozilla/layers/CompositableForwarder.h"
#include "mozilla/layers/ISurfaceAllocator.h"
#include "mozilla/layers/ImageDataSerializer.h"
#include "mozilla/layers/ShadowLayers.h"  // for ShadowLayerForwarder
#include "mozilla/layers/SharedPlanarYCbCrImage.h"
#include "mozilla/layers/YCbCrImageDataSerializer.h"
#include "nsDebug.h"                    // for NS_ASSERTION, NS_WARNING, etc
#include "nsISupportsImpl.h"            // for MOZ_COUNT_CTOR, etc
#include "ImageContainer.h"             // for PlanarYCbCrImage, etc
#include "mozilla/gfx/2D.h"
#include "mozilla/layers/TextureClientOGL.h"

#ifdef XP_WIN
#include "mozilla/layers/TextureD3D9.h"
#include "mozilla/layers/TextureD3D11.h"
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

#ifdef MOZ_ANDROID_OMTC
#  include "gfxReusableImageSurfaceWrapper.h"
#  include "gfxImageSurface.h"
#else
#  include "gfxReusableSharedImageSurfaceWrapper.h"
#  include "gfxSharedImageSurface.h"
#endif

#if 0
#define RECYCLE_LOG(...) printf_stderr(__VA_ARGS__)
#else
#define RECYCLE_LOG(...) do { } while (0)
#endif

using namespace mozilla::gl;
using namespace mozilla::gfx;

namespace mozilla {
namespace layers {

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
class TextureChild : public PTextureChild
                   , public AtomicRefCounted<TextureChild>
{
public:
  MOZ_DECLARE_REFCOUNTED_TYPENAME(TextureChild)
  TextureChild()
  : mForwarder(nullptr)
  , mTextureData(nullptr)
  , mTextureClient(nullptr)
  , mIPCOpen(false)
  {
    MOZ_COUNT_CTOR(TextureChild);
  }

  ~TextureChild()
  {
    MOZ_COUNT_DTOR(TextureChild);
  }

  bool Recv__delete__() MOZ_OVERRIDE;

  bool RecvCompositorRecycle(const MaybeFenceHandle& aFence)
  {
    RECYCLE_LOG("Receive recycle %p (%p)\n", mTextureClient, mWaitForRecycle.get());
    if (aFence.type() != aFence.Tnull_t) {
      FenceHandle fence = aFence.get_FenceHandle();
      if (fence.IsValid() && mTextureClient) {
        mTextureClient->SetReleaseFenceHandle(aFence);
        // HWC might not provide Fence.
        // In this case, HWC implicitly handles buffer's fence.
      }
    }
    mWaitForRecycle = nullptr;
    return true;
  }

  void WaitForCompositorRecycle()
  {
    mWaitForRecycle = mTextureClient;
    RECYCLE_LOG("Wait for recycle %p\n", mWaitForRecycle.get());
    SendClientRecycle();
  }

  /**
   * Only used during the deallocation phase iff we need synchronization between
   * the client and host side for deallocation (that is, when the data is going
   * to be deallocated or recycled on the client side).
   */
  void SetTextureData(TextureClientData* aData)
  {
    mTextureData = aData;
  }

  void DeleteTextureData();

  CompositableForwarder* GetForwarder() { return mForwarder; }

  ISurfaceAllocator* GetAllocator() { return mForwarder; }

  void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE;

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

  RefPtr<CompositableForwarder> mForwarder;
  RefPtr<TextureClient> mWaitForRecycle;
  TextureClientData* mTextureData;
  TextureClient* mTextureClient;
  bool mIPCOpen;

  friend class TextureClient;
};

void
TextureChild::DeleteTextureData()
{
  mWaitForRecycle = nullptr;
  if (mTextureData) {
    mTextureData->DeallocateSharedData(GetAllocator());
    delete mTextureData;
    mTextureData = nullptr;
  }
}

bool
TextureChild::Recv__delete__()
{
  DeleteTextureData();
  return true;
}

void
TextureChild::ActorDestroy(ActorDestroyReason why)
{
  if (mTextureClient) {
    mTextureClient->mActor = nullptr;
  }
  mWaitForRecycle = nullptr;
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

void
TextureClient::WaitForCompositorRecycle()
{
  mActor->WaitForCompositorRecycle();
}

bool
TextureClient::InitIPDLActor(CompositableForwarder* aForwarder)
{
  MOZ_ASSERT(aForwarder);
  if (mActor && mActor->GetForwarder() == aForwarder) {
    return true;
  }
  MOZ_ASSERT(!mActor, "Cannot use a texture on several IPC channels.");

  SurfaceDescriptor desc;
  if (!ToSurfaceDescriptor(desc)) {
    return false;
  }

  mActor = static_cast<TextureChild*>(aForwarder->CreateTexture(desc, GetFlags()));
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

#ifdef MOZ_WIDGET_GONK
static bool
DisableGralloc(SurfaceFormat aFormat)
{
  if (aFormat == gfx::SurfaceFormat::A8) {
    return true;
  }

  return false;
}
#endif

// static
TemporaryRef<TextureClient>
TextureClient::CreateTextureClientForDrawing(ISurfaceAllocator* aAllocator,
                                             SurfaceFormat aFormat,
                                             TextureFlags aTextureFlags,
                                             gfx::BackendType aMoz2DBackend,
                                             const gfx::IntSize& aSizeHint)
{
  if (aMoz2DBackend == gfx::BackendType::NONE) {
    aMoz2DBackend = gfxPlatform::GetPlatform()->GetContentBackend();
  }

  RefPtr<TextureClient> result;

#ifdef XP_WIN
  LayersBackend parentBackend = aAllocator->GetCompositorBackendType();
  if (parentBackend == LayersBackend::LAYERS_D3D11 &&
      (aMoz2DBackend == gfx::BackendType::DIRECT2D ||
        aMoz2DBackend == gfx::BackendType::DIRECT2D1_1) &&
      gfxWindowsPlatform::GetPlatform()->GetD2DDevice() &&

      !(aTextureFlags & TEXTURE_ALLOC_FALLBACK)) {
    result = new TextureClientD3D11(aFormat, aTextureFlags);
  }
  if (parentBackend == LayersBackend::LAYERS_D3D9 &&
      aMoz2DBackend == gfx::BackendType::CAIRO &&
      aAllocator->IsSameProcess() &&
      !(aTextureFlags & TEXTURE_ALLOC_FALLBACK)) {
    if (!gfxWindowsPlatform::GetPlatform()->GetD3D9Device()) {
      result = new DIBTextureClientD3D9(aFormat, aTextureFlags);
    } else {
      result = new CairoTextureClientD3D9(aFormat, aTextureFlags);
    }
  }
#endif

#ifdef MOZ_X11
  LayersBackend parentBackend = aAllocator->GetCompositorBackendType();
  gfxSurfaceType type =
    gfxPlatform::GetPlatform()->ScreenReferenceSurface()->GetType();

  if (parentBackend == LayersBackend::LAYERS_BASIC &&
      aMoz2DBackend == gfx::BackendType::CAIRO &&
      type == gfxSurfaceType::Xlib &&
      !(aTextureFlags & TEXTURE_ALLOC_FALLBACK))
  {
    result = new TextureClientX11(aFormat, aTextureFlags);
  }
#ifdef GL_PROVIDER_GLX
#if 0
  // Bug 977963: Disabled for black layers
  if (parentBackend == LayersBackend::LAYERS_OPENGL &&
      type == gfxSurfaceType::Xlib &&
      !(aTextureFlags & TEXTURE_ALLOC_FALLBACK) &&
      aFormat != SurfaceFormat::A8 &&
      gl::sGLXLibrary.UseTextureFromPixmap())
  {
    result = new TextureClientX11(aFormat, aTextureFlags);
  }
#endif
#endif
#endif

#ifdef MOZ_WIDGET_GONK
  if (!DisableGralloc(aFormat)) {
    // Don't allow Gralloc texture clients to exceed the maximum texture size.
    // BufferTextureClients have code to handle tiling the surface client-side.
    int32_t maxTextureSize = aAllocator->GetMaxTextureSize();
    if (aSizeHint.width <= maxTextureSize && aSizeHint.height <= maxTextureSize) {
      result = new GrallocTextureClientOGL(aAllocator, aFormat, aMoz2DBackend,
                                           aTextureFlags);
    }
  }
#endif

  // Can't do any better than a buffer texture client.
  if (!result) {
    result = CreateBufferTextureClient(aAllocator, aFormat, aTextureFlags, aMoz2DBackend);
  }

  MOZ_ASSERT(!result || result->AsTextureClientDrawTarget(),
             "Not a TextureClientDrawTarget?");
  return result;
}

// static
TemporaryRef<BufferTextureClient>
TextureClient::CreateBufferTextureClient(ISurfaceAllocator* aAllocator,
                                         SurfaceFormat aFormat,
                                         TextureFlags aTextureFlags,
                                         gfx::BackendType aMoz2DBackend)
{
  if (gfxPlatform::GetPlatform()->PreferMemoryOverShmem()) {
    RefPtr<BufferTextureClient> result = new MemoryTextureClient(aAllocator, aFormat,
                                                                 aMoz2DBackend,
                                                                 aTextureFlags);
    return result.forget();
  }
  RefPtr<BufferTextureClient> result = new ShmemTextureClient(aAllocator, aFormat,
                                                              aMoz2DBackend,
                                                              aTextureFlags);
  return result.forget();
}


class ShmemTextureClientData : public TextureClientData
{
public:
  ShmemTextureClientData(ipc::Shmem& aShmem)
  : mShmem(aShmem)
  {
    MOZ_COUNT_CTOR(ShmemTextureClientData);
  }

  ~ShmemTextureClientData()
  {
    MOZ_COUNT_CTOR(ShmemTextureClientData);
  }

  virtual void DeallocateSharedData(ISurfaceAllocator* allocator)
  {
    allocator->DeallocShmem(mShmem);
    mShmem = ipc::Shmem();
  }

private:
  ipc::Shmem mShmem;
};

class MemoryTextureClientData : public TextureClientData
{
public:
  MemoryTextureClientData(uint8_t* aBuffer)
  : mBuffer(aBuffer)
  {
    MOZ_COUNT_CTOR(MemoryTextureClientData);
  }

  ~MemoryTextureClientData()
  {
    MOZ_ASSERT(!mBuffer, "Forgot to deallocate the shared texture data?");
    MOZ_COUNT_DTOR(MemoryTextureClientData);
  }

  virtual void DeallocateSharedData(ISurfaceAllocator*)
  {
    delete[] mBuffer;
    mBuffer = nullptr;
  }

private:
  uint8_t* mBuffer;
};

TextureClientData*
MemoryTextureClient::DropTextureData()
{
  if (!mBuffer) {
    return nullptr;
  }
  TextureClientData* result = new MemoryTextureClientData(mBuffer);
  MarkInvalid();
  mBuffer = nullptr;
  return result;
}

TextureClientData*
ShmemTextureClient::DropTextureData()
{
  if (!mShmem.IsReadable()) {
    return nullptr;
  }
  TextureClientData* result = new ShmemTextureClientData(mShmem);
  MarkInvalid();
  mShmem = ipc::Shmem();
  return result;
}

TextureClient::TextureClient(TextureFlags aFlags)
  : mFlags(aFlags)
  , mShared(false)
  , mValid(true)
{}

TextureClient::~TextureClient()
{
  // All the destruction code that may lead to virtual method calls must
  // be in Finalize() which is called just before the destructor.
}

void TextureClient::ForceRemove()
{
  if (mValid && mActor) {
    if (GetFlags() & TEXTURE_DEALLOCATE_CLIENT) {
      mActor->SetTextureData(DropTextureData());
      if (mActor->IPCOpen()) {
        mActor->SendRemoveTextureSync();
      }
      mActor->DeleteTextureData();
    } else {
      if (mActor->IPCOpen()) {
        mActor->SendRemoveTexture();
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

  if (!aTarget->AsTextureClientDrawTarget() || !AsTextureClientDrawTarget()) {
    return false;
  }

  RefPtr<DrawTarget> destinationTarget = aTarget->AsTextureClientDrawTarget()->GetAsDrawTarget();
  RefPtr<DrawTarget> sourceTarget = AsTextureClientDrawTarget()->GetAsDrawTarget();
  RefPtr<gfx::SourceSurface> source = sourceTarget->Snapshot();
  destinationTarget->CopySurface(source,
                                 aRect ? *aRect : gfx::IntRect(gfx::IntPoint(0, 0), GetSize()),
                                 aPoint ? *aPoint : gfx::IntPoint(0, 0));
  destinationTarget = nullptr;
  source = nullptr;
  sourceTarget = nullptr;

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
    // this will call ForceRemove in the right thread, using a sync proxy if needed
    if (actor->GetForwarder()) {
      actor->GetForwarder()->RemoveTexture(this);
    }
    // The actor has a raw pointer to us, actor->mTextureClient. Null it before we die.
    actor->mTextureClient = nullptr;
  }
}

bool
TextureClient::ShouldDeallocateInDestructor() const
{
  if (!IsAllocated()) {
    return false;
  }

  // If we're meant to be deallocated by the host,
  // but we haven't been shared yet, then we should
  // deallocate on the client instead.
  return !IsSharedWithCompositor();
}

bool
ShmemTextureClient::ToSurfaceDescriptor(SurfaceDescriptor& aDescriptor)
{
  MOZ_ASSERT(IsValid());
  if (!IsAllocated() || GetFormat() == gfx::SurfaceFormat::UNKNOWN) {
    return false;
  }

  aDescriptor = SurfaceDescriptorShmem(mShmem, GetFormat());

  return true;
}

bool
ShmemTextureClient::Allocate(uint32_t aSize)
{
  MOZ_ASSERT(mValid);
  ipc::SharedMemory::SharedMemoryType memType = OptimalShmemType();
  mAllocated = GetAllocator()->AllocUnsafeShmem(aSize, memType, &mShmem);
  return mAllocated;
}

uint8_t*
ShmemTextureClient::GetBuffer() const
{
  MOZ_ASSERT(IsValid());
  if (mAllocated) {
    return mShmem.get<uint8_t>();
  }
  return nullptr;
}

size_t
ShmemTextureClient::GetBufferSize() const
{
  MOZ_ASSERT(IsValid());
  return mShmem.Size<uint8_t>();
}

ShmemTextureClient::ShmemTextureClient(ISurfaceAllocator* aAllocator,
                                       gfx::SurfaceFormat aFormat,
                                       gfx::BackendType aMoz2DBackend,
                                       TextureFlags aFlags)
  : BufferTextureClient(aAllocator, aFormat, aMoz2DBackend, aFlags)
  , mAllocated(false)
{
  MOZ_COUNT_CTOR(ShmemTextureClient);
}

ShmemTextureClient::~ShmemTextureClient()
{
  MOZ_COUNT_DTOR(ShmemTextureClient);
  if (ShouldDeallocateInDestructor()) {
    // if the buffer has never been shared we must deallocate it or ir would
    // leak.
    GetAllocator()->DeallocShmem(mShmem);
  }
}

bool
MemoryTextureClient::ToSurfaceDescriptor(SurfaceDescriptor& aDescriptor)
{
  MOZ_ASSERT(IsValid());
  if (!IsAllocated() || GetFormat() == gfx::SurfaceFormat::UNKNOWN) {
    return false;
  }
  aDescriptor = SurfaceDescriptorMemory(reinterpret_cast<uintptr_t>(mBuffer),
                                        GetFormat());
  return true;
}

bool
MemoryTextureClient::Allocate(uint32_t aSize)
{
  MOZ_ASSERT(!mBuffer);
  static const fallible_t fallible = fallible_t();
  mBuffer = new(fallible) uint8_t[aSize];
  if (!mBuffer) {
    NS_WARNING("Failed to allocate buffer");
    return false;
  }
  GfxMemoryImageReporter::DidAlloc(mBuffer);
  mBufSize = aSize;
  return true;
}

MemoryTextureClient::MemoryTextureClient(ISurfaceAllocator* aAllocator,
                                         gfx::SurfaceFormat aFormat,
                                         gfx::BackendType aMoz2DBackend,
                                         TextureFlags aFlags)
  : BufferTextureClient(aAllocator, aFormat, aMoz2DBackend, aFlags)
  , mBuffer(nullptr)
  , mBufSize(0)
{
  MOZ_COUNT_CTOR(MemoryTextureClient);
}

MemoryTextureClient::~MemoryTextureClient()
{
  MOZ_COUNT_DTOR(MemoryTextureClient);
  if (mBuffer && ShouldDeallocateInDestructor()) {
    // if the buffer has never been shared we must deallocate it or it would
    // leak.
    GfxMemoryImageReporter::WillFree(mBuffer);
    delete [] mBuffer;
  }
}

BufferTextureClient::BufferTextureClient(ISurfaceAllocator* aAllocator,
                                         gfx::SurfaceFormat aFormat,
                                         gfx::BackendType aMoz2DBackend,
                                         TextureFlags aFlags)
  : TextureClient(aFlags)
  , mAllocator(aAllocator)
  , mFormat(aFormat)
  , mBackend(aMoz2DBackend)
  , mOpenMode(0)
  , mUsingFallbackDrawTarget(false)
  , mLocked(false)
{}

BufferTextureClient::~BufferTextureClient()
{}

ISurfaceAllocator*
BufferTextureClient::GetAllocator() const
{
  return mAllocator;
}

bool
BufferTextureClient::UpdateSurface(gfxASurface* aSurface)
{
  MOZ_ASSERT(mLocked);
  MOZ_ASSERT(aSurface);
  MOZ_ASSERT(!IsImmutable());
  MOZ_ASSERT(IsValid());

  ImageDataSerializer serializer(GetBuffer(), GetBufferSize());
  if (!serializer.IsValid()) {
    return false;
  }

  RefPtr<DrawTarget> dt = GetAsDrawTarget();
  RefPtr<SourceSurface> source = gfxPlatform::GetPlatform()->GetSourceSurfaceForSurface(dt, aSurface);

  dt->CopySurface(source, IntRect(IntPoint(), serializer.GetSize()), IntPoint());
  // XXX - if the Moz2D backend is D2D, we would be much better off memcpying
  // the content of the surface directly because with D2D, GetAsDrawTarget is
  // very expensive.

  if (TextureRequiresLocking(mFlags) && !ImplementsLocking()) {
    // We don't have support for proper locking yet, so we'll
    // have to be immutable instead.
    MarkImmutable();
  }
  return true;
}

already_AddRefed<gfxASurface>
BufferTextureClient::GetAsSurface()
{
  MOZ_ASSERT(mLocked);
  MOZ_ASSERT(IsValid());

  ImageDataSerializer serializer(GetBuffer(), GetBufferSize());
  if (!serializer.IsValid()) {
    return nullptr;
  }

  RefPtr<gfxImageSurface> surf = serializer.GetAsThebesSurface();
  nsRefPtr<gfxASurface> result = surf.get();
  return result.forget();
}

bool
BufferTextureClient::AllocateForSurface(gfx::IntSize aSize, TextureAllocationFlags aFlags)
{
  MOZ_ASSERT(IsValid());
  MOZ_ASSERT(mFormat != gfx::SurfaceFormat::YUV, "This textureClient cannot use YCbCr data");
  MOZ_ASSERT(aSize.width * aSize.height);

  int bufSize
    = ImageDataSerializer::ComputeMinBufferSize(aSize, mFormat);
  if (!Allocate(bufSize)) {
    return false;
  }

  if (aFlags & ALLOC_CLEAR_BUFFER) {
    memset(GetBuffer(), 0, bufSize);
  }

  ImageDataSerializer serializer(GetBuffer(), GetBufferSize());
  serializer.InitializeBufferInfo(aSize, mFormat);
  mSize = aSize;
  return true;
}

TemporaryRef<gfx::DrawTarget>
BufferTextureClient::GetAsDrawTarget()
{
  MOZ_ASSERT(IsValid());
  MOZ_ASSERT(mLocked, "GetAsDrawTarget should be called on locked textures only");

  if (mDrawTarget) {
    return mDrawTarget;
  }

  ImageDataSerializer serializer(GetBuffer(), GetBufferSize());
  if (!serializer.IsValid()) {
    return nullptr;
  }

  MOZ_ASSERT(mUsingFallbackDrawTarget == false);
  mDrawTarget = serializer.GetAsDrawTarget(mBackend);
  if (mDrawTarget) {
    return mDrawTarget;
  }

  // fallback path, probably because the Moz2D backend can't create a
  // DrawTarget around raw memory. This is going to be slow :(
  mDrawTarget = gfx::Factory::CreateDrawTarget(mBackend, serializer.GetSize(),
                                               serializer.GetFormat());
  if (!mDrawTarget) {
    return nullptr;
  }

  mUsingFallbackDrawTarget = true;
  if (mOpenMode & OPEN_READ) {
    RefPtr<DataSourceSurface> surface = serializer.GetAsSurface();
    IntRect rect(0, 0, surface->GetSize().width, surface->GetSize().height);
    mDrawTarget->CopySurface(surface, rect, IntPoint(0,0));
  }
  return mDrawTarget;
}

bool
BufferTextureClient::Lock(OpenMode aMode)
{
  MOZ_ASSERT(!mLocked, "The TextureClient is already Locked!");
  mOpenMode = aMode;
  mLocked = IsValid() && IsAllocated();;
  return mLocked;
}

void
BufferTextureClient::Unlock()
{
  MOZ_ASSERT(mLocked, "The TextureClient is already Unlocked!");
  mLocked = false;
  if (!mDrawTarget) {
    mUsingFallbackDrawTarget = false;
    return;
  }

  // see the comment on TextureClientDrawTarget::GetAsDrawTarget.
  // This DrawTarget is internal to the TextureClient and is only exposed to the
  // outside world between Lock() and Unlock(). This assertion checks that no outside
  // reference remains by the time Unlock() is called.
  MOZ_ASSERT(mDrawTarget->refCount() == 1);

  mDrawTarget->Flush();
  if (mUsingFallbackDrawTarget && (mOpenMode & OPEN_WRITE)) {
    // When we are using a fallback DrawTarget, it means we could not create
    // a DrawTarget wrapping the TextureClient's shared memory. In this scenario
    // we need to put the content of the fallback draw target back into our shared
    // memory.
    RefPtr<SourceSurface> snapshot = mDrawTarget->Snapshot();
    RefPtr<DataSourceSurface> surface = snapshot->GetDataSurface();
    ImageDataSerializer serializer(GetBuffer(), GetBufferSize());
    if (!serializer.IsValid() || serializer.GetSize() != surface->GetSize()) {
      NS_WARNING("Could not write the data back into the texture.");
      mDrawTarget = nullptr;
      mUsingFallbackDrawTarget = false;
      return;
    }
    MOZ_ASSERT(surface->GetSize() == serializer.GetSize());
    MOZ_ASSERT(surface->GetFormat() == serializer.GetFormat());
    int bpp = BytesPerPixel(surface->GetFormat());
    for (int i = 0; i < surface->GetSize().height; ++i) {
      memcpy(serializer.GetData() + i*serializer.GetStride(),
             surface->GetData() + i*surface->Stride(),
             surface->GetSize().width * bpp);
    }
  }
  mDrawTarget = nullptr;
  mUsingFallbackDrawTarget = false;
}

bool
BufferTextureClient::UpdateYCbCr(const PlanarYCbCrData& aData)
{
  MOZ_ASSERT(mLocked);
  MOZ_ASSERT(mFormat == gfx::SurfaceFormat::YUV, "This textureClient can only use YCbCr data");
  MOZ_ASSERT(!IsImmutable());
  MOZ_ASSERT(IsValid());
  MOZ_ASSERT(aData.mCbSkip == aData.mCrSkip);

  YCbCrImageDataSerializer serializer(GetBuffer(), GetBufferSize());
  MOZ_ASSERT(serializer.IsValid());
  if (!serializer.CopyData(aData.mYChannel, aData.mCbChannel, aData.mCrChannel,
                           aData.mYSize, aData.mYStride,
                           aData.mCbCrSize, aData.mCbCrStride,
                           aData.mYSkip, aData.mCbSkip)) {
    NS_WARNING("Failed to copy image data!");
    return false;
  }

  if (TextureRequiresLocking(mFlags)) {
    // We don't have support for proper locking yet, so we'll
    // have to be immutable instead.
    MarkImmutable();
  }
  return true;
}

bool
BufferTextureClient::AllocateForYCbCr(gfx::IntSize aYSize,
                                      gfx::IntSize aCbCrSize,
                                      StereoMode aStereoMode)
{
  MOZ_ASSERT(IsValid());

  size_t bufSize = YCbCrImageDataSerializer::ComputeMinBufferSize(aYSize,
                                                                  aCbCrSize);
  if (!Allocate(bufSize)) {
    return false;
  }
  YCbCrImageDataSerializer serializer(GetBuffer(), GetBufferSize());
  serializer.InitializeBufferInfo(aYSize,
                                  aCbCrSize,
                                  aStereoMode);
  mSize = aYSize;
  return true;
}









DeprecatedTextureClient::DeprecatedTextureClient(CompositableForwarder* aForwarder,
                             const TextureInfo& aTextureInfo)
  : mForwarder(aForwarder)
  , mTextureInfo(aTextureInfo)
  , mAccessMode(ACCESS_READ_WRITE)
{
  MOZ_COUNT_CTOR(DeprecatedTextureClient);
}

DeprecatedTextureClient::~DeprecatedTextureClient()
{
  MOZ_COUNT_DTOR(DeprecatedTextureClient);
  MOZ_ASSERT(mDescriptor.type() == SurfaceDescriptor::T__None, "Need to release surface!");
}

DeprecatedTextureClientShmem::DeprecatedTextureClientShmem(CompositableForwarder* aForwarder,
                                       const TextureInfo& aTextureInfo)
  : DeprecatedTextureClient(aForwarder, aTextureInfo)
{
}

DeprecatedTextureClientShmem::~DeprecatedTextureClientShmem()
{
  ReleaseResources();
}

void
DeprecatedTextureClientShmem::ReleaseResources()
{
  if (mSurface) {
    mSurface = nullptr;
    mSurfaceAsImage = nullptr;

    ShadowLayerForwarder::CloseDescriptor(mDescriptor);
  }

  if (!(mTextureInfo.mTextureFlags & TEXTURE_DEALLOCATE_CLIENT)) {
    mDescriptor = SurfaceDescriptor();
    return;
  }

  if (IsSurfaceDescriptorValid(mDescriptor)) {
    mForwarder->DestroySharedSurface(&mDescriptor);
    mDescriptor = SurfaceDescriptor();
  }
}

bool
DeprecatedTextureClientShmem::EnsureAllocated(gfx::IntSize aSize,
                                    gfxContentType aContentType)
{
  if (aSize != mSize ||
      aContentType != mContentType ||
      !IsSurfaceDescriptorValid(mDescriptor)) {
    ReleaseResources();

    mContentType = aContentType;
    mSize = aSize;

    if (!mForwarder->AllocSurfaceDescriptor(mSize, mContentType,
                                            &mDescriptor)) {
      NS_WARNING("creating SurfaceDescriptor failed!");
    }
    if (mContentType == gfxContentType::COLOR_ALPHA) {
      gfxASurface* surface = GetSurface();
      if (!surface) {
        return false;
      }
      nsRefPtr<gfxContext> context = new gfxContext(surface);
      context->SetColor(gfxRGBA(0, 0, 0, 0));
      context->SetOperator(gfxContext::OPERATOR_SOURCE);
      context->Paint();
    }
  }
  return true;
}

void
DeprecatedTextureClientShmem::SetDescriptor(const SurfaceDescriptor& aDescriptor)
{
  if (aDescriptor.type() == SurfaceDescriptor::Tnull_t) {
    EnsureAllocated(mSize, mContentType);
    return;
  }

  ReleaseResources();
  mDescriptor = aDescriptor;

  MOZ_ASSERT(!mSurface);
  NS_ASSERTION(mDescriptor.type() == SurfaceDescriptor::T__None ||
               mDescriptor.type() == SurfaceDescriptor::TSurfaceDescriptorGralloc ||
               mDescriptor.type() == SurfaceDescriptor::TShmem ||
               mDescriptor.type() == SurfaceDescriptor::TMemoryImage ||
               mDescriptor.type() == SurfaceDescriptor::TRGBImage,
               "Invalid surface descriptor");
}

gfxASurface*
DeprecatedTextureClientShmem::GetSurface()
{
  if (!mSurface) {
    if (!IsSurfaceDescriptorValid(mDescriptor)) {
      return nullptr;
    }
    MOZ_ASSERT(mAccessMode == ACCESS_READ_WRITE || mAccessMode == ACCESS_READ_ONLY);
    OpenMode mode = mAccessMode == ACCESS_READ_WRITE
                    ? OPEN_READ_WRITE
                    : OPEN_READ_ONLY;
    mSurface = ShadowLayerForwarder::OpenDescriptor(mode, mDescriptor);
    MOZ_ASSERT(!mSurface || mSurface->GetContentType() == mContentType);
  }

  return mSurface.get();
}


gfx::DrawTarget*
DeprecatedTextureClientShmem::LockDrawTarget()
{
  if (mDrawTarget) {
    return mDrawTarget;
  }

  gfxASurface* surface = GetSurface();
  if (!surface) {
    return nullptr;
  }

  mDrawTarget = gfxPlatform::GetPlatform()->CreateDrawTargetForSurface(surface, mSize);

  return mDrawTarget;
}

void
DeprecatedTextureClientShmem::Unlock()
{
  if (mSurface) {
    mSurface = nullptr;
    mSurfaceAsImage = nullptr;
    ShadowLayerForwarder::CloseDescriptor(mDescriptor);
  }
  mDrawTarget = nullptr;
}

gfxImageSurface*
DeprecatedTextureClientShmem::LockImageSurface()
{
  if (!mSurfaceAsImage) {
    gfxASurface* surface = GetSurface();
    if (!surface) {
      return nullptr;
    }
    mSurfaceAsImage = surface->GetAsImageSurface();
  }

  return mSurfaceAsImage.get();
}

DeprecatedTextureClientTile::DeprecatedTextureClientTile(const DeprecatedTextureClientTile& aOther)
  : DeprecatedTextureClient(aOther.mForwarder, aOther.mTextureInfo)
  , mSurface(aOther.mSurface)
{}

DeprecatedTextureClientTile::~DeprecatedTextureClientTile()
{}

void
DeprecatedTextureClientShmemYCbCr::ReleaseResources()
{
  GetForwarder()->DestroySharedSurface(&mDescriptor);
}

void
DeprecatedTextureClientShmemYCbCr::SetDescriptor(const SurfaceDescriptor& aDescriptor)
{
  MOZ_ASSERT(aDescriptor.type() == SurfaceDescriptor::TYCbCrImage ||
             aDescriptor.type() == SurfaceDescriptor::T__None);

  if (IsSurfaceDescriptorValid(mDescriptor)) {
    GetForwarder()->DestroySharedSurface(&mDescriptor);
  }
  mDescriptor = aDescriptor;
}

void
DeprecatedTextureClientShmemYCbCr::SetDescriptorFromReply(const SurfaceDescriptor& aDescriptor)
{
  MOZ_ASSERT(aDescriptor.type() == SurfaceDescriptor::TYCbCrImage);
  DeprecatedSharedPlanarYCbCrImage* shYCbCr = DeprecatedSharedPlanarYCbCrImage::FromSurfaceDescriptor(aDescriptor);
  if (shYCbCr) {
    shYCbCr->Release();
    mDescriptor = SurfaceDescriptor();
  } else {
    SetDescriptor(aDescriptor);
  }
}

bool
DeprecatedTextureClientShmemYCbCr::EnsureAllocated(gfx::IntSize aSize,
                                         gfxContentType aType)
{
  NS_RUNTIMEABORT("not enough arguments to do this (need both Y and CbCr sizes)");
  return false;
}


DeprecatedTextureClientTile::DeprecatedTextureClientTile(CompositableForwarder* aForwarder,
                                                         const TextureInfo& aTextureInfo,
                                                         gfxReusableSurfaceWrapper* aSurface)
  : DeprecatedTextureClient(aForwarder, aTextureInfo)
  , mSurface(aSurface)
{
  mTextureInfo.mDeprecatedTextureHostFlags = TEXTURE_HOST_TILED;
}

bool
DeprecatedTextureClientTile::EnsureAllocated(gfx::IntSize aSize, gfxContentType aType)
{
  if (!mSurface ||
      mSurface->Format() != gfxPlatform::GetPlatform()->OptimalFormatForContent(aType)) {
#ifdef MOZ_ANDROID_OMTC
    // If we're using OMTC, we can save some cycles by not using shared
    // memory. Using shared memory here is a small, but significant
    // performance regression.
    gfxImageSurface* tmpTile = new gfxImageSurface(gfxIntSize(aSize.width, aSize.height),
                                                   gfxPlatform::GetPlatform()->OptimalFormatForContent(aType),
                                                   aType != gfxContentType::COLOR);
    mSurface = new gfxReusableImageSurfaceWrapper(tmpTile);
#else
    nsRefPtr<gfxSharedImageSurface> sharedImage =
      gfxSharedImageSurface::CreateUnsafe(mForwarder,
                                          gfxIntSize(aSize.width, aSize.height),
                                          gfxPlatform::GetPlatform()->OptimalFormatForContent(aType));
    mSurface = new gfxReusableSharedImageSurfaceWrapper(mForwarder, sharedImage);
#endif
    mContentType = aType;
  }
  return true;
}

gfxImageSurface*
DeprecatedTextureClientTile::LockImageSurface()
{
  // Use the gfxReusableSurfaceWrapper, which will reuse the surface
  // if the compositor no longer has a read lock, otherwise the surface
  // will be copied into a new writable surface.
  gfxImageSurface* writableSurface = nullptr;
  mSurface = mSurface->GetWritable(&writableSurface);
  return writableSurface;
}

// XXX - All the code below can be removed as soon as we remove
// DeprecatedImageClientSingle (which has already been ported to the new
// textures).

bool AutoLockShmemClient::Update(Image* aImage,
                                 uint32_t aContentFlags,
                                 gfxASurface *aSurface)
{
  if (!aImage) {
    return false;
  }

  gfx::IntSize size = aImage->GetSize();

  gfxContentType contentType = aSurface->GetContentType();
  bool isOpaque = (aContentFlags & Layer::CONTENT_OPAQUE);
  if (contentType != gfxContentType::ALPHA &&
      isOpaque) {
    contentType = gfxContentType::COLOR;
  }
  mDeprecatedTextureClient->EnsureAllocated(size, contentType);

  OpenMode mode = mDeprecatedTextureClient->GetAccessMode() == DeprecatedTextureClient::ACCESS_READ_WRITE
                  ? OPEN_READ_WRITE
                  : OPEN_READ_ONLY;
  nsRefPtr<gfxASurface> tmpASurface =
    ShadowLayerForwarder::OpenDescriptor(mode,
                                         *mDeprecatedTextureClient->LockSurfaceDescriptor());
  if (!tmpASurface) {
    return false;
  }
  nsRefPtr<gfxContext> tmpCtx = new gfxContext(tmpASurface.get());
  tmpCtx->SetOperator(gfxContext::OPERATOR_SOURCE);
  tmpCtx->DrawSurface(aSurface, gfxSize(size.width, size.height));

  return true;
}

bool
AutoLockYCbCrClient::Update(PlanarYCbCrImage* aImage)
{
  MOZ_ASSERT(aImage);
  MOZ_ASSERT(mDescriptor);

  const PlanarYCbCrData *data = aImage->GetData();
  NS_ASSERTION(data, "Must be able to retrieve yuv data from image!");
  if (!data) {
    return false;
  }

  if (!EnsureDeprecatedTextureClient(aImage)) {
    return false;
  }

  ipc::Shmem& shmem = mDescriptor->get_YCbCrImage().data();

  YCbCrImageDataSerializer serializer(shmem.get<uint8_t>(), shmem.Size<uint8_t>());
  if (!serializer.CopyData(data->mYChannel, data->mCbChannel, data->mCrChannel,
                           data->mYSize, data->mYStride,
                           data->mCbCrSize, data->mCbCrStride,
                           data->mYSkip, data->mCbSkip)) {
    NS_WARNING("Failed to copy image data!");
    return false;
  }
  return true;
}

bool AutoLockYCbCrClient::EnsureDeprecatedTextureClient(PlanarYCbCrImage* aImage)
{
  MOZ_ASSERT(aImage);
  if (!aImage) {
    return false;
  }

  const PlanarYCbCrData *data = aImage->GetData();
  NS_ASSERTION(data, "Must be able to retrieve yuv data from image!");
  if (!data) {
    return false;
  }

  bool needsAllocation = false;
  if (mDescriptor->type() != SurfaceDescriptor::TYCbCrImage) {
    needsAllocation = true;
  } else {
    ipc::Shmem& shmem = mDescriptor->get_YCbCrImage().data();
    YCbCrImageDataSerializer serializer(shmem.get<uint8_t>(), shmem.Size<uint8_t>());
    if (serializer.GetYSize() != data->mYSize ||
        serializer.GetCbCrSize() != data->mCbCrSize) {
      needsAllocation = true;
    }
  }

  if (!needsAllocation) {
    return true;
  }

  mDeprecatedTextureClient->ReleaseResources();

  ipc::SharedMemory::SharedMemoryType shmType = OptimalShmemType();
  size_t size = YCbCrImageDataSerializer::ComputeMinBufferSize(data->mYSize,
                                                               data->mCbCrSize);
  ipc::Shmem shmem;
  if (!mDeprecatedTextureClient->GetForwarder()->AllocUnsafeShmem(size, shmType, &shmem)) {
    return false;
  }

  YCbCrImageDataSerializer serializer(shmem.get<uint8_t>(), shmem.Size<uint8_t>());
  serializer.InitializeBufferInfo(data->mYSize,
                                  data->mCbCrSize,
                                  data->mStereoMode);

  *mDescriptor = YCbCrImage(shmem, 0);

  return true;
}


}
}
