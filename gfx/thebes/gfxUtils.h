/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_UTILS_H
#define GFX_UTILS_H

#include "gfxTypes.h"
#include "GraphicsFilter.h"
#include "gfxImageSurface.h"
#include "ImageContainer.h"
#include "mozilla/gfx/2D.h"
#include "imgIContainer.h"

class gfxDrawable;
class nsIntRegion;
struct nsIntRect;

class gfxUtils {
public:
    /*
     * Premultiply or Unpremultiply aSourceSurface, writing the result
     * to aDestSurface or back into aSourceSurface if aDestSurface is null.
     *
     * If aDestSurface is given, it must have identical format, dimensions, and
     * stride as the source.
     *
     * If the source is not gfxImageFormatARGB32, no operation is performed.  If
     * aDestSurface is given, the data is copied over.
     */
    static void PremultiplyImageSurface(gfxImageSurface *aSourceSurface,
                                        gfxImageSurface *aDestSurface = nullptr);
    static void UnpremultiplyImageSurface(gfxImageSurface *aSurface,
                                          gfxImageSurface *aDestSurface = nullptr);

    static void ConvertBGRAtoRGBA(gfxImageSurface *aSourceSurface,
                                  gfxImageSurface *aDestSurface = nullptr);

    /**
     * Draw something drawable while working around limitations like bad support
     * for EXTEND_PAD, lack of source-clipping, or cairo / pixman bugs with
     * extreme user-space-to-image-space transforms.
     *
     * The input parameters here usually come from the output of our image
     * snapping algorithm in nsLayoutUtils.cpp.
     * This method is split from nsLayoutUtils::DrawPixelSnapped to allow for
     * adjusting the parameters. For example, certain images with transparent
     * margins only have a drawable subimage. For those images, imgFrame::Draw
     * will tweak the rects and transforms that it gets from the pixel snapping
     * algorithm before passing them on to this method.
     */
    static void DrawPixelSnapped(gfxContext*      aContext,
                                 gfxDrawable*     aDrawable,
                                 const gfxMatrix& aUserSpaceToImageSpace,
                                 const gfxRect&   aSubimage,
                                 const gfxRect&   aSourceRect,
                                 const gfxRect&   aImageRect,
                                 const gfxRect&   aFill,
                                 const gfxImageFormat aFormat,
                                 GraphicsFilter aFilter,
                                 uint32_t         aImageFlags = imgIContainer::FLAG_NONE);

    /**
     * Clip aContext to the region aRegion.
     */
    static void ClipToRegion(gfxContext* aContext, const nsIntRegion& aRegion);

    /**
     * Clip aTarget to the region aRegion.
     */
    static void ClipToRegion(mozilla::gfx::DrawTarget* aTarget, const nsIntRegion& aRegion);

    /**
     * Clip aContext to the region aRegion, snapping the rectangles.
     */
    static void ClipToRegionSnapped(gfxContext* aContext, const nsIntRegion& aRegion);

    /**
     * Clip aTarget to the region aRegion, snapping the rectangles.
     */
    static void ClipToRegionSnapped(mozilla::gfx::DrawTarget* aTarget, const nsIntRegion& aRegion);

    /**
     * Create a path consisting of rectangles in |aRegion|.
     */
    static void PathFromRegion(gfxContext* aContext, const nsIntRegion& aRegion);

    /**
     * Create a path consisting of rectangles in |aRegion|, snapping the rectangles.
     */
    static void PathFromRegionSnapped(gfxContext* aContext, const nsIntRegion& aRegion);

    /*
     * Convert image format to depth value
     */
    static int ImageFormatToDepth(gfxImageFormat aFormat);

    /**
     * Return the transform matrix that maps aFrom to the rectangle defined by
     * aToTopLeft/aToTopRight/aToBottomRight. aFrom must be
     * nonempty and the destination rectangle must be axis-aligned.
     */
    static gfxMatrix TransformRectToRect(const gfxRect& aFrom,
                                         const gfxPoint& aToTopLeft,
                                         const gfxPoint& aToTopRight,
                                         const gfxPoint& aToBottomRight);

    /**
     * If aIn can be represented exactly using an nsIntRect (i.e.
     * integer-aligned edges and coordinates in the int32_t range) then we
     * set aOut to that rectangle, otherwise return failure.
    */
    static bool GfxRectToIntRect(const gfxRect& aIn, nsIntRect* aOut);

    /**
     * Return the smallest power of kScaleResolution (2) greater than or equal to
     * aVal.
     */
    static gfxFloat ClampToScaleFactor(gfxFloat aVal);

    /**
     * Helper function for ConvertYCbCrToRGB that finds the
     * RGB buffer size and format for given YCbCrImage.
     * @param aSuggestedFormat will be set to gfxImageFormatRGB24
     *   if the desired format is not supported.
     * @param aSuggestedSize will be set to the picture size from aData
     *   if either the suggested size was {0,0}
     *   or simultaneous scaling and conversion is not supported.
     */
    static void
    GetYCbCrToRGBDestFormatAndSize(const mozilla::layers::PlanarYCbCrImage::Data& aData,
                                   gfxImageFormat& aSuggestedFormat,
                                   gfxIntSize& aSuggestedSize);

    /**
     * Convert YCbCrImage into RGB aDestBuffer
     * Format and Size parameters must have
     *   been passed to GetYCbCrToRGBDestFormatAndSize
     */
    static void
    ConvertYCbCrToRGB(const mozilla::layers::PlanarYCbCrImage::Data& aData,
                      const gfxImageFormat& aDestFormat,
                      const gfxIntSize& aDestSize,
                      unsigned char* aDestBuffer,
                      int32_t aStride);

    static const uint8_t sUnpremultiplyTable[256*256];
    static const uint8_t sPremultiplyTable[256*256];
#ifdef MOZ_DUMP_PAINTING
    /**
     * Writes a binary PNG file.
     */
    static void WriteAsPNG(mozilla::gfx::DrawTarget* aDT, const char* aFile);

    /**
     * Write as a PNG encoded Data URL to stdout.
     */
    static void DumpAsDataURL(mozilla::gfx::DrawTarget* aDT);

    /**
     * Copy a PNG encoded Data URL to the clipboard.
     */
    static void CopyAsDataURL(mozilla::gfx::DrawTarget* aDT);

    static bool sDumpPaintList;
    static bool sDumpPainting;
    static bool sDumpPaintingToFile;
    static FILE* sDumpPaintFile;
#endif
};

namespace mozilla {
namespace gfx {


/* These techniques are suggested by "Bit Twiddling Hacks"
 */

/**
 * Returns true if |aNumber| is a power of two
 * 0 is incorreclty considered a power of two
 */
static inline bool
IsPowerOfTwo(int aNumber)
{
    return (aNumber & (aNumber - 1)) == 0;
}

/**
 * Returns the first integer greater than |aNumber| which is a power of two
 * Undefined for |aNumber| < 0
 */
static inline int
NextPowerOfTwo(int aNumber)
{
#if defined(__arm__)
    return 1 << (32 - __builtin_clz(aNumber - 1));
#else
    --aNumber;
    aNumber |= aNumber >> 1;
    aNumber |= aNumber >> 2;
    aNumber |= aNumber >> 4;
    aNumber |= aNumber >> 8;
    aNumber |= aNumber >> 16;
    return ++aNumber;
#endif
}

} // namespace gfx
} // namespace mozilla

#endif
