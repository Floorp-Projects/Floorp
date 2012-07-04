/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* utility functions for drawing borders and backgrounds */

#ifndef nsCSSRendering_h___
#define nsCSSRendering_h___

#include "nsStyleConsts.h"
#include "gfxBlur.h"
#include "gfxContext.h"
#include "gfxImageSurface.h"
#include "nsLayoutUtils.h"

struct nsPoint;
class nsStyleContext;
class nsPresContext;
class nsRenderingContext;

/**
 * This is a small wrapper class to encapsulate image drawing that can draw an
 * nsStyleImage image, which may internally be a real image, a sub image, or a
 * CSS gradient.
 *
 * @note Always call the member functions in the order of PrepareImage(),
 * ComputeSize(), and Draw().
 */
class nsImageRenderer {
public:
  typedef mozilla::layers::ImageContainer ImageContainer;

  enum {
    FLAG_SYNC_DECODE_IMAGES = 0x01
  };
  nsImageRenderer(nsIFrame* aForFrame, const nsStyleImage* aImage, PRUint32 aFlags);
  ~nsImageRenderer();
  /**
   * Populates member variables to get ready for rendering.
   * @return true iff the image is ready, and there is at least a pixel to
   * draw.
   */
  bool PrepareImage();
  /**
   * @return the image size in appunits when rendered, after accounting for the
   * background positioning area, background-size, and the image's intrinsic
   * dimensions (if any).
   */
  nsSize ComputeSize(const nsStyleBackground::Size& aLayerSize,
                     const nsSize& aBgPositioningArea);
  /**
   * Draws the image to the target rendering context.
   * @see nsLayoutUtils::DrawImage() for other parameters
   */
  void Draw(nsPresContext*       aPresContext,
            nsRenderingContext& aRenderingContext,
            const nsRect&        aDest,
            const nsRect&        aFill,
            const nsPoint&       aAnchor,
            const nsRect&        aDirty);


  bool IsRasterImage();
  already_AddRefed<ImageContainer> GetContainer();
private:
  /*
   * Compute the "unscaled" dimensions of the image in aUnscaled{Width,Height}
   * and aRatio.  Whether the image has a height and width are indicated by
   * aHaveWidth and aHaveHeight.  If the image doesn't have a ratio, aRatio will
   * be (0, 0).
   */
  void ComputeUnscaledDimensions(const nsSize& aBgPositioningArea,
                                 nscoord& aUnscaledWidth, bool& aHaveWidth,
                                 nscoord& aUnscaledHeight, bool& aHaveHeight,
                                 nsSize& aRatio);

  /*
   * Using the previously-computed unscaled width and height (if each are
   * valid, as indicated by aHaveWidth/aHaveHeight), compute the size at which
   * the image should actually render.
   */
  nsSize
  ComputeDrawnSize(const nsStyleBackground::Size& aLayerSize,
                   const nsSize& aBgPositioningArea,
                   nscoord aUnscaledWidth, bool aHaveWidth,
                   nscoord aUnscaledHeight, bool aHaveHeight,
                   const nsSize& aIntrinsicRatio);

  nsIFrame*                 mForFrame;
  const nsStyleImage*       mImage;
  nsStyleImageType          mType;
  nsCOMPtr<imgIContainer>   mImageContainer;
  nsRefPtr<nsStyleGradient> mGradientData;
  nsIFrame*                 mPaintServerFrame;
  nsLayoutUtils::SurfaceFromElementResult mImageElementSurface;
  bool                      mIsReady;
  nsSize                    mSize; // unscaled size of the image, in app units
  PRUint32                  mFlags;
};

/**
 * A struct representing all the information needed to paint a background
 * image to some target, taking into account all CSS background-* properties.
 * See PrepareBackgroundLayer.
 */
struct nsBackgroundLayerState {
  /**
   * @param aFlags some combination of nsCSSRendering::PAINTBG_* flags
   */
  nsBackgroundLayerState(nsIFrame* aForFrame, const nsStyleImage* aImage, PRUint32 aFlags)
    : mImageRenderer(aForFrame, aImage, aFlags) {}

