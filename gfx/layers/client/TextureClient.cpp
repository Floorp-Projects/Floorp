/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/TextureClient.h"
#include "mozilla/layers/TextureClientOGL.h"

#include "mozilla/layers/ImageClient.h"
#include "mozilla/layers/CanvasClient.h"
#include "mozilla/layers/ContentClient.h"
#include "mozilla/layers/ShadowLayers.h"
#include "mozilla/layers/SharedPlanarYCbCrImage.h"
#include "GLContext.h"
#include "BasicLayers.h" // for PaintContext
#include "mozilla/layers/YCbCrImageDataSerializer.h"
#include "gfxReusableSurfaceWrapper.h"
#include "gfxPlatform.h"
#include "mozilla/StandardInteger.h"

using namespace mozilla::gl;

namespace mozilla {
namespace layers {

TextureClient::TextureClient(CompositableForwarder* aForwarder,
                             const TextureInfo& aTextureInfo)
  : mForwarder(aForwarder)
  , mTextureInfo(aTextureInfo)
  , mAccessMode(ACCESS_READ_WRITE)
{
  MOZ_COUNT_CTOR(TextureClient);
}

TextureClient::~TextureClient()
{
  MOZ_COUNT_DTOR(TextureClient);
  MOZ_ASSERT(mDescriptor.type() == SurfaceDescriptor::T__None, "Need to release surface!");
}

TextureClientShmem::TextureClientShmem(CompositableForwarder* aForwarder,
                                       const TextureInfo& aTextureInfo)
  : TextureClient(aForwarder, aTextureInfo)
  , mSurface(nullptr)
  , mSurfaceAsImage(nullptr)
{
}

void
TextureClientShmem::ReleaseResources()
{
  if (mSurface) {
    mSurface = nullptr;
    ShadowLayerForwarder::CloseDescriptor(mDescriptor);
  }

  if (mTextureInfo.mTextureFlags & HostRelease) {
    mDescriptor = SurfaceDescriptor();
    return;
  }

  if (IsSurfaceDescriptorValid(mDescriptor)) {
    mForwarder->DestroySharedSurface(&mDescriptor);
    mDescriptor = SurfaceDescriptor();
  }
}

void
TextureClientShmem::EnsureAllocated(gfx::IntSize aSize,
                                    gfxASurface::gfxContentType aContentType)
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
  }
}

void
TextureClientShmem::SetDescriptor(const SurfaceDescriptor& aDescriptor)
{
  if (IsSurfaceDescriptorValid(aDescriptor)) {
    ReleaseResources();
    mDescriptor = aDescriptor;
  } else {
    EnsureAllocated(mSize, mContentType);
  }

  mSurface = nullptr;

  NS_ASSERTION(mDescriptor.type() == SurfaceDescriptor::TSurfaceDescriptorGralloc ||
               mDescriptor.type() == SurfaceDescriptor::TShmem ||
               mDescriptor.type() == SurfaceDescriptor::TMemoryImage ||
               mDescriptor.type() == SurfaceDescriptor::TRGBImage,
               "Invalid surface descriptor");
}

gfxASurface*
TextureClientShmem::GetSurface()
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
  }

  return mSurface.get();
}

void
TextureClientShmem::Unlock()
{
  mSurface = nullptr;
  mSurfaceAsImage = nullptr;

  ShadowLayerForwarder::CloseDescriptor(mDescriptor);
}

gfxImageSurface*
TextureClientShmem::LockImageSurface()
{
  if (!mSurfaceAsImage) {
    mSurfaceAsImage = GetSurface()->GetAsImageSurface();
  }

  return mSurfaceAsImage.get();
}

void
TextureClientShmemYCbCr::ReleaseResources()
{
  GetForwarder()->DestroySharedSurface(&mDescriptor);
}

void
TextureClientShmemYCbCr::SetDescriptor(const SurfaceDescriptor& aDescriptor)
{
  MOZ_ASSERT(aDescriptor.type() == SurfaceDescriptor::TYCbCrImage);

  if (IsSurfaceDescriptorValid(mDescriptor)) {
    GetForwarder()->DestroySharedSurface(&mDescriptor);
  }
  mDescriptor = aDescriptor;
  MOZ_ASSERT(IsSurfaceDescriptorValid(mDescriptor));
}

void
TextureClientShmemYCbCr::SetDescriptorFromReply(const SurfaceDescriptor& aDescriptor)
{
  MOZ_ASSERT(aDescriptor.type() == SurfaceDescriptor::TYCbCrImage);
  SharedPlanarYCbCrImage* shYCbCr = SharedPlanarYCbCrImage::FromSurfaceDescriptor(aDescriptor);
  if (shYCbCr) {
    shYCbCr->Release();
    mDescriptor = SurfaceDescriptor();
  } else {
    SetDescriptor(aDescriptor);
  }
}

