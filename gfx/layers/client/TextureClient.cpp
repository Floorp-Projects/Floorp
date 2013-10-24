/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/TextureClient.h"
#include <stdint.h>                     // for uint8_t, uint32_t, etc
#include "Layers.h"                     // for Layer, etc
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
#include "nsTraceRefcnt.h"              // for MOZ_COUNT_CTOR, etc
#include "ImageContainer.h"             // for PlanarYCbCrImage, etc
#include "mozilla/gfx/2D.h"

#ifdef MOZ_ANDROID_OMTC
#  include "gfxReusableImageSurfaceWrapper.h"
#  include "gfxImageSurface.h"
#else
#  include "gfxReusableSharedImageSurfaceWrapper.h"
#  include "gfxSharedImageSurface.h"
#endif

using namespace mozilla::gl;
using namespace mozilla::gfx;

namespace mozilla {
namespace layers {

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
    MOZ_COUNT_CTOR(MemoryTextureClientData);
  }

  virtual void DeallocateSharedData(ISurfaceAllocator*)
  {
    delete[] mBuffer;
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
  : mID(0)
  , mFlags(aFlags)
  , mShared(false)
  , mValid(true)
{}

TextureClient::~TextureClient()
{}

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
  if (!IsAllocated() || GetFormat() == gfx::FORMAT_UNKNOWN) {
    return false;
  }

  aDescriptor = SurfaceDescriptorShmem(mShmem, GetFormat());

  return true;
}

ISurfaceAllocator*
ShmemTextureClient::GetAllocator() const
{
  return mCompositable->GetForwarder();
}

bool
ShmemTextureClient::Allocate(uint32_t aSize)
{
  MOZ_ASSERT(IsValid());
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

ShmemTextureClient::ShmemTextureClient(CompositableClient* aCompositable,
                                       gfx::SurfaceFormat aFormat,
                                       TextureFlags aFlags)
  : BufferTextureClient(aCompositable, aFormat, aFlags)
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
    mCompositable->GetForwarder()->DeallocShmem(mShmem);
  }
}

