/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_UTILS_H
#define GFX_UTILS_H

#include "gfxTypes.h"
#include "GraphicsFilter.h"
#include "imgIContainer.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/RefPtr.h"
#include "nsPrintfCString.h"

class gfxASurface;
class gfxDrawable;
class nsIntRegion;
struct nsIntRect;

namespace mozilla {
namespace layers {
struct PlanarYCbCrData;
}
}

class gfxUtils {
public:
    typedef mozilla::gfx::DataSourceSurface DataSourceSurface;
    typedef mozilla::gfx::IntPoint IntPoint;
    typedef mozilla::gfx::Matrix Matrix;
    typedef mozilla::gfx::SourceSurface SourceSurface;
    typedef mozilla::gfx::SurfaceFormat SurfaceFormat;

    /*
     * Premultiply or Unpremultiply aSourceSurface, writing the result
     * to aDestSurface or back into aSourceSurface if aDestSurface is null.
     *
     * If aDestSurface is given, it must have identical format, dimensions, and
     * stride as the source.
     *
     * If the source is not gfxImageFormat::ARGB32, no operation is performed.  If
     * aDestSurface is given, the data is copied over.
     */
    static bool PremultiplyDataSurface(DataSourceSurface* srcSurf,
                                       DataSourceSurface* destSurf);
    static bool UnpremultiplyDataSurface(DataSourceSurface* srcSurf,
                                         DataSourceSurface* destSurf);

    static mozilla::TemporaryRef<DataSourceSurface>
      CreatePremultipliedDataSurface(DataSourceSurface* srcSurf);
    static mozilla::TemporaryRef<DataSourceSurface>
      CreateUnpremultipliedDataSurface(DataSourceSurface* srcSurf);

    static void ConvertBGRAtoRGBA(uint8_t* aData, uint32_t aLength);

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
                                 const mozilla::gfx::SurfaceFormat aFormat,
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

    static Matrix TransformRectToRect(const gfxRect& aFrom,
                                      const IntPoint& aToTopLeft,
                                      const IntPoint& aToTopRight,
                                      const IntPoint& aToBottomRight);

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
     * @param aSuggestedFormat will be set to gfxImageFormat::RGB24
     *   if the desired format is not supported.
     * @param aSuggestedSize will be set to the picture size from aData
     *   if either the suggested size was {0,0}
     *   or simultaneous scaling and conversion is not supported.
     */
    static void
    GetYCbCrToRGBDestFormatAndSize(const mozilla::layers::PlanarYCbCrData& aData,
                                   gfxImageFormat& aSuggestedFormat,
                                   gfxIntSize& aSuggestedSize);

    /**
     * Convert YCbCrImage into RGB aDestBuffer
     * Format and Size parameters must have
     *   been passed to GetYCbCrToRGBDestFormatAndSize
     */
    static void
    ConvertYCbCrToRGB(const mozilla::layers::PlanarYCbCrData& aData,
                      const gfxImageFormat& aDestFormat,
                      const gfxIntSize& aDestSize,
                      unsigned char* aDestBuffer,
                      int32_t aStride);

    /**
     * Clears surface to transparent black.
     */
    static void ClearThebesSurface(gfxASurface* aSurface);

    /**
     * Creates a copy of aSurface, but having the SurfaceFormat aFormat.
     *
     * This function always creates a new surface. Do not call it if aSurface's
     * format is the same as aFormat. Such a non-conversion would just be an
     * unnecessary and wasteful copy (this function asserts to prevent that).
     *
     * This function is intended to be called by code that needs to access the
     * pixel data of the surface, but doesn't want to have lots of branches
     * to handle different pixel data formats (code which would become out of
     * date if and when new formats are added). Callers can use this function
     * to copy the surface to a specified format so that they only have to
     * handle pixel data in that one format.
     *
     * WARNING: There are format conversions that will not be supported by this
     * function. It very much depends on what the Moz2D backends support. If
     * the temporary B8G8R8A8 DrawTarget that this function creates has a
     * backend that supports DrawSurface() calls passing a surface with
     * aSurface's format it will work. Otherwise it will not.
     *
     *                      *** IMPORTANT PERF NOTE ***
     *
     * This function exists partly because format conversion is fraught with
     * non-obvious performance hazards, so we don't want Moz2D consumers to be
     * doing their own format conversion. Do not try to do so, or at least read
     * the comments in this functions implemtation. That said, the copy that
     * this function carries out has a cost and, although this function tries
     * to avoid perf hazards such as expensive uploads to/readbacks from the
     * GPU, it can't guarantee that it always successfully does so. Perf
     * critical code that can directly handle the common formats that it
     * encounters in a way that is cheaper than a copy-with-format-conversion
     * should consider doing so, and only use this function as a fallback to
     * handle other formats.
     *
     * XXXjwatt it would be nice if SourceSurface::GetDataSurface took a
     * SurfaceFormat argument (with a default argument meaning "use the
     * existing surface's format") and returned a DataSourceSurface in that
     * format. (There would then be an issue of callers maybe failing to
     * realize format conversion may involve expensive copying/uploading/
     * readback.)
     */
    static mozilla::TemporaryRef<DataSourceSurface>
    CopySurfaceToDataSourceSurfaceWithFormat(SourceSurface* aSurface,
                                             SurfaceFormat aFormat);

    static const uint8_t sUnpremultiplyTable[256*256];
    static const uint8_t sPremultiplyTable[256*256];

    /**
     * Return a color that can be used to identify a frame with a given frame number.
     * The colors will cycle after sNumFrameColors.  You can query colors 0 .. sNumFrameColors-1
     * to get all the colors back.
     */
    static const mozilla::gfx::Color& GetColorForFrameNumber(uint64_t aFrameNumber);
    static const uint32_t sNumFrameColors;

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

    static bool DumpPaintList();

    static bool sDumpPainting;
    static bool sDumpPaintingToFile;
    static FILE* sDumpPaintFile;

    /**
     * Writes a binary PNG file.
     * Expensive. Creates a DataSourceSurface, then a DrawTarget, then passes to DrawTarget overloads
     */
    static void WriteAsPNG(mozilla::gfx::SourceSurface* aSourceSurface, const char* aFile);

    /**
     * Write as a PNG encoded Data URL to stdout.
     * Expensive. Creates a DataSourceSurface, then a DrawTarget, then passes to DrawTarget overloads
     */
    static void DumpAsDataURL(mozilla::gfx::SourceSurface* aSourceSurface);

    /**
     * Copy a PNG encoded Data URL to the clipboard.
     * Expensive. Creates a DataSourceSurface, then a DrawTarget, then passes to DrawTarget overloads
     */
    static void CopyAsDataURL(mozilla::gfx::SourceSurface* aSourceSurface);
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
