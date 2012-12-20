/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "gfxAlphaRecovery.h"
#include "gfxImageSurface.h"

#include "cairo.h"
#include "mozilla/gfx/2D.h"
#include "gfx2DGlue.h"

using namespace mozilla::gfx;

gfxImageSurface::gfxImageSurface()
  : mSize(0, 0),
    mOwnsData(false),
    mFormat(ImageFormatUnknown),
    mStride(0)
{
}

void
gfxImageSurface::InitFromSurface(cairo_surface_t *csurf)
{
    mSize.width = cairo_image_surface_get_width(csurf);
    mSize.height = cairo_image_surface_get_height(csurf);
    mData = cairo_image_surface_get_data(csurf);
    mFormat = (gfxImageFormat) cairo_image_surface_get_format(csurf);
    mOwnsData = false;
    mStride = cairo_image_surface_get_stride(csurf);

    Init(csurf, true);
}

gfxImageSurface::gfxImageSurface(unsigned char *aData, const gfxIntSize& aSize,
                                 long aStride, gfxImageFormat aFormat)
{
    InitWithData(aData, aSize, aStride, aFormat);
}

void
gfxImageSurface::MakeInvalid()
{
    mSize = gfxIntSize(-1, -1);
    mData = NULL;
    mStride = 0;
}

void
gfxImageSurface::InitWithData(unsigned char *aData, const gfxIntSize& aSize,
                              long aStride, gfxImageFormat aFormat)
{
    mSize = aSize;
    mOwnsData = false;
    mData = aData;
    mFormat = aFormat;
    mStride = aStride;

    if (!CheckSurfaceSize(aSize))
        MakeInvalid();

    cairo_surface_t *surface =
        cairo_image_surface_create_for_data((unsigned char*)mData,
                                            (cairo_format_t)mFormat,
                                            mSize.width,
                                            mSize.height,
                                            mStride);

    // cairo_image_surface_create_for_data can return a 'null' surface
    // in out of memory conditions. The gfxASurface::Init call checks
    // the surface it receives to see if there is an error with the
    // surface and handles it appropriately. That is why there is
    // no check here.
    Init(surface);
}

static void*
TryAllocAlignedBytes(size_t aSize)
{
    // Use fallible allocators here
#if defined(HAVE_POSIX_MEMALIGN)
    void* ptr;
    // Try to align for fast alpha recovery.  This should only help
    // cairo too, can't hurt.
    return moz_posix_memalign(&ptr,
                              1 << gfxAlphaRecovery::GoodAlignmentLog2(),
                              aSize) ?
             nullptr : ptr;
#else
    // Oh well, hope that luck is with us in the allocator
    return moz_malloc(aSize);
#endif
}

gfxImageSurface::gfxImageSurface(const gfxIntSize& size, gfxImageFormat format, bool aClear) :
    mSize(size), mOwnsData(false), mData(nullptr), mFormat(format)
{
    mStride = ComputeStride();

    if (!CheckSurfaceSize(size))
        MakeInvalid();

    // if we have a zero-sized surface, just leave mData nullptr
    if (mSize.height * mStride > 0) {

        // This can fail to allocate memory aligned as we requested,
        // or it can fail to allocate any memory at all.
        mData = (unsigned char *) TryAllocAlignedBytes(mSize.height * mStride);
        if (!mData)
            return;
        if (aClear)
            memset(mData, 0, mSize.height * mStride);
    }

    mOwnsData = true;

    cairo_surface_t *surface =
        cairo_image_surface_create_for_data((unsigned char*)mData,
                                            (cairo_format_t)format,
                                            mSize.width,
                                            mSize.height,
                                            mStride);

    Init(surface);

    if (mSurfaceValid) {
        RecordMemoryUsed(mSize.height * ComputeStride() +
                         sizeof(gfxImageSurface));
    }
}