bool
MemoryTextureClient::ToSurfaceDescriptor(SurfaceDescriptor& aDescriptor)
{
  MOZ_ASSERT(IsValid());
  if (!IsAllocated() || GetFormat() == gfx::FORMAT_UNKNOWN) {
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
  mBuffer = new uint8_t[aSize];
  mBufSize = aSize;
  return true;
}

MemoryTextureClient::MemoryTextureClient(CompositableClient* aCompositable,
                                         gfx::SurfaceFormat aFormat,
                                         TextureFlags aFlags)
  : BufferTextureClient(aCompositable, aFormat, aFlags)
  , mBuffer(nullptr)
  , mBufSize(0)
{
  MOZ_COUNT_CTOR(MemoryTextureClient);
}

MemoryTextureClient::~MemoryTextureClient()
{
  MOZ_COUNT_DTOR(MemoryTextureClient);
  if (ShouldDeallocateInDestructor()) {
    // if the buffer has never been shared we must deallocate it or ir would
    // leak.
    delete mBuffer;
  }
}

BufferTextureClient::BufferTextureClient(CompositableClient* aCompositable,
                                         gfx::SurfaceFormat aFormat,
                                         TextureFlags aFlags)
  : TextureClient(aFlags)
  , mCompositable(aCompositable)
  , mFormat(aFormat)
{}

BufferTextureClient::~BufferTextureClient()
{}

bool
BufferTextureClient::UpdateSurface(gfxASurface* aSurface)
{
  MOZ_ASSERT(aSurface);
  MOZ_ASSERT(!IsImmutable());
  MOZ_ASSERT(IsValid());

  ImageDataSerializer serializer(GetBuffer());
  if (!serializer.IsValid()) {
    return false;
  }

  if (gfxPlatform::GetPlatform()->SupportsAzureContent()) {
    RefPtr<DrawTarget> dt = serializer.GetAsDrawTarget();
    RefPtr<SourceSurface> source = gfxPlatform::GetPlatform()->GetSourceSurfaceForSurface(dt, aSurface);

    dt->CopySurface(source, IntRect(IntPoint(), serializer.GetSize()), IntPoint());
  } else {
    RefPtr<gfxImageSurface> surf = serializer.GetAsThebesSurface();
    if (!surf) {
      return false;
    }

    nsRefPtr<gfxContext> tmpCtx = new gfxContext(surf.get());
    tmpCtx->SetOperator(gfxContext::OPERATOR_SOURCE);
    tmpCtx->DrawSurface(aSurface, gfxSize(serializer.GetSize().width,
                                          serializer.GetSize().height));
  }


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
  MOZ_ASSERT(IsValid());

  ImageDataSerializer serializer(GetBuffer());
  if (!serializer.IsValid()) {
    return nullptr;
  }

  RefPtr<gfxImageSurface> surf = serializer.GetAsThebesSurface();
  nsRefPtr<gfxASurface> result = surf.get();
  return result.forget();
}

bool
BufferTextureClient::AllocateForSurface(gfx::IntSize aSize)
{
  MOZ_ASSERT(IsValid());
  MOZ_ASSERT(mFormat != gfx::FORMAT_YUV, "This textureClient cannot use YCbCr data");

  int bufSize
    = ImageDataSerializer::ComputeMinBufferSize(aSize, mFormat);
  if (!Allocate(bufSize)) {
    return false;
  }
  ImageDataSerializer serializer(GetBuffer());
  serializer.InitializeBufferInfo(aSize, mFormat);
  mSize = aSize;
  return true;
}

bool
BufferTextureClient::UpdateYCbCr(const PlanarYCbCrData& aData)
{
  MOZ_ASSERT(mFormat == gfx::FORMAT_YUV, "This textureClient can only use YCbCr data");
  MOZ_ASSERT(!IsImmutable());
  MOZ_ASSERT(IsValid());
  MOZ_ASSERT(aData.mCbSkip == aData.mCrSkip);

  YCbCrImageDataSerializer serializer(GetBuffer());
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
  YCbCrImageDataSerializer serializer(GetBuffer());
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

void
DeprecatedTextureClient::OnActorDestroy()
{
  if (ISurfaceAllocator::IsShmem(&mDescriptor)) {
    mDescriptor = SurfaceDescriptor();
  }
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

    if (!mForwarder->AllocSurfaceDescriptor(gfxIntSize(mSize.width, mSize.height),
                                            mContentType, &mDescriptor)) {
      NS_WARNING("creating SurfaceDescriptor failed!");
    }
    if (mContentType == GFX_CONTENT_COLOR_ALPHA) {
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
  mSurface = nullptr;
  mSurfaceAsImage = nullptr;
  mDrawTarget = nullptr;

  ShadowLayerForwarder::CloseDescriptor(mDescriptor);
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
                                                   aType != GFX_CONTENT_COLOR);
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

  gfxIntSize size = aImage->GetSize();

  gfxContentType contentType = aSurface->GetContentType();
  bool isOpaque = (aContentFlags & Layer::CONTENT_OPAQUE);
  if (contentType != GFX_CONTENT_ALPHA &&
      isOpaque) {
    contentType = GFX_CONTENT_COLOR;
  }
  mDeprecatedTextureClient->EnsureAllocated(gfx::IntSize(size.width, size.height), contentType);

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

  YCbCrImageDataSerializer serializer(shmem.get<uint8_t>());
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
    YCbCrImageDataSerializer serializer(shmem.get<uint8_t>());
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

  YCbCrImageDataSerializer serializer(shmem.get<uint8_t>());
  serializer.InitializeBufferInfo(data->mYSize,
                                  data->mCbCrSize,
                                  data->mStereoMode);

  *mDescriptor = YCbCrImage(shmem, 0);

  return true;
}


}
}