  /**
   * The nsImageRenderer that will be used to draw the background.
   */
  nsImageRenderer mImageRenderer;
  /**
   * A rectangle that one copy of the image tile is mapped onto. Same
   * coordinate system as aBorderArea/aBGClipRect passed into
   * PrepareBackgroundLayer.
   */
  nsRect mDestArea;
  /**
   * The actual rectangle that should be filled with (complete or partial)
   * image tiles. Same coordinate system as aBorderArea/aBGClipRect passed into
   * PrepareBackgroundLayer.
   */
  nsRect mFillArea;
  /**
   * The anchor point that should be snapped to a pixel corner. Same
   * coordinate system as aBorderArea/aBGClipRect passed into
   * PrepareBackgroundLayer.
   */
  nsPoint mAnchor;
};

struct nsCSSRendering {
  /**
   * Initialize any static variables used by nsCSSRendering.
   */
  static void Init();
  
  /**
   * Clean up any static variables used by nsCSSRendering.
   */
  static void Shutdown();
  
  static void PaintBoxShadowInner(nsPresContext* aPresContext,
                                  nsRenderingContext& aRenderingContext,
                                  nsIFrame* aForFrame,
                                  const nsRect& aFrameArea,
                                  const nsRect& aDirtyRect);

  static void PaintBoxShadowOuter(nsPresContext* aPresContext,
                                  nsRenderingContext& aRenderingContext,
                                  nsIFrame* aForFrame,
                                  const nsRect& aFrameArea,
                                  const nsRect& aDirtyRect);

  static void ComputePixelRadii(const nscoord *aAppUnitsRadii,
                                nscoord aAppUnitsPerPixel,
                                gfxCornerSizes *oBorderRadii);

  /**
   * Render the border for an element using css rendering rules
   * for borders. aSkipSides is a bitmask of the sides to skip
   * when rendering. If 0 then no sides are skipped.
   */
  static void PaintBorder(nsPresContext* aPresContext,
                          nsRenderingContext& aRenderingContext,
                          nsIFrame* aForFrame,
                          const nsRect& aDirtyRect,
                          const nsRect& aBorderArea,
                          nsStyleContext* aStyleContext,
                          PRIntn aSkipSides = 0);

  /**
   * Like PaintBorder, but taking an nsStyleBorder argument instead of
   * getting it from aStyleContext.
   */
  static void PaintBorderWithStyleBorder(nsPresContext* aPresContext,
                                         nsRenderingContext& aRenderingContext,
                                         nsIFrame* aForFrame,
                                         const nsRect& aDirtyRect,
                                         const nsRect& aBorderArea,
                                         const nsStyleBorder& aBorderStyle,
                                         nsStyleContext* aStyleContext,
                                         PRIntn aSkipSides = 0);


  /**
   * Render the outline for an element using css rendering rules
   * for borders. aSkipSides is a bitmask of the sides to skip
   * when rendering. If 0 then no sides are skipped.
   */
  static void PaintOutline(nsPresContext* aPresContext,
                          nsRenderingContext& aRenderingContext,
                          nsIFrame* aForFrame,
                          const nsRect& aDirtyRect,
                          const nsRect& aBorderArea,
                          nsStyleContext* aStyleContext);

  /**
   * Render keyboard focus on an element.
   * |aFocusRect| is the outer rectangle of the focused element.
   * Uses a fixed style equivalent to "1px dotted |aColor|".
   * Not used for controls, because the native theme may differ.
   */
  static void PaintFocus(nsPresContext* aPresContext,
                         nsRenderingContext& aRenderingContext,
                         const nsRect& aFocusRect,
                         nscolor aColor);

  /**
   * Render a gradient for an element.
   */
  static void PaintGradient(nsPresContext* aPresContext,
                            nsRenderingContext& aRenderingContext,
                            nsStyleGradient* aGradient,
                            const nsRect& aDirtyRect,
                            const nsRect& aOneCellArea,
                            const nsRect& aFillArea);

