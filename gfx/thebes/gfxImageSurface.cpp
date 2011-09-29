/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Oracle Corporation code.
 *
 * The Initial Developer of the Original Code is Oracle Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <pavlov@pavlov.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "prmem.h"

#include "gfxAlphaRecovery.h"
#include "gfxImageSurface.h"

#include "cairo.h"

gfxImageSurface::gfxImageSurface()
  : mSize(0, 0),
    mOwnsData(PR_FALSE),
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
    mOwnsData = PR_FALSE;
    mStride = cairo_image_surface_get_stride(csurf);

    Init(csurf, PR_TRUE);
}

gfxImageSurface::gfxImageSurface(unsigned char *aData, const gfxIntSize& aSize,
                                 long aStride, gfxImageFormat aFormat)
{
    InitWithData(aData, aSize, aStride, aFormat);
}

void
gfxImageSurface::InitWithData(unsigned char *aData, const gfxIntSize& aSize,
                              long aStride, gfxImageFormat aFormat)
{
    mSize = aSize;
    mOwnsData = PR_FALSE;
    mData = aData;
    mFormat = aFormat;
    mStride = aStride;

    if (!CheckSurfaceSize(aSize))
        return;

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
#if defined(HAVE_POSIX_MEMALIGN) || defined(HAVE_JEMALLOC_POSIX_MEMALIGN)
    void* ptr;
    // Try to align for fast alpha recovery.  This should only help
    // cairo too, can't hurt.
    return moz_posix_memalign(&ptr,
                              1 << gfxAlphaRecovery::GoodAlignmentLog2(),
                              aSize) ?
             nsnull : ptr;
#else
    // Oh well, hope that luck is with us in the allocator
    return moz_malloc(aSize);
#endif
}

gfxImageSurface::gfxImageSurface(const gfxIntSize& size, gfxImageFormat format) :
    mSize(size), mOwnsData(PR_FALSE), mData(nsnull), mFormat(format)
{
    mStride = ComputeStride();

    if (!CheckSurfaceSize(size))
        return;

    // if we have a zero-sized surface, just leave mData nsnull
    if (mSize.height * mStride > 0) {

        // This can fail to allocate memory aligned as we requested,
        // or it can fail to allocate any memory at all.
        mData = (unsigned char *) TryAllocAlignedBytes(mSize.height * mStride);
        if (!mData)
            return;
        memset(mData, 0, mSize.height * mStride);
    }

    mOwnsData = PR_TRUE;

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
    mOwnsData = PR_FALSE;
    mStride = cairo_image_surface_get_stride(csurf);

    Init(csurf, PR_TRUE);
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

bool
gfxImageSurface::CopyFrom(gfxImageSurface *other)
{
    if (other->mSize != mSize)
    {
        return PR_FALSE;
    }

    if (other->mFormat != mFormat &&
        !(other->mFormat == ImageFormatARGB32 && mFormat == ImageFormatRGB24) &&
        !(other->mFormat == ImageFormatRGB24 && mFormat == ImageFormatARGB32))
    {
        return PR_FALSE;
    }

    if (other->mStride == mStride) {
        memcpy (mData, other->mData, mStride * mSize.height);
    } else {
        int lineSize = NS_MIN(other->mStride, mStride);
        for (int i = 0; i < mSize.height; i++) {
            unsigned char *src = other->mData + other->mStride * i;
            unsigned char *dst = mData + mStride * i;

            memcpy (dst, src, lineSize);
        }
    }

    return PR_TRUE;
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
    const PRInt32 bpp = BytePerPixelFromFormat(mFormat);
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