gfxImageSurface::gfxImageSurface(cairo_surface_t *csurf)
{
    mSize.width = cairo_image_surface_get_width(csurf);
    mSize.height = cairo_image_surface_get_height(csurf);
    mData = cairo_image_surface_get_data(csurf);
    mFormat = (gfxImageFormat) cairo_image_surface_get_format(csurf);
    mOwnsData = false;
    mStride = cairo_image_surface_get_stride(csurf);

    Init(csurf, true);
}

gfxImageSurface::~gfxImageSurface()
{
    if (mOwnsData)
        free(mData);
}

/*static*/ long
gfxImageSurface::ComputeStride(const gfxIntSize& aSize, gfxImageFormat aFormat)
{
    long stride;

    if (aFormat == ImageFormatARGB32)
        stride = aSize.width * 4;
    else if (aFormat == ImageFormatRGB24)
        stride = aSize.width * 4;
    else if (aFormat == ImageFormatRGB16_565)
        stride = aSize.width * 2;
    else if (aFormat == ImageFormatA8)
        stride = aSize.width;
    else if (aFormat == ImageFormatA1) {
        stride = (aSize.width + 7) / 8;
    } else {
        NS_WARNING("Unknown format specified to gfxImageSurface!");
        stride = aSize.width * 4;
    }

    stride = ((stride + 3) / 4) * 4;

    return stride;
}

size_t
gfxImageSurface::SizeOfExcludingThis(nsMallocSizeOfFun aMallocSizeOf) const
{
    size_t n = gfxASurface::SizeOfExcludingThis(aMallocSizeOf);
    if (mOwnsData) {
        n += aMallocSizeOf(mData);
    }
    return n;
}

size_t
gfxImageSurface::SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf) const
{
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}

bool
gfxImageSurface::SizeOfIsMeasured() const
{
    return true;
}

// helper function for the CopyFrom methods
static void
CopyForStride(unsigned char* aDest, unsigned char* aSrc, const gfxIntSize& aSize, long aDestStride, long aSrcStride)
{
    if (aDestStride == aSrcStride) {
        memcpy (aDest, aSrc, aSrcStride * aSize.height);
    } else {
        int lineSize = NS_MIN(aDestStride, aSrcStride);
        for (int i = 0; i < aSize.height; i++) {
            unsigned char* src = aSrc + aSrcStride * i;
            unsigned char* dst = aDest + aDestStride * i;

            memcpy (dst, src, lineSize);
        }
    }
}

// helper function for the CopyFrom methods
static bool
FormatsAreCompatible(gfxASurface::gfxImageFormat a1, gfxASurface::gfxImageFormat a2)
{
    if (a1 != a2 &&
        !(a1 == gfxASurface::ImageFormatARGB32 &&
          a2 == gfxASurface::ImageFormatRGB24) &&
        !(a1 == gfxASurface::ImageFormatRGB24 &&
          a2 == gfxASurface::ImageFormatARGB32)) {
        return false;
    }

    return true;
}

bool
gfxImageSurface::CopyFrom (SourceSurface *aSurface)
{
    mozilla::RefPtr<DataSourceSurface> data = aSurface->GetDataSurface();

    if (!data) {
        return false;
    }

    gfxIntSize size(data->GetSize().width, data->GetSize().height);
    if (size != mSize) {
        return false;
    }

    if (!FormatsAreCompatible(SurfaceFormatToImageFormat(aSurface->GetFormat()),
                              mFormat)) {
        return false;
    }

    CopyForStride(mData, data->GetData(), size, mStride, data->Stride());

    return true;
}


bool
gfxImageSurface::CopyFrom(gfxImageSurface *other)
{
    if (other->mSize != mSize) {
        return false;
    }

    if (!FormatsAreCompatible(other->mFormat, mFormat)) {
        return false;
    }

    CopyForStride(mData, other->mData, mSize, mStride, other->mStride);

    return true;
}