  /**
   * Find the frame whose background style should be used to draw the
   * canvas background. aForFrame must be the frame for the root element
   * whose background style should be used. This function will return
   * aForFrame unless the <body> background should be propagated, in
   * which case we return the frame associated with the <body>'s background.
   */
  static nsIFrame* FindBackgroundStyleFrame(nsIFrame* aForFrame);

  /**
   * @return true if |aFrame| is a canvas frame, in the CSS sense.
   */
  static bool IsCanvasFrame(nsIFrame* aFrame);

  /**
   * Fill in an aBackgroundSC to be used to paint the background
   * for an element.  This applies the rules for propagating
   * backgrounds between BODY, the root element, and the canvas.
   * @return true if there is some meaningful background.
   */
  static bool FindBackground(nsPresContext* aPresContext,
                               nsIFrame* aForFrame,
                               nsStyleContext** aBackgroundSC);

  /**
   * As FindBackground, but the passed-in frame is known to be a root frame
   * (returned from nsCSSFrameConstructor::GetRootElementStyleFrame())
   * and there is always some meaningful background returned.
   */
  static nsStyleContext* FindRootFrameBackground(nsIFrame* aForFrame);

  /**
   * Returns background style information for the canvas.
   *
   * @param aForFrame
   *   the frame used to represent the canvas, in the CSS sense (i.e.
   *   nsCSSRendering::IsCanvasFrame(aForFrame) must be true)
   * @param aRootElementFrame
   *   the frame representing the root element of the document
   * @param aBackground
   *   contains background style information for the canvas on return
   */
  static nsStyleContext*
  FindCanvasBackground(nsIFrame* aForFrame, nsIFrame* aRootElementFrame)
  {
    NS_ABORT_IF_FALSE(IsCanvasFrame(aForFrame), "not a canvas frame");
    if (aRootElementFrame)
      return FindRootFrameBackground(aRootElementFrame);

    // This should always give transparent, so we'll fill it in with the
    // default color if needed.  This seems to happen a bit while a page is
    // being loaded.
    return aForFrame->GetStyleContext();
  }

  /**
   * Find a frame which draws a non-transparent background,
   * for various table-related and HR-related backwards-compatibility hacks.
   * This function will also stop if it finds themed frame which might draw
   * background.
   *
   * Be very hesitant if you're considering calling this function -- it's
   * usually not what you want.
   */
  static nsIFrame*
  FindNonTransparentBackgroundFrame(nsIFrame* aFrame,
                                    bool aStartAtParent = false);

  /**
   * Determine the background color to draw taking into account print settings.
   */
  static nscolor
  DetermineBackgroundColor(nsPresContext* aPresContext,
                           nsStyleContext* aStyleContext,
                           nsIFrame* aFrame,
                           bool& aDrawBackgroundImage,
                           bool& aDrawBackgroundColor);

  static nsBackgroundLayerState
  PrepareBackgroundLayer(nsPresContext* aPresContext,
                         nsIFrame* aForFrame,
                         PRUint32 aFlags,
                         const nsRect& aBorderArea,
                         const nsRect& aBGClipRect,
                         const nsStyleBackground& aBackground,
                         const nsStyleBackground::Layer& aLayer);

  /**
   * Render the background for an element using css rendering rules
   * for backgrounds.
   */
  enum {
    /**
     * When this flag is passed, the element's nsDisplayBorder will be
     * painted immediately on top of this background.
     */
    PAINTBG_WILL_PAINT_BORDER = 0x01,
    /**
     * When this flag is passed, images are synchronously decoded.
     */
    PAINTBG_SYNC_DECODE_IMAGES = 0x02,
    /**
     * When this flag is passed, painting will go to the screen so we can
     * take advantage of the fact that it will be clipped to the viewport.
     */
    PAINTBG_TO_WINDOW = 0x04
  };
  static void PaintBackground(nsPresContext* aPresContext,
                              nsRenderingContext& aRenderingContext,
                              nsIFrame* aForFrame,
                              const nsRect& aDirtyRect,
                              const nsRect& aBorderArea,
                              PRUint32 aFlags,
                              nsRect* aBGClipRect = nsnull);

