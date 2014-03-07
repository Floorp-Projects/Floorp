/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SourceSurfaceD2D1.h"
#include "DrawTargetD2D1.h"
#include "Tools.h"

namespace mozilla {
namespace gfx {

SourceSurfaceD2D1::SourceSurfaceD2D1(ID2D1Image *aImage, ID2D1DeviceContext *aDC,
                                     SurfaceFormat aFormat, const IntSize &aSize,
                                     DrawTargetD2D1 *aDT)
  : mImage(aImage)
  , mDC(aDC)
  , mDrawTarget(aDT)
{
  aImage->QueryInterface((ID2D1Bitmap1**)byRef(mRealizedBitmap));

  mFormat = aFormat;
  mSize = aSize;
}

SourceSurfaceD2D1::~SourceSurfaceD2D1()
{
}

TemporaryRef<DataSourceSurface>
SourceSurfaceD2D1::GetDataSurface()
{
  EnsureRealizedBitmap();

  RefPtr<ID2D1Bitmap1> softwareBitmap;
  D2D1_BITMAP_PROPERTIES1 props;
  props.dpiX = 96;
  props.dpiY = 96;
  props.pixelFormat = D2DPixelFormat(mFormat);
  props.colorContext = nullptr;
  props.bitmapOptions = D2D1_BITMAP_OPTIONS_CANNOT_DRAW |
                        D2D1_BITMAP_OPTIONS_CPU_READ;
  mDC->CreateBitmap(D2DIntSize(mSize), nullptr, 0, props, (ID2D1Bitmap1**)byRef(softwareBitmap));

  D2D1_POINT_2U point = D2D1::Point2U(0, 0);
  D2D1_RECT_U rect = D2D1::RectU(0, 0, mSize.width, mSize.height);
  softwareBitmap->CopyFromBitmap(&point, mRealizedBitmap, &rect);
  return new DataSourceSurfaceD2D1(softwareBitmap, mFormat);
}

void
SourceSurfaceD2D1::EnsureRealizedBitmap()
{
  if (mRealizedBitmap) {
    return;
  }

  RefPtr<ID2D1DeviceContext> dc;
  Factory::GetD2D1Device()->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, byRef(dc));

  D2D1_BITMAP_PROPERTIES1 props;
  props.dpiX = 96;
  props.dpiY = 96;
  props.pixelFormat = D2DPixelFormat(mFormat);
  props.colorContext = nullptr;
  props.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET;
  dc->CreateBitmap(D2DIntSize(mSize), nullptr, 0, props, (ID2D1Bitmap1**)byRef(mRealizedBitmap));

  dc->SetTarget(mRealizedBitmap);

  dc->BeginDraw();
  dc->DrawImage(mImage);
  dc->EndDraw();
}

void
SourceSurfaceD2D1::DrawTargetWillChange()
{
  // At this point in time this should always be true here.
  MOZ_ASSERT(mRealizedBitmap);

  RefPtr<ID2D1Bitmap1> oldBitmap = mRealizedBitmap;

  D2D1_BITMAP_PROPERTIES1 props;
  props.dpiX = 96;
  props.dpiY = 96;
  props.pixelFormat = D2DPixelFormat(mFormat);
  props.colorContext = nullptr;
  props.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET;
  mDC->CreateBitmap(D2DIntSize(mSize), nullptr, 0, props, (ID2D1Bitmap1**)byRef(mRealizedBitmap));

  D2D1_POINT_2U point = D2D1::Point2U(0, 0);
  D2D1_RECT_U rect = D2D1::RectU(0, 0, mSize.width, mSize.height);
  mRealizedBitmap->CopyFromBitmap(&point, oldBitmap, &rect);
  mImage = mRealizedBitmap;

  DrawTargetD2D1::mVRAMUsageSS += mSize.width * mSize.height * BytesPerPixel(mFormat);
  mDrawTarget = nullptr;

  // We now no longer depend on the source surface content remaining the same.
  MarkIndependent();
}

void
SourceSurfaceD2D1::MarkIndependent()
{
  if (mDrawTarget) {
    MOZ_ASSERT(mDrawTarget->mSnapshot == this);
    mDrawTarget->mSnapshot = nullptr;
    mDrawTarget = nullptr;
  }
}

DataSourceSurfaceD2D1::DataSourceSurfaceD2D1(ID2D1Bitmap1 *aMappableBitmap, SurfaceFormat aFormat)
  : mBitmap(aMappableBitmap)
  , mFormat(aFormat)
  , mMapped(false)
{
}

DataSourceSurfaceD2D1::~DataSourceSurfaceD2D1()
{
  if (mMapped) {
    mBitmap->Unmap();
  }
}

IntSize
DataSourceSurfaceD2D1::GetSize() const
{
  D2D1_SIZE_F size = mBitmap->GetSize();

  return IntSize(int32_t(size.width), int32_t(size.height));
}

uint8_t*
DataSourceSurfaceD2D1::GetData()
{
  EnsureMapped();

  return mMap.bits;
}

bool
DataSourceSurfaceD2D1::Map(MapType aMapType, MappedSurface *aMappedSurface)
{
  // DataSourceSurfaces used with the new Map API should not be used with GetData!!
  MOZ_ASSERT(!mMapped);
  MOZ_ASSERT(!mIsMapped);

  D2D1_MAP_OPTIONS options;
  if (aMapType == MapType::READ) {
    options = D2D1_MAP_OPTIONS_READ;
  } else {
    MOZ_CRASH("No support for Write maps on D2D1 DataSourceSurfaces yet!");
  }

  D2D1_MAPPED_RECT map;
  mBitmap->Map(D2D1_MAP_OPTIONS_READ, &map);
  aMappedSurface->mData = map.bits;
  aMappedSurface->mStride = map.pitch;

  mIsMapped = true;
  return true;
}

void
DataSourceSurfaceD2D1::Unmap()
{
  MOZ_ASSERT(mIsMapped);

  mIsMapped = false;
  mBitmap->Unmap();
}

int32_t
DataSourceSurfaceD2D1::Stride()
{
  EnsureMapped();

  return mMap.pitch;
}

void
DataSourceSurfaceD2D1::EnsureMapped()
{
  // Do not use GetData() after having used Map!
  MOZ_ASSERT(!mIsMapped);
  if (mMapped) {
    return;
  }
  mBitmap->Map(D2D1_MAP_OPTIONS_READ, &mMap);
  mMapped = true;
}

}
}
