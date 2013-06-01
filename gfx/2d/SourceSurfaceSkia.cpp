/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "Logging.h"
#include "SourceSurfaceSkia.h"
#include "skia/SkBitmap.h"
#include "skia/SkDevice.h"
#include "HelpersSkia.h"
#include "DrawTargetSkia.h"

namespace mozilla {
namespace gfx {

SourceSurfaceSkia::SourceSurfaceSkia()
  : mDrawTarget(nullptr)
{
}

SourceSurfaceSkia::~SourceSurfaceSkia()
{
  MarkIndependent();
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
  temp.setConfig(GfxFormatToSkiaConfig(aFormat), aSize.width, aSize.height, aStride);
  temp.setPixels(aData);

  if (!temp.copyTo(&mBitmap, GfxFormatToSkiaConfig(aFormat))) {
    return false;
  }

  if (aFormat == FORMAT_B8G8R8X8) {
    mBitmap.lockPixels();
    // We have to manually set the A channel to be 255 as Skia doesn't understand BGRX
    ConvertBGRXToBGRA(reinterpret_cast<unsigned char*>(mBitmap.getPixels()), aSize, aStride);
    mBitmap.unlockPixels();
    mBitmap.notifyPixelsChanged();
    mBitmap.setIsOpaque(true);
  }

  mSize = aSize;
  mFormat = aFormat;
  mStride = aStride;
  return true;
}

unsigned char*
SourceSurfaceSkia::GetData()
{
  mBitmap.lockPixels();
  unsigned char *pixels = (unsigned char *)mBitmap.getPixels();
  mBitmap.unlockPixels();
  return pixels;

}

void
SourceSurfaceSkia::DrawTargetWillChange()
{
  if (mDrawTarget) {
    mDrawTarget = nullptr;
    SkBitmap temp = mBitmap;
    mBitmap.reset();
    temp.copyTo(&mBitmap, temp.getConfig());
  }
}

void
SourceSurfaceSkia::DrawTargetDestroyed()
{
  mDrawTarget = nullptr;
}

void
SourceSurfaceSkia::MarkIndependent()
{
  if (mDrawTarget) {
    mDrawTarget->RemoveSnapshot(this);
    mDrawTarget = nullptr;
  }
}

}
}
