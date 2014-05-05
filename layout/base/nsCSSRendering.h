/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* utility functions for drawing borders and backgrounds */

#ifndef nsCSSRendering_h___
#define nsCSSRendering_h___

#include "gfxBlur.h"
#include "gfxContext.h"
#include "nsLayoutUtils.h"
#include "nsStyleStruct.h"
#include "nsIFrame.h"

class nsStyleContext;
class nsPresContext;
class nsRenderingContext;

namespace mozilla {

namespace layers {
class ImageContainer;
}

// A CSSSizeOrRatio represents a (possibly partially specified) size for use
// in computing image sizes. Either or both of the width and height might be
// given. A ratio of width to height may also be given. If we at least two
// of these then we can compute a concrete size, that is a width and height.
struct CSSSizeOrRatio
{
  CSSSizeOrRatio()
    : mRatio(0, 0)
    , mHasWidth(false)
    , mHasHeight(false) {}

  bool CanComputeConcreteSize() const
  {
    return mHasWidth + mHasHeight + HasRatio() >= 2;
  }
  bool IsConcrete() const { return mHasWidth && mHasHeight; }
  bool HasRatio() const { return mRatio.width > 0 && mRatio.height > 0; }
  bool IsEmpty() const
  {
    return (mHasWidth && mWidth <= 0) ||
           (mHasHeight && mHeight <= 0) ||
           mRatio.width <= 0 || mRatio.height <= 0; 
  }

  // CanComputeConcreteSize must return true when ComputeConcreteSize is
  // called.
  nsSize ComputeConcreteSize() const;

  void SetWidth(nscoord aWidth)
  {
    mWidth = aWidth;
    mHasWidth = true;
    if (mHasHeight) {
      mRatio = nsSize(mWidth, mHeight);
    }
  }
  void SetHeight(nscoord aHeight)
  {
    mHeight = aHeight;
    mHasHeight = true;
    if (mHasWidth) {
      mRatio = nsSize(mWidth, mHeight);
    }
  }
  void SetSize(const nsSize& aSize)
  {
    mWidth = aSize.width;
    mHeight = aSize.height;
    mHasWidth = true;
    mHasHeight = true;
    mRatio = aSize;    
  }
  void SetRatio(const nsSize& aRatio)
  {
    MOZ_ASSERT(!mHasWidth || !mHasHeight,
               "Probably shouldn't be setting a ratio if we have a concrete size");
    mRatio = aRatio;
  }

  nsSize mRatio;
  nscoord mWidth;
  nscoord mHeight;
  bool mHasWidth;
  bool mHasHeight;
};

}

/**
 * This is a small wrapper class to encapsulate image drawing that can draw an
 * nsStyleImage image, which may internally be a real image, a sub image, or a
 * CSS gradient.
 *
 * @note Always call the member functions in the order of PrepareImage(),
 * SetSize(), and Draw*().
 */
class nsImageRenderer {
public:
  typedef mozilla::layers::LayerManager LayerManager;
  typedef mozilla::layers::ImageContainer ImageContainer;

  enum {
    FLAG_SYNC_DECODE_IMAGES = 0x01,
    FLAG_PAINTING_TO_WINDOW = 0x02
  };
  enum FitType
  {
    CONTAIN,
    COVER
  };

  nsImageRenderer(nsIFrame* aForFrame, const nsStyleImage* aImage, uint32_t aFlags);
  ~nsImageRenderer();
  /**
   * Populates member variables to get ready for rendering.
   * @return true iff the image is ready, and there is at least a pixel to
   * draw.
   */
  bool PrepareImage();

  /**
   * The three Compute*Size functions correspond to the sizing algorthms and
   * definitions from the CSS Image Values and Replaced Content spec. See
   * http://dev.w3.org/csswg/css-images-3/#sizing .
   */
   
  /**
   * Compute the intrinsic size of the image as defined in the CSS Image Values
   * spec. The intrinsic size is the unscaled size which the image would ideally
   * like to be in app units.
   */
  mozilla::CSSSizeOrRatio ComputeIntrinsicSize();

  /**
   * Compute the size of the rendered image using either the 'cover' or
   * 'contain' constraints (aFitType).
   * aIntrinsicRatio may be an invalid ratio, that is one or both of its
   * dimensions can be less than or equal to zero.
   */
  static nsSize ComputeConstrainedSize(const nsSize& aConstrainingSize,
                                       const nsSize& aIntrinsicRatio,
                                       FitType aFitType);
  /**
   * Compute the size of the rendered image (the concrete size) where no cover/
   * contain constraints are given. The 'default algorithm' from the CSS Image
   * Values spec.
   */
  static nsSize ComputeConcreteSize(const mozilla::CSSSizeOrRatio& aSpecifiedSize,
                                    const mozilla::CSSSizeOrRatio& aIntrinsicSize,
                                    const nsSize& aDefaultSize);

