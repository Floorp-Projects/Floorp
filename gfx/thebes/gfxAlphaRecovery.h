/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _GFXALPHARECOVERY_H_
#define _GFXALPHARECOVERY_H_

#include "gfxContext.h"
#include "gfxImageSurface.h"
#include "mozilla/SSE.h"
#include "nsRect.h"

class THEBES_API gfxAlphaRecovery {
public:
    struct Analysis {
        bool uniformColor;
        bool uniformAlpha;
        gfxFloat alpha;
        gfxFloat r, g, b;
    };

    /**
     * Some SIMD fast-paths only can be taken if the relative
     * byte-alignment of images' pointers and strides meets certain
     * criteria.  Aligning image pointers and strides by
     * |GoodAlignmentLog2()| below will ensure that fast-paths aren't
     * skipped because of misalignment.  Fast-paths may still be taken
     * even if GoodAlignmentLog2() is not met, in some conditions.
     */
    static PRUint32 GoodAlignmentLog2() { return 4; /* for SSE2 */ }

    /* Given two surfaces of equal size with the same rendering, one onto a
     * black background and the other onto white, recovers alpha values from
     * the difference and sets the alpha values on the black surface.
     * The surfaces must have format RGB24 or ARGB32.
     * Returns true on success.
     */
    static bool RecoverAlpha (gfxImageSurface *blackSurface,
                                const gfxImageSurface *whiteSurface,
                                Analysis *analysis = nullptr);

#ifdef MOZILLA_MAY_SUPPORT_SSE2
    /* This does the same as the previous function, but uses SSE2
     * optimizations. Usually this should not be called directly.  Be sure to
     * check mozilla::supports_sse2() before calling this function.
     */
    static bool RecoverAlphaSSE2 (gfxImageSurface *blackSurface,
                                    const gfxImageSurface *whiteSurface);

    /**
     * A common use-case for alpha recovery is to paint into a
     * temporary "white image", then paint onto a subrect of the
     * surface, the "black image", into which alpha-recovered pixels
     * are eventually to be written.  This function returns a rect
     * aligned so that recovering alpha for that rect will hit SIMD
     * fast-paths, if possible.  It's not always possible to align
     * |aRect| so that fast-paths will be taken.
     *
     * The returned rect is always a superset of |aRect|.
     */
    static nsIntRect AlignRectForSubimageRecovery(const nsIntRect& aRect,
                                                  gfxImageSurface* aSurface);
#else
    static nsIntRect AlignRectForSubimageRecovery(const nsIntRect& aRect,
                                                  gfxImageSurface*)
    { return aRect; }
#endif

    /** from cairo-xlib-utils.c, modified */
    /**
     * Given the RGB data for two image surfaces, one a source image composited
     * with OVER onto a black background, and one a source image composited with 
     * OVER onto a white background, reconstruct the original image data into
     * black_data.
     *
     * Consider a single color channel and a given pixel. Suppose the original
     * premultiplied color value was C and the alpha value was A. Let the final
     * on-black color be B and the final on-white color be W. All values range
     * over 0-255.
     *
     * Then B=C and W=(255*(255 - A) + C*255)/255. Solving for A, we get
     * A=255 - (W - C). Therefore it suffices to leave the black_data color
     * data alone and set the alpha values using that simple formula. It shouldn't
     * matter what color channel we pick for the alpha computation, but we'll
     * pick green because if we went through a color channel downsample the green
     * bits are likely to be the most accurate.
     *
     * This function needs to be in the header file since it's used by both
     * gfxRecoverAlpha.cpp and gfxRecoverAlphaSSE2.cpp.
     */

    static inline PRUint32
    RecoverPixel(PRUint32 black, PRUint32 white)
    {
        const PRUint32 GREEN_MASK = 0x0000FF00;
        const PRUint32 ALPHA_MASK = 0xFF000000;

        /* |diff| here is larger when the source image pixel is more transparent.
           If both renderings are from the same source image composited with OVER,
           then the color values on white will always be greater than those on
           black, so |diff| would not overflow.  However, overflow may happen, for
           example, when a plugin plays a video and the image is rapidly changing.
           If there is overflow, then behave as if we limit to the difference to
           >= 0, which will make the rendering opaque.  (Without this overflow
           will make the rendering transparent.) */
        PRUint32 diff = (white & GREEN_MASK) - (black & GREEN_MASK);
        /* |diff| is 0xFFFFxx00 on overflow and 0x0000xx00 otherwise, so use this
            to limit the transparency. */
        PRUint32 limit = diff & ALPHA_MASK;
        /* The alpha bits of the result */
        PRUint32 alpha = (ALPHA_MASK - (diff << 16)) | limit;

        return alpha | (black & ~ALPHA_MASK);
    }
};

#endif /* _GFXALPHARECOVERY_H_ */
