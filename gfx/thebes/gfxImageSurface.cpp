/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/MemoryReporting.h"
#if defined(HAVE_POSIX_MEMALIGN)
#  include "gfxAlphaRecovery.h"
#endif
#include "gfxImageSurface.h"

#include "cairo.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/HelpersCairo.h"
#include "gfx2DGlue.h"
#include <algorithm>

using namespace mozilla;
using namespace mozilla::gfx;

gfxImageSurface::gfxImageSurface()
    : mSize(0, 0),
      mOwnsData(false),
      mData(nullptr),
      mFormat(SurfaceFormat::UNKNOWN),
      mStride(0) {}

void gfxImageSurface::InitFromSurface(cairo_surface_t* csurf) {
  if (!csurf || cairo_surface_status(csurf)) {
    MakeInvalid();
    return;
  }

  mSize.width = cairo_image_surface_get_width(csurf);
  mSize.height = cairo_image_surface_get_height(csurf);
  mData = cairo_image_surface_get_data(csurf);
  mFormat = CairoFormatToGfxFormat(cairo_image_surface_get_format(csurf));
  mOwnsData = false;
  mStride = cairo_image_surface_get_stride(csurf);

  Init(csurf, true);
}

gfxImageSurface::gfxImageSurface(unsigned char* aData, const IntSize& aSize,
                                 long aStride, gfxImageFormat aFormat) {
  InitWithData(aData, aSize, aStride, aFormat);
}

void gfxImageSurface::MakeInvalid() {
  mSize = IntSize(-1, -1);
  mData = nullptr;
  mStride = 0;
}

void gfxImageSurface::InitWithData(unsigned char* aData, const IntSize& aSize,
                                   long aStride, gfxImageFormat aFormat) {
  mSize = aSize;
  mOwnsData = false;
  mData = aData;
  mFormat = aFormat;
  mStride = aStride;

  if (!Factory::CheckSurfaceSize(aSize)) MakeInvalid();

  cairo_format_t cformat = GfxFormatToCairoFormat(mFormat);
  cairo_surface_t* surface = cairo_image_surface_create_for_data(
      (unsigned char*)mData, cformat, mSize.width, mSize.height, mStride);

  // cairo_image_surface_create_for_data can return a 'null' surface
  // in out of memory conditions. The gfxASurface::Init call checks
  // the surface it receives to see if there is an error with the
  // surface and handles it appropriately. That is why there is
  // no check here.
  Init(surface);
}

static void* TryAllocAlignedBytes(size_t aSize) {
  // Use fallible allocators here
#if defined(HAVE_POSIX_MEMALIGN)
  void* ptr;
  // Try to align for fast alpha recovery.  This should only help
  // cairo too, can't hurt.
  return posix_memalign(&ptr, 1 << gfxAlphaRecovery::GoodAlignmentLog2(), aSize)
             ? nullptr
             : ptr;
#else
  // Oh well, hope that luck is with us in the allocator
  return malloc(aSize);
#endif
}

gfxImageSurface::gfxImageSurface(const IntSize& size, gfxImageFormat format,
                                 bool aClear)
    : mSize(size), mData(nullptr), mFormat(format) {
  AllocateAndInit(0, 0, aClear);
}

void gfxImageSurface::AllocateAndInit(long aStride, int32_t aMinimalAllocation,
                                      bool aClear) {
  // The callers should set mSize and mFormat.
  MOZ_ASSERT(!mData);
  mData = nullptr;
  mOwnsData = false;

  mStride = aStride > 0 ? aStride : ComputeStride();
  if (aMinimalAllocation < mSize.height * mStride)
    aMinimalAllocation = mSize.height * mStride;

  if (!Factory::CheckSurfaceSize(mSize)) MakeInvalid();

  // if we have a zero-sized surface, just leave mData nullptr
  if (mSize.height * mStride > 0) {
    // This can fail to allocate memory aligned as we requested,
    // or it can fail to allocate any memory at all.
    mData = (unsigned char*)TryAllocAlignedBytes(aMinimalAllocation);
    if (!mData) return;
    if (aClear) memset(mData, 0, aMinimalAllocation);
  }

  mOwnsData = true;

  cairo_format_t cformat = GfxFormatToCairoFormat(mFormat);
  cairo_surface_t* surface = cairo_image_surface_create_for_data(
      (unsigned char*)mData, cformat, mSize.width, mSize.height, mStride);

  Init(surface);

  if (mSurfaceValid) {
    RecordMemoryUsed(mSize.height * ComputeStride() + sizeof(gfxImageSurface));
  }
}

gfxImageSurface::gfxImageSurface(const IntSize& size, gfxImageFormat format,
                                 long aStride, int32_t aExtraBytes, bool aClear)
    : mSize(size), mData(nullptr), mFormat(format) {
  AllocateAndInit(aStride, aExtraBytes, aClear);
}

gfxImageSurface::gfxImageSurface(cairo_surface_t* csurf) {
  mSize.width = cairo_image_surface_get_width(csurf);
  mSize.height = cairo_image_surface_get_height(csurf);
  mData = cairo_image_surface_get_data(csurf);
  mFormat = CairoFormatToGfxFormat(cairo_image_surface_get_format(csurf));
  mOwnsData = false;
  mStride = cairo_image_surface_get_stride(csurf);

  Init(csurf, true);
}

gfxImageSurface::~gfxImageSurface() {
  if (mOwnsData) free(mData);
}

