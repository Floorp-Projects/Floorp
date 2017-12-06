/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "Logging.h"
#include "SourceSurfaceSkia.h"
#include "HelpersSkia.h"
#include "DrawTargetSkia.h"
#include "DataSurfaceHelpers.h"
#include "skia/include/core/SkData.h"
#include "mozilla/CheckedInt.h"

using namespace std;

namespace mozilla {
namespace gfx {

SourceSurfaceSkia::SourceSurfaceSkia()
  : mDrawTarget(nullptr)
  , mChangeMutex("SourceSurfaceSkia::mChangeMutex")
{
}

SourceSurfaceSkia::~SourceSurfaceSkia()
{
}

IntSize
SourceSurfaceSkia::GetSize() const
{
  return mSize;
}

SurfaceFormat
SourceSurfaceSkia::GetFormat() const
{
  return mFormat;
}

sk_sp<SkImage>
SourceSurfaceSkia::GetImage()
{
  MutexAutoLock lock(mChangeMutex);
  sk_sp<SkImage> image = mImage;
  return image;
}

static sk_sp<SkData>
MakeSkData(void* aData, int32_t aHeight, size_t aStride)
{
  CheckedInt<size_t> size = aStride;
  size *= aHeight;
  if (size.isValid()) {
    void* mem = sk_malloc_flags(size.value(), 0);
    if (mem) {
      if (aData) {
        memcpy(mem, aData, size.value());
      }
      return SkData::MakeFromMalloc(mem, size.value());
    }
  }
  return nullptr;
}

static sk_sp<SkImage>
ReadSkImage(const sk_sp<SkImage>& aImage, const SkImageInfo& aInfo, size_t aStride)
{
  if (sk_sp<SkData> data = MakeSkData(nullptr, aInfo.height(), aStride)) {
    if (aImage->readPixels(aInfo, data->writable_data(), aStride, 0, 0, SkImage::kDisallow_CachingHint)) {
      return SkImage::MakeRasterData(aInfo, data, aStride);
    }
  }
  return nullptr;
}

bool
SourceSurfaceSkia::InitFromData(unsigned char* aData,
                                const IntSize &aSize,
                                int32_t aStride,
                                SurfaceFormat aFormat)
{
  sk_sp<SkData> data = MakeSkData(aData, aSize.height, aStride);
  if (!data) {
    return false;
  }

  SkImageInfo info = MakeSkiaImageInfo(aSize, aFormat);
  mImage = SkImage::MakeRasterData(info, data, aStride);
  if (!mImage) {
    return false;
  }

  mSize = aSize;
  mFormat = aFormat;
  mStride = aStride;
  return true;
}

bool
SourceSurfaceSkia::InitFromImage(const sk_sp<SkImage>& aImage,
                                 SurfaceFormat aFormat,
                                 DrawTargetSkia* aOwner)
{
  if (!aImage) {
    return false;
  }

  mSize = IntSize(aImage->width(), aImage->height());

  // For the raster image case, we want to use the format and stride
  // information that the underlying raster image is using, which is
  // reliable.
  // For the GPU case (for which peekPixels is false), we can't easily
  // figure this information out. It is better to report the originally
  // intended format and stride that we will convert to if this GPU
  // image is ever read back into a raster image.
  SkPixmap pixmap;
  if (aImage->peekPixels(&pixmap)) {
    mFormat =
      aFormat != SurfaceFormat::UNKNOWN ?
        aFormat :
        SkiaColorTypeToGfxFormat(pixmap.colorType(), pixmap.alphaType());
    mStride = pixmap.rowBytes();
  } else if (aFormat != SurfaceFormat::UNKNOWN) {
    mFormat = aFormat;
    SkImageInfo info = MakeSkiaImageInfo(mSize, mFormat);
    mStride = SkAlign4(info.minRowBytes());
  } else {
    return false;
  }

  mImage = aImage;

  if (aOwner) {
    mDrawTarget = aOwner;
  }

  return true;
}

uint8_t*
SourceSurfaceSkia::GetData()
{
  if (!mImage) {
    return nullptr;
  }
#ifdef USE_SKIA_GPU
  if (mImage->isTextureBacked()) {
    if (sk_sp<SkImage> raster = ReadSkImage(mImage, MakeSkiaImageInfo(mSize, mFormat), mStride)) {
      mImage = raster;
    } else {
      gfxCriticalError() << "Failed making Skia raster image for GPU surface";
    }
  }
#endif
  SkPixmap pixmap;
  if (!mImage->peekPixels(&pixmap)) {
    gfxCriticalError() << "Failed accessing pixels for Skia raster image";
  }
  return reinterpret_cast<uint8_t*>(pixmap.writable_addr());
}

bool
SourceSurfaceSkia::Map(MapType, MappedSurface *aMappedSurface)
{
  mChangeMutex.Lock();
  aMappedSurface->mData = GetData();
  aMappedSurface->mStride = Stride();
  mIsMapped = !!aMappedSurface->mData;
  return mIsMapped;
}

void
SourceSurfaceSkia::Unmap()
{
  mChangeMutex.Unlock();
  MOZ_ASSERT(mIsMapped);
  mIsMapped = false;
}

void
SourceSurfaceSkia::DrawTargetWillChange()
{
  MutexAutoLock lock(mChangeMutex);
  if (mDrawTarget) {
    // Raster snapshots do not use Skia's internal copy-on-write mechanism,
    // so we need to do an explicit copy here.
    // GPU snapshots, for which peekPixels is false, will already be dealt
    // with automatically via the internal copy-on-write mechanism, so we
    // don't need to do anything for them here.
    SkPixmap pixmap;
    if (mImage->peekPixels(&pixmap)) {
      mImage = ReadSkImage(mImage, pixmap.info(), pixmap.rowBytes());
      if (!mImage) {
        gfxCriticalError() << "Failed copying Skia raster snapshot";
      }
    }
    mDrawTarget = nullptr;
  }
}

} // namespace gfx
} // namespace mozilla