  /**
   * Same as |PaintBackground|, except using the provided style structs.
   * This short-circuits the code that ensures that the root element's
   * background is drawn on the canvas.
   */
  static void PaintBackgroundWithSC(nsPresContext* aPresContext,
                                    nsRenderingContext& aRenderingContext,
                                    nsIFrame* aForFrame,
                                    const nsRect& aDirtyRect,
                                    const nsRect& aBorderArea,
                                    nsStyleContext *aStyleContext,
                                    const nsStyleBorder& aBorder,
                                    PRUint32 aFlags,
                                    nsRect* aBGClipRect = nsnull);

  /**
   * Returns the rectangle covered by the given background layer image, taking
   * into account background positioning, sizing, and repetition, but not
   * clipping.
   */
  static nsRect GetBackgroundLayerRect(nsPresContext* aPresContext,
                                       nsIFrame* aForFrame,
                                       const nsRect& aBorderArea,
                                       const nsStyleBackground& aBackground,
                                       const nsStyleBackground::Layer& aLayer);

  /**
   * Called by the presShell when painting is finished, so we can clear our
   * inline background data cache.
   */
  static void DidPaint();

  // Draw a border segment in the table collapsing border model without
  // beveling corners
  static void DrawTableBorderSegment(nsRenderingContext& aContext,
                                     PRUint8              aBorderStyle,  
                                     nscolor              aBorderColor,
                                     const nsStyleBackground* aBGColor,
                                     const nsRect&        aBorderRect,
                                     PRInt32              aAppUnitsPerCSSPixel,
                                     PRUint8              aStartBevelSide = 0,
                                     nscoord              aStartBevelOffset = 0,
                                     PRUint8              aEndBevelSide = 0,
                                     nscoord              aEndBevelOffset = 0);

  /**
   * Function for painting the decoration lines for the text.
   * NOTE: aPt, aLineSize, aAscent and aOffset are non-rounded device pixels,
   *       not app units.
   *   input:
   *     @param aFrame            the frame which needs the decoration line
   *     @param aGfxContext
   *     @param aDirtyRect        no need to paint outside this rect
   *     @param aColor            the color of the decoration line
   *     @param aPt               the top/left edge of the text
   *     @param aXInFrame         the distance between aPt.x and left edge of
   *                              aFrame.  If the decoration line is for shadow,
   *                              set the distance between the left edge of
   *                              the aFrame and the position of the text as
   *                              positioned without offset of the shadow.
   *     @param aLineSize         the width and the height of the decoration
   *                              line
   *     @param aAscent           the ascent of the text
   *     @param aOffset           the offset of the decoration line from
   *                              the baseline of the text (if the value is
   *                              positive, the line is lifted up)
   *     @param aDecoration       which line will be painted. The value can be
   *                              NS_STYLE_TEXT_DECORATION_LINE_UNDERLINE or
   *                              NS_STYLE_TEXT_DECORATION_LINE_OVERLINE or
   *                              NS_STYLE_TEXT_DECORATION_LINE_LINE_THROUGH.
   *     @param aStyle            the style of the decoration line such as
   *                              NS_STYLE_TEXT_DECORATION_STYLE_*.
   *     @param aDescentLimit     If aDescentLimit is zero or larger and the
   *                              underline overflows from the descent space,
   *                              the underline should be lifted up as far as
   *                              possible.  Note that this does not mean the
   *                              underline never overflows from this
   *                              limitation.  Because if the underline is
   *                              positioned to the baseline or upper, it causes
   *                              unreadability.  Note that if this is zero
   *                              or larger, the underline rect may be shrunken
   *                              if it's possible.  Therefore, this value is
   *                              used for strikeout line and overline too.
   */
  static void PaintDecorationLine(nsIFrame* aFrame,
                                  gfxContext* aGfxContext,
                                  const gfxRect& aDirtyRect,
                                  const nscolor aColor,
                                  const gfxPoint& aPt,
                                  const gfxFloat aXInFrame,
                                  const gfxSize& aLineSize,
                                  const gfxFloat aAscent,
                                  const gfxFloat aOffset,
                                  const PRUint8 aDecoration,
                                  const PRUint8 aStyle,
                                  const gfxFloat aDescentLimit = -1.0);

