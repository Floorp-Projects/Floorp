/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_BLUR_H
#define GFX_BLUR_H

#include "gfxTypes.h"
#include "nsSize.h"
#include "nsAutoPtr.h"
#include "gfxPoint.h"

class gfxContext;
class gfxImageSurface;
struct gfxRect;
struct gfxRGBA;
class gfxCornerSizes;
class gfxMatrix;

namespace mozilla {
  namespace gfx {
    class AlphaBoxBlur;
  }
}

/**
 * Implementation of a triple box blur approximation of a Gaussian blur.
 *
 * A Gaussian blur is good for blurring because, when done independently
 * in the horizontal and vertical directions, it matches the result that
 * would be obtained using a different (rotated) set of axes.  A triple
 * box blur is a very close approximation of a Gaussian.
 *
 * Creates an 8-bit alpha channel context for callers to draw in,
 * spreads the contents of that context, blurs the contents, and applies
 * it as an alpha mask on a different existing context.
 * 
 * A spread N makes each output pixel the maximum value of all source
 * pixels within a square of side length 2N+1 centered on the output pixel.
 * 
 * A temporary surface is created in the Init function. The caller then draws
 * any desired content onto the context acquired through GetContext, and lastly
 * calls Paint to apply the blurred content as an alpha mask.
 */
class gfxAlphaBoxBlur
{
public:
    gfxAlphaBoxBlur();

    ~gfxAlphaBoxBlur();

    /**
     * Constructs a box blur and initializes the temporary surface.
     * @param aRect The coordinates of the surface to create in device units.
     *
     * @param aBlurRadius The blur radius in pixels.  This is the radius of
     *   the entire (triple) kernel function.  Each individual box blur has
     *   radius approximately 1/3 this value, or diameter approximately 2/3
     *   this value.  This parameter should nearly always be computed using
     *   CalculateBlurRadius, below.
     *
     * @param aDirtyRect A pointer to a dirty rect, measured in device units,
     *  if available. This will be used for optimizing the blur operation. It
     *  is safe to pass nullptr here.
     *
     * @param aSkipRect A pointer to a rect, measured in device units, that
     *  represents an area where blurring is unnecessary and shouldn't be done
     *  for speed reasons. It is safe to pass nullptr here.
     */
    gfxContext* Init(const gfxRect& aRect,
                     const gfxIntSize& aSpreadRadius,
                     const gfxIntSize& aBlurRadius,
                     const gfxRect* aDirtyRect,
                     const gfxRect* aSkipRect);

    /**
     * Returns the context that should be drawn to supply the alpha mask to be
     * blurred. If the returned surface is null, then there was an error in
     * its creation.
     */
    gfxContext* GetContext()
    {
        return mContext;
    }

    /**
     * Does the actual blurring/spreading and mask applying. Users of this
     * object must have drawn whatever they want to be blurred onto the internal
     * gfxContext returned by GetContext before calling this.
     *
     * @param aDestinationCtx The graphics context on which to apply the
     *  blurred mask.
     */
    void Paint(gfxContext* aDestinationCtx);

    /**
     * Calculates a blur radius that, when used with box blur, approximates
     * a Gaussian blur with the given standard deviation.  The result of
     * this function should be used as the aBlurRadius parameter to Init,
     * above.
     */
    static gfxIntSize CalculateBlurRadius(const gfxPoint& aStandardDeviation);

    /**
     * Blurs a coloured rectangle onto aDestinationCtx. This is equivalent
     * to calling Init(), drawing a rectangle onto the returned surface
     * and then calling Paint, but may let us optimize better in the
     * backend.
     *
     * @param aDestinationCtx      The destination to blur to.
     * @param aRect                The rectangle to blur in device pixels.
     * @param aCornerRadii         Corner radii for aRect, if it is a rounded
     *                             rectangle.
     * @param aBlurRadius          The standard deviation of the blur.
     * @param aShadowColor         The color to draw the blurred shadow.
     * @param aDirtyRect           An area in device pixels that is dirty and needs
     *                             to be redrawn.
     * @param aSkipRect            An area in device pixels to avoid blurring over,
     *                             to prevent unnecessary work.
     */
    static void BlurRectangle(gfxContext *aDestinationCtx,
                              const gfxRect& aRect,
                              gfxCornerSizes* aCornerRadii,
                              const gfxPoint& aBlurStdDev,
                              const gfxRGBA& aShadowColor,
                              const gfxRect& aDirtyRect,
                              const gfxRect& aSkipRect);



protected:
    /**
     * The context of the temporary alpha surface.
     */
    nsRefPtr<gfxContext> mContext;

    /**
     * The temporary alpha surface.
     */
    nsRefPtr<gfxImageSurface> mImageSurface;

     /**
      * The object that actually does the blurring for us.
      */
    mozilla::gfx::AlphaBoxBlur *mBlur;
};

#endif /* GFX_BLUR_H */