already_AddRefed<gfxSubimageSurface>
gfxImageSurface::GetSubimage(const gfxRect& aRect)
{
    gfxRect r(aRect);
    r.Round();
    unsigned char* subData = Data() +
        (Stride() * (int)r.Y()) +
        (int)r.X() * gfxASurface::BytePerPixelFromFormat(Format());

    nsRefPtr<gfxSubimageSurface> image =
        new gfxSubimageSurface(this, subData,
                               gfxIntSize((int)r.Width(), (int)r.Height()));

    return image.forget().get();
}

gfxSubimageSurface::gfxSubimageSurface(gfxImageSurface* aParent,
                                       unsigned char* aData,
                                       const gfxIntSize& aSize)
  : gfxImageSurface(aData, aSize, aParent->Stride(), aParent->Format())
  , mParent(aParent)
{
}

already_AddRefed<gfxImageSurface>
gfxImageSurface::GetAsImageSurface()
{
  nsRefPtr<gfxImageSurface> surface = this;
  return surface.forget();
}

void
gfxImageSurface::MovePixels(const nsIntRect& aSourceRect,
                            const nsIntPoint& aDestTopLeft)
{
    const nsIntRect bounds(0, 0, mSize.width, mSize.height);
    nsIntPoint offset = aDestTopLeft - aSourceRect.TopLeft(); 
    nsIntRect clippedSource = aSourceRect;
    clippedSource.IntersectRect(clippedSource, bounds);
    nsIntRect clippedDest = clippedSource + offset;
    clippedDest.IntersectRect(clippedDest, bounds);
    const nsIntRect dest = clippedDest;
    const nsIntRect source = dest - offset;
    // NB: this relies on IntersectRect() and operator+/- preserving
    // x/y for empty rectangles
    NS_ABORT_IF_FALSE(bounds.Contains(dest) && bounds.Contains(source) &&
                      aSourceRect.Contains(source) &&
                      nsIntRect(aDestTopLeft, aSourceRect.Size()).Contains(dest) &&
                      source.Size() == dest.Size() &&
                      offset == (dest.TopLeft() - source.TopLeft()),
                      "Messed up clipping, crash or corruption will follow");
    if (source.IsEmpty() || source.IsEqualInterior(dest)) {
        return;
    }

    long naturalStride = ComputeStride(mSize, mFormat);
    if (mStride == naturalStride && dest.width == bounds.width) {
        // Fast path: this is a vertical shift of some rows in a
        // "normal" image surface.  We can directly memmove and
        // hopefully stay in SIMD land.
        unsigned char* dst = mData + dest.y * mStride;
        const unsigned char* src = mData + source.y * mStride;
        size_t nBytes = dest.height * mStride;
        memmove(dst, src, nBytes);
        return;
    }

    // Slow(er) path: have to move row-by-row.
    const int32_t bpp = BytePerPixelFromFormat(mFormat);
    const size_t nRowBytes = dest.width * bpp;
    // dstRow points at the first pixel within the current destination
    // row, and similarly for srcRow.  endSrcRow is one row beyond the
    // last row we need to copy.  stride is either +mStride or
    // -mStride, depending on which direction we're copying.
    unsigned char* dstRow;
    unsigned char* srcRow;
    unsigned char* endSrcRow;   // NB: this may point outside the image
    long stride;
    if (dest.y > source.y) {
        // We're copying down from source to dest, so walk backwards
        // starting from the last rows to avoid stomping pixels we
        // need.
        stride = -mStride;
        dstRow = mData + dest.x * bpp + (dest.YMost() - 1) * mStride;
        srcRow = mData + source.x * bpp + (source.YMost() - 1) * mStride;
        endSrcRow = mData + source.x * bpp + (source.y - 1) * mStride;
    } else {
        stride = mStride;
        dstRow = mData + dest.x * bpp + dest.y * mStride;
        srcRow = mData + source.x * bpp + source.y * mStride;
        endSrcRow = mData + source.x * bpp + source.YMost() * mStride;
    }

    for (; srcRow != endSrcRow; dstRow += stride, srcRow += stride) {
        memmove(dstRow, srcRow, nRowBytes);
    }
}
