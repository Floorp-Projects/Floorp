/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/TextureClient.h"
#include <stdint.h>                     // for uint8_t, uint32_t, etc
#include "Layers.h"                     // for Layer, etc
#include "gfx2DGlue.h"
#include "gfxPlatform.h"                // for gfxPlatform
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
class TextureChild final : public PTextureChild
{
  ~TextureChild()
  {
    if (mKeep && mMainThreadOnly && !NS_IsMainThread()) {
      nsRefPtr<ReleaseKeepAlive> release = new ReleaseKeepAlive();
      release->mKeep = Move(mKeep);
      NS_DispatchToMainThread(release);
    }
  }
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(TextureChild)

  TextureChild()
  : mForwarder(nullptr)
  , mTextureClient(nullptr)
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
    mWaitForRecycle = mTextureClient;
    RECYCLE_LOG("[CLIENT] Wait for recycle %p\n", mWaitForRecycle.get());
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

  RefPtr<CompositableForwarder> mForwarder;
  RefPtr<TextureClient> mWaitForRecycle;
  TextureClient* mTextureClient;
  UniquePtr<KeepAlive> mKeep;
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
  }
  mWaitForRecycle = nullptr;
  mKeep = nullptr;
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
  if (mValid && mActor && mActor->IPCOpen()) {
    mActor->SendRecycleTexture(mFlags);
  }
}

void
TextureClient::RemoveFlags(TextureFlags aFlags)
{
  MOZ_ASSERT(!IsSharedWithCompositor() ||
             ((GetFlags() & TextureFlags::RECYCLE) && !IsAddedToCompositableClient()));
  mFlags &= ~aFlags;
  if (mValid && mActor && mActor->IPCOpen()) {
    mActor->SendRecycleTexture(mFlags);
  }
}

