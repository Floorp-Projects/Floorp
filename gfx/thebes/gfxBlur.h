/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_BLUR_H
#define GFX_BLUR_H

#include "gfxTypes.h"
#include "nsSize.h"
#include "gfxPoint.h"
#include "mozilla/RefPtr.h"
#include "mozilla/gfx/Blur.h"

class gfxContext;
struct gfxRect;

namespace mozilla {
  namespace gfx {
    struct Color;
    struct RectCornerRadii;
    class SourceSurface;
    class DrawTarget;
  } // namespace gfx
} // namespace mozilla

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
    typedef mozilla::gfx::Color Color;
    typedef mozilla::gfx::DrawTarget DrawTarget;
    typedef mozilla::gfx::RectCornerRadii RectCornerRadii;

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
    already_AddRefed<gfxContext>
    Init(const gfxRect& aRect,
         const mozilla::gfx::IntSize& aSpreadRadius,
         const mozilla::gfx::IntSize& aBlurRadius,
         const gfxRect* aDirtyRect,
         const gfxRect* aSkipRect);

    already_AddRefed<DrawTarget>
    InitDrawTarget(const mozilla::gfx::Rect& aRect,
                   const mozilla::gfx::IntSize& aSpreadRadius,
                   const mozilla::gfx::IntSize& aBlurRadius,
                   const mozilla::gfx::Rect* aDirtyRect = nullptr,
                   const mozilla::gfx::Rect* aSkipRect = nullptr);

    already_AddRefed<mozilla::gfx::SourceSurface>
    DoBlur(DrawTarget* aDT, mozilla::gfx::IntPoint* aTopLeft);

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
    static mozilla::gfx::IntSize CalculateBlurRadius(const gfxPoint& aStandardDeviation);

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
                              const RectCornerRadii* aCornerRadii,
                              const gfxPoint& aBlurStdDev,
                              const Color& aShadowColor,
                              const gfxRect& aDirtyRect,
                              const gfxRect& aSkipRect);

    static void ShutdownBlurCache();

    /***
     * Blurs an inset box shadow according to a given path.
     * This is equivalent to calling Init(), drawing the inset path,
     * and calling paint. Do not call Init() if using this method.
     *
     * @param aDestinationCtx     The destination to blur to.
     * @param aDestinationRect    The destination rect in device pixels
     * @param aShadowClipRect     The destiniation inner rect of the
     *                            inset path in device pixels.
     * @param aBlurRadius         The standard deviation of the blur.
     * @param aShadowColor        The color of the blur.
     * @param aInnerClipRadii     Corner radii for the inside rect if it is a rounded rect.
     * @param aSkipRect           An area in device pixels we don't have to paint in.
     */
    void BlurInsetBox(gfxContext* aDestinationCtx,
                      const mozilla::gfx::Rect& aDestinationRect,
                      const mozilla::gfx::Rect& aShadowClipRect,
                      const mozilla::gfx::IntSize& aBlurRadius,
                      const mozilla::gfx::Color& aShadowColor,
                      const RectCornerRadii* aInnerClipRadii,
                      const mozilla::gfx::Rect& aSkipRect,
                      const mozilla::gfx::Point& aShadowOffset);

protected:
    already_AddRefed<mozilla::gfx::SourceSurface>
    GetInsetBlur(const mozilla::gfx::Rect& aOuterRect,
                 const mozilla::gfx::Rect& aWhitespaceRect,
                 bool aIsDestRect,
                 const mozilla::gfx::Color& aShadowColor,
                 const mozilla::gfx::IntSize& aBlurRadius,
                 const RectCornerRadii* aInnerClipRadii,
                 DrawTarget* aDestDrawTarget,
                 bool aMirrorCorners);

    /**
     * The temporary alpha surface.
     */
    uint8_t* mData;

     /**
      * The object that actually does the blurring for us.
      */
    mozilla::gfx::AlphaBoxBlur mBlur;
};

#endif /* GFX_BLUR_H */
