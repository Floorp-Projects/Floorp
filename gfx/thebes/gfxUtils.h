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
 * The Original Code is Mozilla Corporation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Robert O'Callahan <robert@ocallahan.org>
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

#ifndef GFX_UTILS_H
#define GFX_UTILS_H

#include "gfxTypes.h"
#include "gfxPattern.h"
#include "gfxImageSurface.h"
#include "ImageLayers.h"
#include "mozilla/gfx/2D.h"
#include "imgIContainer.h"

class gfxDrawable;
class nsIntRegion;
struct nsIntRect;

class THEBES_API gfxUtils {
public:
    /*
     * Premultiply or Unpremultiply aSourceSurface, writing the result
     * to aDestSurface or back into aSourceSurface if aDestSurface is null.
     *
     * If aDestSurface is given, it must have identical format, dimensions, and
     * stride as the source.
     *
     * If the source is not ImageFormatARGB32, no operation is performed.  If
     * aDestSurface is given, the data is copied over.
     */
    static void PremultiplyImageSurface(gfxImageSurface *aSourceSurface,
                                        gfxImageSurface *aDestSurface = nsnull);
    static void UnpremultiplyImageSurface(gfxImageSurface *aSurface,
                                          gfxImageSurface *aDestSurface = nsnull);

    static void ConvertBGRAtoRGBA(gfxImageSurface *aSourceSurface,
                                  gfxImageSurface *aDestSurface = nsnull);

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
                                 const gfxImageSurface::gfxImageFormat aFormat,
                                 const gfxPattern::GraphicsFilter& aFilter,
                                 PRUint32         aImageFlags = imgIContainer::FLAG_NONE);

    /**
     * Clip aContext to the region aRegion.
     */
    static void ClipToRegion(gfxContext* aContext, const nsIntRegion& aRegion);

    /**
     * Clip aContext to the region aRegion, snapping the rectangles.
     */
    static void ClipToRegionSnapped(gfxContext* aContext, const nsIntRegion& aRegion);

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
    static int ImageFormatToDepth(gfxASurface::gfxImageFormat aFormat);

    /**
     * If aIn can be represented exactly using an nsIntRect (i.e.
     * integer-aligned edges and coordinates in the PRInt32 range) then we
     * set aOut to that rectangle, otherwise return failure.
    */
    static bool GfxRectToIntRect(const gfxRect& aIn, nsIntRect* aOut);

    /**
     * If aIn can be represented exactly using an nsIntRect (i.e.
     * integer-aligned edges and coordinates in the PRInt32 range) then we
     * set aOut to that rectangle, otherwise return failure.
    */
    static bool RectToIntRect(const mozilla::gfx::Rect& aIn, mozilla::gfx::IntRect* aOut);

    /**
     * Return the smallest power of kScaleResolution (2) greater than or equal to
     * aVal.
     */
    static gfxFloat ClampToScaleFactor(gfxFloat aVal);

    /**
     * Helper function for ConvertYCbCrToRGB that finds the
     * RGB buffer size and format for given YCbCrImage.
     * @param aSuggestedFormat will be set to ImageFormatRGB24
     *   if the desired format is not supported.
     * @param aSuggestedSize will be set to the picture size from aData
     *   if either the suggested size was {0,0}
     *   or simultaneous scaling and conversion is not supported.
     */
    static void
    GetYCbCrToRGBDestFormatAndSize(const mozilla::layers::PlanarYCbCrImage::Data& aData,
                                   gfxASurface::gfxImageFormat& aSuggestedFormat,
                                   gfxIntSize& aSuggestedSize);

    /**
     * Convert YCbCrImage into RGB aDestBuffer
     * Format and Size parameters must have
     *   been passed to GetYCbCrToRGBDestFormatAndSize
     */
    static void
    ConvertYCbCrToRGB(const mozilla::layers::PlanarYCbCrImage::Data& aData,
                      const gfxASurface::gfxImageFormat& aDestFormat,
                      const gfxIntSize& aDestSize,
                      unsigned char* aDestBuffer,
                      PRInt32 aStride);

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


/*
 * Copyright 2008 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Returns true if |aNumber| is a power of two
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