void
TextureClient::RecycleTexture(TextureFlags aFlags)
{
  MOZ_ASSERT(GetFlags() & TextureFlags::RECYCLE);
  MOZ_ASSERT(!HasRecycleCallback());

  mAddedToCompositableClient = false;
  if (mFlags != aFlags) {
    mFlags = aFlags;
    if (mValid && mActor && mActor->IPCOpen()) {
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

bool
TextureClient::InitIPDLActor(CompositableForwarder* aForwarder)
{
  MOZ_ASSERT(aForwarder && aForwarder->GetMessageLoop() == mAllocator->GetMessageLoop());
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
DisableGralloc(SurfaceFormat aFormat, const gfx::IntSize& aSizeHint)
{
  if (gfxPrefs::DisableGralloc()) {
    return true;
  }
  if (aFormat == gfx::SurfaceFormat::A8) {
    return true;
  }

#if ANDROID_VERSION <= 15
  // Adreno 200 has a problem of drawing gralloc buffer width less than 64 and
  // drawing gralloc buffer with a height 9px-16px.
  // See Bug 983971.
  if (aSizeHint.width < 64 || aSizeHint.height < 32) {
    return true;
  }
#endif

  return false;
}
#endif

static
already_AddRefed<BufferTextureClient>
CreateBufferTextureClient(ISurfaceAllocator* aAllocator,
                          SurfaceFormat aFormat,
                          TextureFlags aTextureFlags,
                          gfx::BackendType aMoz2DBackend)
{
  if (aAllocator->IsSameProcess()) {
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

static inline gfx::BackendType
BackendTypeForBackendSelector(BackendSelector aSelector)
{
  switch (aSelector) {
    case BackendSelector::Canvas:
      return gfxPlatform::GetPlatform()->GetPreferredCanvasBackend();
    case BackendSelector::Content:
      return gfxPlatform::GetPlatform()->GetContentBackend();
    default:
      MOZ_ASSERT_UNREACHABLE("Unknown backend selector");
      return gfx::BackendType::NONE;
  }
};

// static
already_AddRefed<TextureClient>
TextureClient::CreateForDrawing(ISurfaceAllocator* aAllocator,
                                gfx::SurfaceFormat aFormat,
                                gfx::IntSize aSize,
                                BackendSelector aSelector,
                                TextureFlags aTextureFlags,
                                TextureAllocationFlags aAllocFlags)
{
  gfx::BackendType moz2DBackend = BackendTypeForBackendSelector(aSelector);

  RefPtr<TextureClient> texture;

#if defined(MOZ_WIDGET_GONK) || defined(XP_WIN)
  int32_t maxTextureSize = aAllocator->GetMaxTextureSize();
#endif

#ifdef XP_WIN
  LayersBackend parentBackend = aAllocator->GetCompositorBackendType();
  if (parentBackend == LayersBackend::LAYERS_D3D11 &&
      (moz2DBackend == gfx::BackendType::DIRECT2D ||
       moz2DBackend == gfx::BackendType::DIRECT2D1_1) &&
      aSize.width <= maxTextureSize &&
      aSize.height <= maxTextureSize)
  {
    texture = new TextureClientD3D11(aAllocator, aFormat, aTextureFlags);
  }
  if (parentBackend == LayersBackend::LAYERS_D3D9 &&
      moz2DBackend == gfx::BackendType::CAIRO &&
      aAllocator->IsSameProcess() &&
      aSize.width <= maxTextureSize &&
      aSize.height <= maxTextureSize &&
      NS_IsMainThread()) {
    if (gfxWindowsPlatform::GetPlatform()->GetD3D9Device()) {
      texture = new TextureClientD3D9(aAllocator, aFormat, aTextureFlags);
    }
  }

  if (!texture && aFormat == SurfaceFormat::B8G8R8X8 &&
      aAllocator->IsSameProcess() &&
      moz2DBackend == gfx::BackendType::CAIRO &&
      NS_IsMainThread()) {
    if (aAllocator->IsSameProcess()) {
      texture = new TextureClientMemoryDIB(aAllocator, aFormat, aTextureFlags);
    } else {
      texture = new TextureClientShmemDIB(aAllocator, aFormat, aTextureFlags);
    }
  }
#endif

#ifdef MOZ_X11
  LayersBackend parentBackend = aAllocator->GetCompositorBackendType();
  gfxSurfaceType type =
    gfxPlatform::GetPlatform()->ScreenReferenceSurface()->GetType();

  if (parentBackend == LayersBackend::LAYERS_BASIC &&
      moz2DBackend == gfx::BackendType::CAIRO &&
      type == gfxSurfaceType::Xlib)
  {
    texture = new TextureClientX11(aAllocator, aFormat, aTextureFlags);
  }
#ifdef GL_PROVIDER_GLX
  if (parentBackend == LayersBackend::LAYERS_OPENGL &&
      type == gfxSurfaceType::Xlib &&
      aFormat != SurfaceFormat::A8 &&
      gl::sGLXLibrary.UseTextureFromPixmap())
  {
    texture = new TextureClientX11(aAllocator, aFormat, aTextureFlags);
  }
#endif
#endif

#ifdef MOZ_WIDGET_GONK
  if (!DisableGralloc(aFormat, aSize)) {
    // Don't allow Gralloc texture clients to exceed the maximum texture size.
    // BufferTextureClients have code to handle tiling the surface client-side.
    if (aSize.width <= maxTextureSize && aSize.height <= maxTextureSize) {
      texture = new GrallocTextureClientOGL(aAllocator, aFormat, moz2DBackend,
                                           aTextureFlags);
    }
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
  texture = CreateBufferTextureClient(aAllocator, aFormat, aTextureFlags, moz2DBackend);

  if (!texture->AllocateForSurface(aSize, aAllocFlags)) {
    return nullptr;
  }

  return texture.forget();
}

// static
already_AddRefed<BufferTextureClient>
TextureClient::CreateForRawBufferAccess(ISurfaceAllocator* aAllocator,
                                        gfx::SurfaceFormat aFormat,
                                        gfx::IntSize aSize,
                                        gfx::BackendType aMoz2DBackend,
                                        TextureFlags aTextureFlags,
                                        TextureAllocationFlags aAllocFlags)
{
  RefPtr<BufferTextureClient> texture =
    CreateBufferTextureClient(aAllocator, aFormat,
                              aTextureFlags, aMoz2DBackend);
  if (texture) {
    if (!texture->AllocateForSurface(aSize, aAllocFlags)) {
      return nullptr;
    }
  }
  return texture.forget();
}

// static
already_AddRefed<BufferTextureClient>
TextureClient::CreateForYCbCr(ISurfaceAllocator* aAllocator,
                              gfx::IntSize aYSize,
                              gfx::IntSize aCbCrSize,
                              StereoMode aStereoMode,
                              TextureFlags aTextureFlags)
{
  RefPtr<BufferTextureClient> texture;
  if (aAllocator->IsSameProcess()) {
    texture = new MemoryTextureClient(aAllocator, gfx::SurfaceFormat::YUV,
                                      gfx::BackendType::NONE,
                                      aTextureFlags);
  } else {
    texture = new ShmemTextureClient(aAllocator, gfx::SurfaceFormat::YUV,
                                     gfx::BackendType::NONE,
                                     aTextureFlags);
  }

  if (!texture->AllocateForYCbCr(aYSize, aCbCrSize, aStereoMode)) {
    return nullptr;
  }

  return texture.forget();
}

// static
already_AddRefed<BufferTextureClient>
TextureClient::CreateWithBufferSize(ISurfaceAllocator* aAllocator,
                     gfx::SurfaceFormat aFormat,
                     size_t aSize,
                     TextureFlags aTextureFlags)
{
  RefPtr<BufferTextureClient> texture;
  if (aAllocator->IsSameProcess()) {
    texture = new MemoryTextureClient(aAllocator, gfx::SurfaceFormat::YUV,
                                      gfx::BackendType::NONE,
                                      aTextureFlags);
  } else {
    texture = new ShmemTextureClient(aAllocator, gfx::SurfaceFormat::YUV,
                                     gfx::BackendType::NONE,
                                     aTextureFlags);
  }

  if (!texture->Allocate(aSize)) {
    return nullptr;
  }

  return texture.forget();
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
  if (mValid && mActor) {
    if (sync || GetFlags() & TextureFlags::DEALLOCATE_CLIENT) {
      MOZ_PERFORMANCE_WARNING("gfx", "TextureClient/Host pair requires synchronous deallocation");
      if (mActor->IPCOpen()) {
        mActor->SendClearTextureHostSync();
        mActor->SendRemoveTexture();
      }
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
    // The actor has a raw pointer to us, actor->mTextureClient.
    // Null it before RemoveTexture calls to avoid invalid actor->mTextureClient
    // when calling TextureChild::ActorDestroy()
    actor->mTextureClient = nullptr;

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
  if (aSize > 0) {
    SharedMemory::SharedMemoryType memType = OptimalShmemType();
    mAllocated = GetAllocator()->AllocUnsafeShmem(aSize, memType, &mShmem);
  }
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
  mBuffer = new (fallible) uint8_t[aSize];
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
  : TextureClient(aAllocator, aFlags)
  , mFormat(aFormat)
  , mBackend(aMoz2DBackend)
  , mOpenMode(OpenMode::OPEN_NONE)
  , mLocked(false)
{}

BufferTextureClient::~BufferTextureClient()
{}

already_AddRefed<TextureClient>
BufferTextureClient::CreateSimilar(TextureFlags aFlags,
                                   TextureAllocationFlags aAllocFlags) const
{
  // This may return null
  RefPtr<BufferTextureClient> newBufferTex = TextureClient::CreateForRawBufferAccess(
    mAllocator, mFormat, mSize, mBackend, mFlags | aFlags, aAllocFlags
  );

  return newBufferTex.forget();
}

bool
BufferTextureClient::AllocateForSurface(gfx::IntSize aSize, TextureAllocationFlags aFlags)
{
  MOZ_ASSERT(IsValid());
  MOZ_ASSERT(mFormat != gfx::SurfaceFormat::YUV, "This textureClient cannot use YCbCr data");
  MOZ_ASSERT(aSize.width > 0 && aSize.height > 0);

  if (aSize.width <= 0 || aSize.height <= 0) {
    gfxDebug() << "Asking for buffer of invalid size " << aSize.width << "x" << aSize.height;
    return false;
  }

  uint32_t bufSize = ImageDataSerializer::ComputeMinBufferSize(aSize, mFormat);
  if (!bufSize || !Allocate(bufSize)) {
    return false;
  }

  if (aFlags & ALLOC_CLEAR_BUFFER) {
    memset(GetBuffer(), 0, bufSize);
  }
  if (aFlags & ALLOC_CLEAR_BUFFER_WHITE) {
    memset(GetBuffer(), 0xFF, bufSize);
  }

  ImageDataSerializer serializer(GetBuffer(), GetBufferSize());
  serializer.InitializeBufferInfo(aSize, mFormat);
  mSize = aSize;
  return true;
}

gfx::DrawTarget*
BufferTextureClient::BorrowDrawTarget()
{
  MOZ_ASSERT(IsValid());
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mLocked, "BorrowDrawTarget should be called on locked textures only");
  if (!mLocked) {
    return nullptr;
  }

  if (mDrawTarget) {
    mDrawTarget->SetTransform(Matrix());
    return mDrawTarget;
  }

  ImageDataSerializer serializer(GetBuffer(), GetBufferSize());
  if (!serializer.IsValid()) {
    return nullptr;
  }

  mDrawTarget = serializer.GetAsDrawTarget(mBackend);
  if (mDrawTarget) {
    return mDrawTarget;
  }

  mDrawTarget = serializer.GetAsDrawTarget(BackendType::CAIRO);

  return mDrawTarget;
}

void
BufferTextureClient::UpdateFromSurface(gfx::SourceSurface* aSurface)
{
  ImageDataSerializer serializer(GetBuffer(), GetBufferSize());

  RefPtr<DataSourceSurface> surface = serializer.GetAsSurface();

  if (!surface) {
    gfxCriticalError() << "Failed to get serializer as surface!";
    return;
  }

  RefPtr<DataSourceSurface> srcSurf = aSurface->GetDataSurface();

  if (!srcSurf) {
    gfxCriticalError() << "Failed to GetDataSurface in UpdateFromSurface.";
    return;
  }

  if (surface->GetSize() != srcSurf->GetSize() || surface->GetFormat() != srcSurf->GetFormat()) {
    gfxCriticalError() << "Attempt to update texture client from a surface with a different size or format! This: " << surface->GetSize() << " " << surface->GetFormat() << " Other: " << aSurface->GetSize() << " " << aSurface->GetFormat();
    return;
  }

  DataSourceSurface::MappedSurface sourceMap;
  DataSourceSurface::MappedSurface destMap;
  if (!srcSurf->Map(DataSourceSurface::READ, &sourceMap)) {
    gfxCriticalError() << "Failed to map source surface for UpdateFromSurface.";
    return;
  }

  if (!surface->Map(DataSourceSurface::WRITE, &destMap)) {
    srcSurf->Unmap();
    gfxCriticalError() << "Failed to map destination surface for UpdateFromSurface.";
    return;
  }


  for (int y = 0; y < srcSurf->GetSize().height; y++) {
    memcpy(destMap.mData + destMap.mStride * y,
           sourceMap.mData + sourceMap.mStride * y,
           srcSurf->GetSize().width * BytesPerPixel(srcSurf->GetFormat()));
  }

  srcSurf->Unmap();
  surface->Unmap();
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
    return;
  }

  // see the comment on TextureClient::BorrowDrawTarget.
  // This DrawTarget is internal to the TextureClient and is only exposed to the
  // outside world between Lock() and Unlock(). This assertion checks that no outside
  // reference remains by the time Unlock() is called.
  MOZ_ASSERT(mDrawTarget->refCount() == 1);

  if (mReadbackSink) {
    RefPtr<SourceSurface> snapshot = mDrawTarget->Snapshot();
    RefPtr<DataSourceSurface> dataSurf = snapshot->GetDataSurface();
    mReadbackSink->ProcessReadback(dataSurf);
  }

  mDrawTarget->Flush();
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
  if (!bufSize || !Allocate(bufSize)) {
    return false;
  }
  YCbCrImageDataSerializer serializer(GetBuffer(), GetBufferSize());
  serializer.InitializeBufferInfo(aYSize,
                                  aCbCrSize,
                                  aStereoMode);
  mSize = aYSize;
  return true;
}

uint8_t*
BufferTextureClient::GetLockedData() const
{
  MOZ_ASSERT(IsLocked());

  ImageDataSerializer serializer(GetBuffer(), GetBufferSize());
  MOZ_ASSERT(serializer.IsValid());

  return serializer.GetData();
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

} // namespace layers
} // namespace mozilla
