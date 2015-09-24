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
  SkISize size = aCanvas->getDeviceSize();

  mBitmap = (SkBitmap)aCanvas->getDevice()->accessBitmap(false);

  mFormat = aFormat;

  mSize = IntSize(size.fWidth, size.fHeight);
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
  SkAlphaType alphaType = (aFormat == SurfaceFormat::B8G8R8X8) ?
    kOpaque_SkAlphaType : kPremul_SkAlphaType;

  SkImageInfo info = SkImageInfo::Make(aSize.width,
                                       aSize.height,
                                       GfxFormatToSkiaColorType(aFormat),
                                       alphaType);
  temp.setInfo(info, aStride);
  temp.setPixels(aData);

  if (!temp.copyTo(&mBitmap, GfxFormatToSkiaColorType(aFormat))) {
    return false;
  }

  if (aFormat == SurfaceFormat::B8G8R8X8) {
    mBitmap.setAlphaType(kIgnore_SkAlphaType);
  }

  mSize = aSize;
  mFormat = aFormat;
  mStride = mBitmap.rowBytes();
  return true;
}

bool
SourceSurfaceSkia::InitFromTexture(DrawTargetSkia* aOwner,
                                   unsigned int aTexture,
                                   const IntSize &aSize,
                                   SurfaceFormat aFormat)
{
  MOZ_ASSERT(aOwner, "null GrContext");
#ifdef USE_SKIA_GPU
  GrBackendTextureDesc skiaTexGlue;
  mSize.width = skiaTexGlue.fWidth = aSize.width;
  mSize.height = skiaTexGlue.fHeight = aSize.height;
  skiaTexGlue.fFlags = kNone_GrBackendTextureFlag;
  skiaTexGlue.fOrigin = kTopLeft_GrSurfaceOrigin;
  skiaTexGlue.fConfig = GfxFormatToGrConfig(aFormat);
  skiaTexGlue.fSampleCnt = 0;
  skiaTexGlue.fTextureHandle = aTexture;

  GrTexture *skiaTexture = aOwner->mGrContext->wrapBackendTexture(skiaTexGlue);
  SkImageInfo imgInfo = SkImageInfo::Make(aSize.width, aSize.height, GfxFormatToSkiaColorType(aFormat), kOpaque_SkAlphaType);
  SkGrPixelRef *texRef = new SkGrPixelRef(imgInfo, skiaTexture, false);
  mBitmap.setInfo(imgInfo, aSize.width*aSize.height*4);
  mBitmap.setPixelRef(texRef);
#endif

  mDrawTarget = aOwner;
  return true;
}

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
    SkBitmap temp = mBitmap;
    mBitmap.reset();
    temp.copyTo(&mBitmap, temp.colorType());
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