  /**
   * Function for getting the decoration line rect for the text.
   * NOTE: aLineSize, aAscent and aOffset are non-rounded device pixels,
   *       not app units.
   *   input:
   *     @param aPresContext
   *     @param aLineSize         the width and the height of the decoration
   *                              line
   *     @param aAscent           the ascent of the text
   *     @param aOffset           the offset of the decoration line from
   *                              the baseline of the text (if the value is
   *                              positive, the line is lifted up)
   *     @param aDecoration       which line will be painted. The value can be
   *                              NS_STYLE_TEXT_DECORATION_LINE_UNDERLINE or
   *                              NS_STYLE_TEXT_DECORATION_LINE_OVERLINE or
   *                              NS_STYLE_TEXT_DECORATION_LINE_LINE_THROUGH.
   *     @param aStyle            the style of the decoration line such as
   *                              NS_STYLE_TEXT_DECORATION_STYLE_*.
   *     @param aDescentLimit     If aDescentLimit is zero or larger and the
   *                              underline overflows from the descent space,
   *                              the underline should be lifted up as far as
   *                              possible.  Note that this does not mean the
   *                              underline never overflows from this
   *                              limitation.  Because if the underline is
   *                              positioned to the baseline or upper, it causes
   *                              unreadability.  Note that if this is zero
   *                              or larger, the underline rect may be shrunken
   *                              if it's possible.  Therefore, this value is
   *                              used for strikeout line and overline too.
   *   output:
   *     @return                  the decoration line rect for the input,
   *                              the each values are app units.
   */
  static nsRect GetTextDecorationRect(nsPresContext* aPresContext,
                                      const gfxSize& aLineSize,
                                      const gfxFloat aAscent,
                                      const gfxFloat aOffset,
                                      const PRUint8 aDecoration,
                                      const PRUint8 aStyle,
                                      const gfxFloat aDescentLimit = -1.0);

protected:
  static gfxRect GetTextDecorationRectInternal(const gfxPoint& aPt,
                                               const gfxSize& aLineSize,
                                               const gfxFloat aAscent,
                                               const gfxFloat aOffset,
                                               const PRUint8 aDecoration,
                                               const PRUint8 aStyle,
                                               const gfxFloat aDscentLimit);

  /**
   * Returns inflated rect for painting a decoration line.
   * Complex style decoration lines should be painted from leftmost of nearest
   * ancestor block box because that makes better look of connection of lines
   * for different nodes.  ExpandPaintingRectForDecorationLine() returns
   * a rect for actual painting rect for the clipped rect.
   *
   * input:
   *     @param aFrame            the frame which needs the decoration line.
   *     @param aStyle            the style of the complex decoration line
   *                              NS_STYLE_TEXT_DECORATION_STYLE_DOTTED or
   *                              NS_STYLE_TEXT_DECORATION_STYLE_DASHED or
   *                              NS_STYLE_TEXT_DECORATION_STYLE_WAVY.
   *     @param aClippedRect      the clipped rect for the decoration line.
   *                              in other words, visible area of the line.
   *     @param aXInFrame         the distance between left edge of aFrame and
   *                              aClippedRect.pos.x.
   *     @param aCycleLength      the width of one cycle of the line style.
   */
  static gfxRect ExpandPaintingRectForDecorationLine(
                   nsIFrame* aFrame,
                   const PRUint8 aStyle,
                   const gfxRect &aClippedRect,
                   const gfxFloat aXInFrame,
                   const gfxFloat aCycleLength);
};

