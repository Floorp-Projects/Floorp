/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "Logging.h"
#include "SourceSurfaceSkia.h"
#include "HelpersSkia.h"
#include "DrawTargetSkia.h"
#include "DataSurfaceHelpers.h"
#include "skia/include/core/SkData.h"
#include "mozilla/CheckedInt.h"

namespace mozilla {
namespace gfx {

SourceSurfaceSkia::SourceSurfaceSkia()
  : mDrawTarget(nullptr)
{
}

SourceSurfaceSkia::~SourceSurfaceSkia()
{
  if (mDrawTarget) {
    mDrawTarget->SnapshotDestroyed();
    mDrawTarget = nullptr;
  }
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

bool
SourceSurfaceSkia::InitFromData(unsigned char* aData,
                                const IntSize &aSize,
                                int32_t aStride,
                                SurfaceFormat aFormat)
{
  SkPixmap pixmap(MakeSkiaImageInfo(aSize, aFormat), aData, aStride);
  mImage = SkImage::MakeRasterCopy(pixmap);
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
#ifdef USE_SKIA_GPU
  if (mImage->isTextureBacked()) {
    sk_sp<SkImage> raster;
    CheckedInt<size_t> size = mStride;
    size *= mSize.height;
    if (size.isValid()) {
      if (sk_sp<SkData> data = SkData::MakeUninitialized(size.value())) {
        SkImageInfo info = MakeSkiaImageInfo(mSize, mFormat);
        if (mImage->readPixels(info, data->writable_data(), mStride, 0, 0, SkImage::kDisallow_CachingHint)) {
          raster = SkImage::MakeRasterData(info, data, mStride);
        }
      }
    }
    if (raster) {
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

void
SourceSurfaceSkia::DrawTargetWillChange()
{
  if (mDrawTarget) {
    // Raster snapshots do not use Skia's internal copy-on-write mechanism,
    // so we need to do an explicit copy here.
    // GPU snapshots, for which peekPixels is false, will already be dealt
    // with automatically via the internal copy-on-write mechanism, so we
    // don't need to do anything for them here.
    SkPixmap pixmap;
    if (mImage->peekPixels(&pixmap)) {
      mImage = SkImage::MakeRasterCopy(pixmap);
      if (!mImage) {
        gfxCriticalError() << "Failed copying Skia raster snapshot";
      }
    }
    mDrawTarget = nullptr;
  }
}

} // namespace gfx
} // namespace mozilla