/*static*/
long gfxImageSurface::ComputeStride(const IntSize& aSize,
                                    gfxImageFormat aFormat) {
  long stride;

  if (aFormat == SurfaceFormat::A8R8G8B8_UINT32)
    stride = aSize.width * 4;
  else if (aFormat == SurfaceFormat::X8R8G8B8_UINT32)
    stride = aSize.width * 4;
  else if (aFormat == SurfaceFormat::R5G6B5_UINT16)
    stride = aSize.width * 2;
  else if (aFormat == SurfaceFormat::A8)
    stride = aSize.width;
  else {
    NS_WARNING("Unknown format specified to gfxImageSurface!");
    stride = aSize.width * 4;
  }

  stride = ((stride + 3) / 4) * 4;

  return stride;
}

size_t gfxImageSurface::SizeOfExcludingThis(
    mozilla::MallocSizeOf aMallocSizeOf) const {
  size_t n = gfxASurface::SizeOfExcludingThis(aMallocSizeOf);
  if (mOwnsData) {
    n += aMallocSizeOf(mData);
  }
  return n;
}

size_t gfxImageSurface::SizeOfIncludingThis(
    mozilla::MallocSizeOf aMallocSizeOf) const {
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}

bool gfxImageSurface::SizeOfIsMeasured() const { return true; }

// helper function for the CopyFrom methods
static void CopyForStride(unsigned char* aDest, unsigned char* aSrc,
                          const IntSize& aSize, long aDestStride,
                          long aSrcStride) {
  if (aDestStride == aSrcStride) {
    memcpy(aDest, aSrc, aSrcStride * aSize.height);
  } else {
    int lineSize = std::min(aDestStride, aSrcStride);
    for (int i = 0; i < aSize.height; i++) {
      unsigned char* src = aSrc + aSrcStride * i;
      unsigned char* dst = aDest + aDestStride * i;

      memcpy(dst, src, lineSize);
    }
  }
}

// helper function for the CopyFrom methods
static bool FormatsAreCompatible(gfxImageFormat a1, gfxImageFormat a2) {
  if (a1 != a2 &&
      !(a1 == SurfaceFormat::A8R8G8B8_UINT32 &&
        a2 == SurfaceFormat::X8R8G8B8_UINT32) &&
      !(a1 == SurfaceFormat::X8R8G8B8_UINT32 &&
        a2 == SurfaceFormat::A8R8G8B8_UINT32)) {
    return false;
  }

  return true;
}

bool gfxImageSurface::CopyFrom(SourceSurface* aSurface) {
  RefPtr<DataSourceSurface> data = aSurface->GetDataSurface();

  if (!data) {
    return false;
  }

  IntSize size(data->GetSize().width, data->GetSize().height);
  if (size != mSize) {
    return false;
  }

  if (!FormatsAreCompatible(SurfaceFormatToImageFormat(aSurface->GetFormat()),
                            mFormat)) {
    return false;
  }

  DataSourceSurface::ScopedMap map(data, DataSourceSurface::READ);
  CopyForStride(mData, map.GetData(), size, mStride, map.GetStride());

  return true;
}

bool gfxImageSurface::CopyFrom(gfxImageSurface* other) {
  if (other->mSize != mSize) {
    return false;
  }

  if (!FormatsAreCompatible(other->mFormat, mFormat)) {
    return false;
  }

  CopyForStride(mData, other->mData, mSize, mStride, other->mStride);

  return true;
}

bool gfxImageSurface::CopyTo(SourceSurface* aSurface) {
  RefPtr<DataSourceSurface> data = aSurface->GetDataSurface();

  if (!data) {
    return false;
  }

  IntSize size(data->GetSize().width, data->GetSize().height);
  if (size != mSize) {
    return false;
  }

  if (!FormatsAreCompatible(SurfaceFormatToImageFormat(aSurface->GetFormat()),
                            mFormat)) {
    return false;
  }

  DataSourceSurface::ScopedMap map(data, DataSourceSurface::READ_WRITE);
  CopyForStride(map.GetData(), mData, size, map.GetStride(), mStride);

  return true;
}

already_AddRefed<DataSourceSurface>
gfxImageSurface::CopyToB8G8R8A8DataSourceSurface() {
  RefPtr<DataSourceSurface> dataSurface = Factory::CreateDataSourceSurface(
      IntSize(GetSize().width, GetSize().height), SurfaceFormat::B8G8R8A8);
  if (dataSurface) {
    CopyTo(dataSurface);
  }
  return dataSurface.forget();
}

already_AddRefed<gfxSubimageSurface> gfxImageSurface::GetSubimage(
    const gfxRect& aRect) {
  gfxRect r(aRect);
  r.Round();
  MOZ_ASSERT(gfxRect(0, 0, mSize.width, mSize.height).Contains(r));

  gfxImageFormat format = Format();

  unsigned char* subData =
      Data() + (Stride() * (int)r.Y()) +
      (int)r.X() * gfxASurface::BytePerPixelFromFormat(Format());

  if (format == SurfaceFormat::A8R8G8B8_UINT32 &&
      GetOpaqueRect().Contains(aRect)) {
    format = SurfaceFormat::X8R8G8B8_UINT32;
  }

  RefPtr<gfxSubimageSurface> image = new gfxSubimageSurface(
      this, subData, IntSize((int)r.Width(), (int)r.Height()), format);

  return image.forget();
}

gfxSubimageSurface::gfxSubimageSurface(gfxImageSurface* aParent,
                                       unsigned char* aData,
                                       const IntSize& aSize,
                                       gfxImageFormat aFormat)
    : gfxImageSurface(aData, aSize, aParent->Stride(), aFormat),
      mParent(aParent) {}

already_AddRefed<gfxImageSurface> gfxImageSurface::GetAsImageSurface() {
  RefPtr<gfxImageSurface> surface = this;
  return surface.forget();
}