/*
 * nsContextBoxBlur
 * Creates an 8-bit alpha channel context for callers to draw in, blurs the
 * contents of that context and applies it as a 1-color mask on a
 * different existing context. Uses gfxAlphaBoxBlur as its back end.
 *
 * You must call Init() first to create a suitable temporary surface to draw
 * on.  You must then draw any desired content onto the given context, then
 * call DoPaint() to apply the blurred content as a single-color mask. You
 * can only call Init() once, so objects cannot be reused.
 *
 * This is very useful for creating drop shadows or silhouettes.
 */
class nsContextBoxBlur {
public:
  enum {
    FORCE_MASK = 0x01
  };
  /**
   * Prepares a gfxContext to draw on. Do not call this twice; if you want
   * to get the gfxContext again use GetContext().
   *
   * @param aRect                The coordinates of the surface to create.
   *                             All coordinates must be in app units.
   *                             This must not include the blur radius, pass
   *                             it as the second parameter and everything
   *                             is taken care of.
   *
   * @param aBlurRadius          The blur radius in app units.
   *
   * @param aAppUnitsPerDevPixel The number of app units in a device pixel,
   *                             for conversion.  Most of the time you'll
   *                             pass this from the current PresContext if
   *                             available.
   *
   * @param aDestinationCtx      The graphics context to apply the blurred
   *                             mask to when you call DoPaint(). Make sure
   *                             it is not destroyed before you call
   *                             DoPaint(). To set the color of the
   *                             resulting blurred graphic mask, you must
   *                             set the color on this context before
   *                             calling Init().
   *
   * @param aDirtyRect           The absolute dirty rect in app units. Used to
   *                             optimize the temporary surface size and speed up blur.
   *
   * @param aSkipRect            An area in device pixels (NOT app units!) to avoid
   *                             blurring over, to prevent unnecessary work.
   *                             
   * @param aFlags               FORCE_MASK to ensure that the content drawn to the
   *                             returned gfxContext is used as a mask, and not
   *                             drawn directly to aDestinationCtx.
   *
   * @return            A blank 8-bit alpha-channel-only graphics context to
   *                    draw on, or null on error. Must not be freed. The
   *                    context has a device offset applied to it given by
   *                    aRect. This means you can use coordinates as if it
   *                    were at the desired position at aRect and you don't
   *                    need to worry about translating any coordinates to
   *                    draw on this temporary surface.
   *
   * If aBlurRadius is 0, the returned context is aDestinationCtx and
   * DoPaint() does nothing, because no blurring is required. Therefore, you
   * should prepare the destination context as if you were going to draw
   * directly on it instead of any temporary surface created in this class.
   */
  gfxContext* Init(const nsRect& aRect, nscoord aSpreadRadius,
                   nscoord aBlurRadius,
                   PRInt32 aAppUnitsPerDevPixel, gfxContext* aDestinationCtx,
                   const nsRect& aDirtyRect, const gfxRect* aSkipRect,
                   PRUint32 aFlags = 0);

  /**
   * Does the actual blurring/spreading. Users of this object *must*
   * have called Init() first, then have drawn whatever they want to be
   * blurred onto the internal gfxContext before calling this.
   */
  void DoEffects();
  
  /**
   * Does the actual blurring and mask applying. Users of this object *must*
   * have called Init() first, then have drawn whatever they want to be
   * blurred onto the internal gfxContext before calling this.
   */
  void DoPaint();

  /**
   * Gets the internal gfxContext at any time. Must not be freed. Avoid
   * calling this before calling Init() since the context would not be
   * constructed at that point.
   */
  gfxContext* GetContext();


  /**
   * Get the margin associated with the given blur radius, i.e., the
   * additional area that might be painted as a result of it.  (The
   * margin for a spread radius is itself, on all sides.)
   */
  static nsMargin GetBlurRadiusMargin(nscoord aBlurRadius,
                                      PRInt32 aAppUnitsPerDevPixel);

protected:
  gfxAlphaBoxBlur blur;
  nsRefPtr<gfxContext> mContext;
  gfxContext* mDestinationCtx;

  /* This is true if the blur already has it's content transformed
   * by mDestinationCtx's transform */
  bool mPreTransformed;

};

#endif /* nsCSSRendering_h___ */