  /**
   * Set this image's preferred size. This will be its intrinsic size where
   * specified and the default size where it is not. Used as the unscaled size
   * when rendering the image.
   */
  void SetPreferredSize(const mozilla::CSSSizeOrRatio& aIntrinsicSize,
                        const nsSize& aDefaultSize);

  /**
   * Draws the image to the target rendering context.
   * aSrc is a rect on the source image which will be mapped to aDest.
   * @see nsLayoutUtils::DrawImage() for other parameters.
   */
  void Draw(nsPresContext*       aPresContext,
            nsRenderingContext&  aRenderingContext,
            const nsRect&        aDirtyRect,
            const nsRect&        aFill,
            const nsRect&        aDest,
            const mozilla::CSSIntRect& aSrc);
  /**
   * Draws the image to the target rendering context using background-specific
   * arguments.
   * @see nsLayoutUtils::DrawImage() for parameters.
   */
  void DrawBackground(nsPresContext*       aPresContext,
                      nsRenderingContext&  aRenderingContext,
                      const nsRect&        aDest,
                      const nsRect&        aFill,
                      const nsPoint&       aAnchor,
                      const nsRect&        aDirty);

  /**
   * Draw the image to a single component of a border-image style rendering.
   * aFill The destination rect to be drawn into
   * aSrc is the part of the image to be rendered into a tile (aUnitSize in
   * aFill), if aSrc and the dest tile are different sizes, the image will be
   * scaled to map aSrc onto the dest tile.
   * aHFill and aVFill are the repeat patterns for the component -
   * NS_STYLE_BORDER_IMAGE_REPEAT_*
   * aUnitSize The scaled size of a single source rect (in destination coords)
   * aIndex identifies the component: 0 1 2
   *                                  3 4 5
   *                                  6 7 8
   */
  void
  DrawBorderImageComponent(nsPresContext*       aPresContext,
                           nsRenderingContext&  aRenderingContext,
                           const nsRect&        aDirtyRect,
                           const nsRect&        aFill,
                           const mozilla::CSSIntRect& aSrc,
                           uint8_t              aHFill,
                           uint8_t              aVFill,
                           const nsSize&        aUnitSize,
                           uint8_t              aIndex);

  bool IsRasterImage();
  bool IsAnimatedImage();
  already_AddRefed<ImageContainer> GetContainer(LayerManager* aManager);

  bool IsReady() { return mIsReady; }

private:
  /**
   * Helper method for creating a gfxDrawable from mPaintServerFrame or 
   * mImageElementSurface.
   * Requires mType is eStyleImageType_Element.
   * Returns null if we cannot create the drawable.
   */
  already_AddRefed<gfxDrawable> DrawableForElement(const nsRect& aImageRect,
                                                   nsRenderingContext&  aRenderingContext);