void
TextureClientShmemYCbCr::EnsureAllocated(gfx::IntSize aSize,
                                         gfxASurface::gfxContentType aType)
{
  NS_RUNTIMEABORT("not enough arguments to do this (need both Y and CbCr sizes)");
}


TextureClientTile::TextureClientTile(const TextureClientTile& aOther)
  : TextureClient(aOther.mForwarder, aOther.mTextureInfo)
  , mSurface(aOther.mSurface)
{}

TextureClientTile::~TextureClientTile()
{}

TextureClientTile::TextureClientTile(CompositableForwarder* aForwarder,
                                     const TextureInfo& aTextureInfo)
  : TextureClient(aForwarder, aTextureInfo)
  , mSurface(nullptr)
{
  mTextureInfo.mTextureHostFlags = TEXTURE_HOST_TILED;
}

void
TextureClientTile::EnsureAllocated(gfx::IntSize aSize, gfxASurface::gfxContentType aType)
{
  if (!mSurface ||
      mSurface->Format() != gfxPlatform::GetPlatform()->OptimalFormatForContent(aType)) {
    gfxImageSurface* tmpTile = new gfxImageSurface(gfxIntSize(aSize.width, aSize.height),
                                                   gfxPlatform::GetPlatform()->OptimalFormatForContent(aType),
                                                   aType != gfxASurface::CONTENT_COLOR);
    mSurface = new gfxReusableSurfaceWrapper(tmpTile);
    mContentType = aType;
  }
}

gfxImageSurface*
TextureClientTile::LockImageSurface()
{
  // Use the gfxReusableSurfaceWrapper, which will reuse the surface
  // if the compositor no longer has a read lock, otherwise the surface
  // will be copied into a new writable surface.
  gfxImageSurface* writableSurface = nullptr;
  mSurface = mSurface->GetWritable(&writableSurface);
  return writableSurface;
}

bool AutoLockShmemClient::Update(Image* aImage,
                                 uint32_t aContentFlags,
                                 gfxPattern* pat)
{
  nsRefPtr<gfxASurface> surface = pat->GetSurface();
  if (!aImage) {
    return false;
  }

  nsRefPtr<gfxPattern> pattern = pat ? pat : new gfxPattern(surface);

  gfxIntSize size = aImage->GetSize();

  gfxASurface::gfxContentType contentType = gfxASurface::CONTENT_COLOR_ALPHA;
  bool isOpaque = (aContentFlags & Layer::CONTENT_OPAQUE);
  if (surface) {
    contentType = surface->GetContentType();
  }
  if (contentType != gfxASurface::CONTENT_ALPHA &&
      isOpaque) {
    contentType = gfxASurface::CONTENT_COLOR;
  }
  mTextureClient->EnsureAllocated(gfx::IntSize(size.width, size.height), contentType);

  OpenMode mode = mTextureClient->GetAccessMode() == TextureClient::ACCESS_READ_WRITE
                  ? OPEN_READ_WRITE
                  : OPEN_READ_ONLY;
  nsRefPtr<gfxASurface> tmpASurface =
    ShadowLayerForwarder::OpenDescriptor(mode,
                                         *mTextureClient->LockSurfaceDescriptor());
  if (!tmpASurface) {
    return false;
  }
  nsRefPtr<gfxContext> tmpCtx = new gfxContext(tmpASurface.get());
  tmpCtx->SetOperator(gfxContext::OPERATOR_SOURCE);
  PaintContext(pat,
               nsIntRegion(nsIntRect(0, 0, size.width, size.height)),
               1.0, tmpCtx, nullptr);

  return true;
}

bool
AutoLockYCbCrClient::Update(PlanarYCbCrImage* aImage)
{
  MOZ_ASSERT(aImage);
  MOZ_ASSERT(mDescriptor);

  const PlanarYCbCrImage::Data *data = aImage->GetData();
  NS_ASSERTION(data, "Must be able to retrieve yuv data from image!");
  if (!data) {
    return false;
  }

  if (!EnsureTextureClient(aImage)) {
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

bool AutoLockYCbCrClient::EnsureTextureClient(PlanarYCbCrImage* aImage)
{
  MOZ_ASSERT(aImage);
  if (!aImage) {
    return false;
  }

  const PlanarYCbCrImage::Data *data = aImage->GetData();
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

  mTextureClient->ReleaseResources();

  ipc::SharedMemory::SharedMemoryType shmType = OptimalShmemType();
  size_t size = YCbCrImageDataSerializer::ComputeMinBufferSize(data->mYSize,
                                                               data->mCbCrSize);
  ipc::Shmem shmem;
  if (!mTextureClient->GetForwarder()->AllocUnsafeShmem(size, shmType, &shmem)) {
    return false;
  }

  YCbCrImageDataSerializer serializer(shmem.get<uint8_t>());
  serializer.InitializeBufferInfo(data->mYSize,
                                  data->mCbCrSize);

  *mDescriptor = YCbCrImage(shmem, 0);

  return true;
}

}
}
