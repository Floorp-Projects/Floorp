/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "Logging.h"
#include "SourceSurfaceSkia.h"
#include "skia/include/core/SkBitmap.h"
#include "skia/include/core/SkDevice.h"
#include "HelpersSkia.h"
#include "DrawTargetSkia.h"
#include "DataSurfaceHelpers.h"

#ifdef USE_SKIA_GPU
#include "skia/include/gpu/SkGrPixelRef.h"
#endif

namespace mozilla {
namespace gfx {

SourceSurfaceSkia::SourceSurfaceSkia()
  : mDrawTarget(nullptr), mLocked(false)
{
}

SourceSurfaceSkia::~SourceSurfaceSkia()
{
  MaybeUnlock();
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
SourceSurfaceSkia::InitFromCanvas(SkCanvas* aCanvas,
                                  SurfaceFormat aFormat,
                                  DrawTargetSkia* aOwner)
{
  mBitmap = aCanvas->getDevice()->accessBitmap(false);

  mFormat = aFormat;

  mSize = IntSize(mBitmap.width(), mBitmap.height());
  mStride = mBitmap.rowBytes();
  mDrawTarget = aOwner;

  return true;
}

bool
SourceSurfaceSkia::InitFromData(unsigned char* aData,
                                const IntSize &aSize,
                                int32_t aStride,
                                SurfaceFormat aFormat)
{
  SkBitmap temp;
  temp.setInfo(MakeSkiaImageInfo(aSize, aFormat), aStride);
  temp.setPixels(aData);

  if (!temp.copyTo(&mBitmap)) {
    return false;
  }

  mSize = aSize;
  mFormat = aFormat;
  mStride = mBitmap.rowBytes();
  return true;
}

void
SourceSurfaceSkia::InitFromBitmap(const SkBitmap& aBitmap)
{
  mBitmap = aBitmap;

  mSize = IntSize(mBitmap.width(), mBitmap.height());
  mFormat = SkiaColorTypeToGfxFormat(mBitmap.colorType(), mBitmap.alphaType());
  mStride = mBitmap.rowBytes();
}

#ifdef USE_SKIA_GPU
bool
SourceSurfaceSkia::InitFromGrTexture(GrTexture* aTexture,
                                     const IntSize &aSize,
                                     SurfaceFormat aFormat)
{
  if (!aTexture) {
    return false;
  }

  // Create a GPU pixelref wrapping the texture.
  SkImageInfo imgInfo = MakeSkiaImageInfo(aSize, aFormat);
  mBitmap.setInfo(imgInfo);
  mBitmap.setPixelRef(new SkGrPixelRef(imgInfo, aTexture))->unref();

  mSize = aSize;
  mFormat = aFormat;
  mStride = mBitmap.rowBytes();
  return true;
}
#endif

unsigned char*
SourceSurfaceSkia::GetData()
{
  if (!mLocked) {
    mBitmap.lockPixels();
    mLocked = true;
  }

  unsigned char *pixels = (unsigned char *)mBitmap.getPixels();
  return pixels;
}

void
SourceSurfaceSkia::DrawTargetWillChange()
{
  if (mDrawTarget) {
    MaybeUnlock();

    mDrawTarget = nullptr;

    // First try a deep copy to avoid a readback from the GPU.
    // If that fails, try the CPU copy.
    if (!mBitmap.deepCopyTo(&mBitmap) &&
        !mBitmap.copyTo(&mBitmap)) {
      mBitmap.reset();
    }
  }
}

void
SourceSurfaceSkia::MaybeUnlock()
{
  if (mLocked) {
    mBitmap.unlockPixels();
    mLocked = false;
  }
}

} // namespace gfx
} // namespace mozilla