  nsIFrame*                 mForFrame;
  const nsStyleImage*       mImage;
  nsStyleImageType          mType;
  nsCOMPtr<imgIContainer>   mImageContainer;
  nsRefPtr<nsStyleGradient> mGradientData;
  nsIFrame*                 mPaintServerFrame;
  nsLayoutUtils::SurfaceFromElementResult mImageElementSurface;
  bool                      mIsReady;
  nsSize                    mSize; // unscaled size of the image, in app units
  uint32_t                  mFlags;
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
  nsBackgroundLayerState(nsIFrame* aForFrame, const nsStyleImage* aImage, uint32_t aFlags)
    : mImageRenderer(aForFrame, aImage, aFlags), mCompositingOp(gfxContext::OPERATOR_OVER) {}

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
  /**
   * The compositing operation that the image should use
   */
  gfxContext::GraphicsOperator mCompositingOp;
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
                                  const nsRect& aDirtyRect,
                                  float aOpacity = 1.0);

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
                          int aSkipSides = 0);

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
                                         int aSkipSides = 0);


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
   * aDest is the rect for a single tile of the gradient on the destination.
   * aFill is the rect on the destination to be covered by repeated tiling of
   * the gradient.
   * aSrc is the part of the gradient to be rendered into a tile (aDest), if
   * aSrc and aDest are different sizes, the image will be scaled to map aSrc
   * onto aDest.
   * aIntrinsicSize is the size of the source gradient.
   */
  static void PaintGradient(nsPresContext* aPresContext,
                            nsRenderingContext& aRenderingContext,
                            nsStyleGradient* aGradient,
                            const nsRect& aDirtyRect,
                            const nsRect& aDest,
                            const nsRect& aFill,
                            const mozilla::CSSIntRect& aSrc,
                            const nsSize& aIntrinsiceSize);

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
  static bool FindBackground(nsIFrame* aForFrame,
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
    return aForFrame->StyleContext();
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

  static nsRect
  ComputeBackgroundPositioningArea(nsPresContext* aPresContext,
                                   nsIFrame* aForFrame,
                                   const nsRect& aBorderArea,
                                   const nsStyleBackground::Layer& aLayer,
                                   nsIFrame** aAttachedToFrame);

  static nsBackgroundLayerState
  PrepareBackgroundLayer(nsPresContext* aPresContext,
                         nsIFrame* aForFrame,
                         uint32_t aFlags,
                         const nsRect& aBorderArea,
                         const nsRect& aBGClipRect,
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
                              uint32_t aFlags,
                              nsRect* aBGClipRect = nullptr,
                              int32_t aLayer = -1);
 
  static void PaintBackgroundColor(nsPresContext* aPresContext,
                                   nsRenderingContext& aRenderingContext,
                                   nsIFrame* aForFrame,
                                   const nsRect& aDirtyRect,
                                   const nsRect& aBorderArea,
                                   uint32_t aFlags);

  /**
   * Same as |PaintBackground|, except using the provided style structs.
   * This short-circuits the code that ensures that the root element's
   * background is drawn on the canvas.
   * The aLayer parameter allows you to paint a single layer of the background.
   * The default value for aLayer, -1, means that all layers will be painted.
   * The background color will only be painted if the back-most layer is also
   * being painted.
   */
  static void PaintBackgroundWithSC(nsPresContext* aPresContext,
                                    nsRenderingContext& aRenderingContext,
                                    nsIFrame* aForFrame,
                                    const nsRect& aDirtyRect,
                                    const nsRect& aBorderArea,
                                    nsStyleContext *aStyleContext,
                                    const nsStyleBorder& aBorder,
                                    uint32_t aFlags,
                                    nsRect* aBGClipRect = nullptr,
                                    int32_t aLayer = -1);

  static void PaintBackgroundColorWithSC(nsPresContext* aPresContext,
                                         nsRenderingContext& aRenderingContext,
                                         nsIFrame* aForFrame,
                                         const nsRect& aDirtyRect,
                                         const nsRect& aBorderArea,
                                         nsStyleContext *aStyleContext,
                                         const nsStyleBorder& aBorder,
                                         uint32_t aFlags);
  /**
   * Returns the rectangle covered by the given background layer image, taking
   * into account background positioning, sizing, and repetition, but not
   * clipping.
   */
  static nsRect GetBackgroundLayerRect(nsPresContext* aPresContext,
                                       nsIFrame* aForFrame,
                                       const nsRect& aBorderArea,
                                       const nsRect& aClipRect,
                                       const nsStyleBackground::Layer& aLayer,
                                       uint32_t aFlags);

  /**
   * Checks if image in layer aLayer of aBackground is currently decoded.
   */
  static bool IsBackgroundImageDecodedForStyleContextAndLayer(
    const nsStyleBackground *aBackground, uint32_t aLayer);

  /**
   * Checks if all images that are part of the background for aFrame are
   * currently decoded.
   */
  static bool AreAllBackgroundImagesDecodedForFrame(nsIFrame* aFrame);

  /**
   * Called when we start creating a display list. The frame tree will not
   * change until a matching EndFrameTreeLocked is called.
   */
  static void BeginFrameTreesLocked();
  /**
   * Called when we've finished using a display list. When all
   * BeginFrameTreeLocked calls have been balanced by an EndFrameTreeLocked,
   * the frame tree may start changing again.
   */
  static void EndFrameTreesLocked();

  // Draw a border segment in the table collapsing border model without
  // beveling corners
  static void DrawTableBorderSegment(nsRenderingContext& aContext,
                                     uint8_t              aBorderStyle,  
                                     nscolor              aBorderColor,
                                     const nsStyleBackground* aBGColor,
                                     const nsRect&        aBorderRect,
                                     int32_t              aAppUnitsPerCSSPixel,
                                     uint8_t              aStartBevelSide = 0,
                                     nscoord              aStartBevelOffset = 0,
                                     uint8_t              aEndBevelSide = 0,
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
                                  const uint8_t aDecoration,
                                  const uint8_t aStyle,
                                  const gfxFloat aDescentLimit = -1.0);

  /**
   * Adds a path corresponding to the outline of the decoration line to
   * the specified context.  Arguments have the same meaning as for
   * PaintDecorationLine.  Currently this only works for solid
   * decorations; for other decoration styles, an empty path is added
   * to the context.
   */
  static void DecorationLineToPath(nsIFrame* aFrame,
                                   gfxContext* aGfxContext,
                                   const gfxRect& aDirtyRect,
                                   const nscolor aColor,
                                   const gfxPoint& aPt,
                                   const gfxFloat aXInFrame,
                                   const gfxSize& aLineSize,
                                   const gfxFloat aAscent,
                                   const gfxFloat aOffset,
                                   const uint8_t aDecoration,
                                   const uint8_t aStyle,
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
                                      const uint8_t aDecoration,
                                      const uint8_t aStyle,
                                      const gfxFloat aDescentLimit = -1.0);

  static gfxContext::GraphicsOperator GetGFXBlendMode(uint8_t mBlendMode) {
    switch (mBlendMode) {
      case NS_STYLE_BLEND_NORMAL:      return gfxContext::OPERATOR_OVER;
      case NS_STYLE_BLEND_MULTIPLY:    return gfxContext::OPERATOR_MULTIPLY;
      case NS_STYLE_BLEND_SCREEN:      return gfxContext::OPERATOR_SCREEN;
      case NS_STYLE_BLEND_OVERLAY:     return gfxContext::OPERATOR_OVERLAY;
      case NS_STYLE_BLEND_DARKEN:      return gfxContext::OPERATOR_DARKEN;
      case NS_STYLE_BLEND_LIGHTEN:     return gfxContext::OPERATOR_LIGHTEN;
      case NS_STYLE_BLEND_COLOR_DODGE: return gfxContext::OPERATOR_COLOR_DODGE;
      case NS_STYLE_BLEND_COLOR_BURN:  return gfxContext::OPERATOR_COLOR_BURN;
      case NS_STYLE_BLEND_HARD_LIGHT:  return gfxContext::OPERATOR_HARD_LIGHT;
      case NS_STYLE_BLEND_SOFT_LIGHT:  return gfxContext::OPERATOR_SOFT_LIGHT;
      case NS_STYLE_BLEND_DIFFERENCE:  return gfxContext::OPERATOR_DIFFERENCE;
      case NS_STYLE_BLEND_EXCLUSION:   return gfxContext::OPERATOR_EXCLUSION;
      case NS_STYLE_BLEND_HUE:         return gfxContext::OPERATOR_HUE;
      case NS_STYLE_BLEND_SATURATION:  return gfxContext::OPERATOR_SATURATION;
      case NS_STYLE_BLEND_COLOR:       return gfxContext::OPERATOR_COLOR;
      case NS_STYLE_BLEND_LUMINOSITY:  return gfxContext::OPERATOR_LUMINOSITY;
      default:                         MOZ_ASSERT(false); return gfxContext::OPERATOR_OVER;
    }

    return gfxContext::OPERATOR_OVER;
  }

protected:
  static gfxRect GetTextDecorationRectInternal(const gfxPoint& aPt,
                                               const gfxSize& aLineSize,
                                               const gfxFloat aAscent,
                                               const gfxFloat aOffset,
                                               const uint8_t aDecoration,
                                               const uint8_t aStyle,
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
                   const uint8_t aStyle,
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
                   int32_t aAppUnitsPerDevPixel, gfxContext* aDestinationCtx,
                   const nsRect& aDirtyRect, const gfxRect* aSkipRect,
                   uint32_t aFlags = 0);

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
                                      int32_t aAppUnitsPerDevPixel);

  /**
   * Blurs a coloured rectangle onto aDestinationCtx. This is equivalent
   * to calling Init(), drawing a rectangle onto the returned surface
   * and then calling DoPaint, but may let us optimize better in the
   * backend.
   *
   * @param aDestinationCtx      The destination to blur to.
   * @param aRect                The rectangle to blur in app units.
   * @param aAppUnitsPerDevPixel The number of app units in a device pixel,
   *                             for conversion.  Most of the time you'll
   *                             pass this from the current PresContext if
   *                             available.
   * @param aCornerRadii         Corner radii for aRect, if it is a rounded
   *                             rectangle.
   * @param aBlurRadius          The blur radius in app units.
   * @param aShadowColor         The color to draw the blurred shadow.
   * @param aDirtyRect           The absolute dirty rect in app units. Used to
   *                             optimize the temporary surface size and speed up blur.
   * @param aSkipRect            An area in device pixels (NOT app units!) to avoid
   *                             blurring over, to prevent unnecessary work.
   */
  static void BlurRectangle(gfxContext* aDestinationCtx,
                            const nsRect& aRect,
                            int32_t aAppUnitsPerDevPixel,
                            gfxCornerSizes* aCornerRadii,
                            nscoord aBlurRadius,
                            const gfxRGBA& aShadowColor,
                            const nsRect& aDirtyRect,
                            const gfxRect& aSkipRect);

protected:
  gfxAlphaBoxBlur blur;
  nsRefPtr<gfxContext> mContext;
  gfxContext* mDestinationCtx;

  /* This is true if the blur already has it's content transformed
   * by mDestinationCtx's transform */
  bool mPreTransformed;

};

#endif /* nsCSSRendering_h___ */
