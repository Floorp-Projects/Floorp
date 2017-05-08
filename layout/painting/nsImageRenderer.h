/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsImageRenderer_h__
#define nsImageRenderer_h__

#include "nsLayoutUtils.h"
#include "nsStyleStruct.h"
#include "Units.h"

class gfxDrawable;
namespace mozilla {

namespace layers {
class StackingContextHelper;
class WebRenderParentCommand;
class WebRenderDisplayItemLayer;
} // namespace layers

namespace wr {
class DisplayListBuilder;
} // namespace wr

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
  typedef mozilla::image::DrawResult DrawResult;
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
   * Computes the placement for a background image, or for the image data
   * inside of a replaced element.
   *
   * @param aPos The CSS <position> value that specifies the image's position.
   * @param aOriginBounds The box to which the tiling position should be
   *          relative. For background images, this should correspond to
   *          'background-origin' for the frame, except when painting on the
   *          canvas, in which case the origin bounds should be the bounds
   *          of the root element's frame. For a replaced element, this should
   *          be the element's content-box.
   * @param aTopLeft [out] The top-left corner where an image tile should be
   *          drawn.
   * @param aAnchorPoint [out] A point which should be pixel-aligned by
   *          nsLayoutUtils::DrawImage. This is the same as aTopLeft, unless
   *          CSS specifies a percentage (including 'right' or 'bottom'), in
   *          which case it's that percentage within of aOriginBounds. So
   *          'right' would set aAnchorPoint.x to aOriginBounds.XMost().
   *
   * Points are returned relative to aOriginBounds.
   */
  static void ComputeObjectAnchorPoint(const mozilla::Position& aPos,
                                       const nsSize& aOriginBounds,
                                       const nsSize& aImageSize,
                                       nsPoint* aTopLeft,
                                       nsPoint* aAnchorPoint);

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
   * Draws the image to the target rendering context using
   * {background|mask}-specific arguments.
   * @see nsLayoutUtils::DrawImage() for parameters.
   */
  DrawResult DrawLayer(nsPresContext*       aPresContext,
                       nsRenderingContext&  aRenderingContext,
                       const nsRect&        aDest,
                       const nsRect&        aFill,
                       const nsPoint&       aAnchor,
                       const nsRect&        aDirty,
                       const nsSize&        aRepeatSize,
                       float                aOpacity);

  /**
   * Builds WebRender DisplayItems for an image using
   * {background|mask}-specific arguments.
   * @see nsLayoutUtils::DrawImage() for parameters.
   */
  DrawResult BuildWebRenderDisplayItemsForLayer(nsPresContext*       aPresContext,
                                                mozilla::wr::DisplayListBuilder& aBuilder,
                                                const mozilla::layers::StackingContextHelper& aSc,
                                                nsTArray<layers::WebRenderParentCommand>& aParentCommands,
                                                mozilla::layers::WebRenderDisplayItemLayer* aLayer,
                                                const nsRect&        aDest,
                                                const nsRect&        aFill,
                                                const nsPoint&       aAnchor,
                                                const nsRect&        aDirty,
                                                const nsSize&        aRepeatSize,
                                                float                aOpacity);

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
   * aSVGViewportSize The image size evaluated by default sizing algorithm.
   * Pass Nothing() if we can read a valid viewport size or aspect-ratio from
   * the drawing image directly, otherwise, pass Some() with viewport size
   * evaluated from default sizing algorithm.
   * aHasIntrinsicRatio is used to record if the source image has fixed
   * intrinsic ratio.
   */
  DrawResult
  DrawBorderImageComponent(nsPresContext*       aPresContext,
                           nsRenderingContext&  aRenderingContext,
                           const nsRect&        aDirtyRect,
                           const nsRect&        aFill,
                           const mozilla::CSSIntRect& aSrc,
                           uint8_t              aHFill,
                           uint8_t              aVFill,
                           const nsSize&        aUnitSize,
                           uint8_t              aIndex,
                           const mozilla::Maybe<nsSize>& aSVGViewportSize,
                           const bool           aHasIntrinsicRatio);

  bool IsRasterImage();
  bool IsAnimatedImage();

  /// Retrieves the image associated with this nsImageRenderer, if there is one.
  already_AddRefed<imgIContainer> GetImage();

  bool IsImageContainerAvailable(layers::LayerManager* aManager, uint32_t aFlags);
  bool IsReady() const { return mPrepareResult == DrawResult::SUCCESS; }
  DrawResult PrepareResult() const { return mPrepareResult; }
  void SetExtendMode(mozilla::gfx::ExtendMode aMode) { mExtendMode = aMode; }
  void SetMaskOp(uint8_t aMaskOp) { mMaskOp = aMaskOp; }
  void PurgeCacheForViewportChange(const mozilla::Maybe<nsSize>& aSVGViewportSize,
                                   const bool aHasRatio);
  nsStyleImageType GetType() const { return mType; }
  already_AddRefed<nsStyleGradient> GetGradientData();

private:
  /**
   * Draws the image to the target rendering context.
   * aSrc is a rect on the source image which will be mapped to aDest; it's
   * currently only used for gradients.
   *
   * @see nsLayoutUtils::DrawImage() for other parameters.
   */
  DrawResult Draw(nsPresContext*       aPresContext,
                  nsRenderingContext&  aRenderingContext,
                  const nsRect&        aDirtyRect,
                  const nsRect&        aDest,
                  const nsRect&        aFill,
                  const nsPoint&       aAnchor,
                  const nsSize&        aRepeatSize,
                  const mozilla::CSSIntRect& aSrc,
                  float                aOpacity = 1.0);

  /**
   * Builds WebRender DisplayItems for the image.
   * aSrc is a rect on the source image which will be mapped to aDest; it's
   * currently only used for gradients.
   *
   * @see nsLayoutUtils::DrawImage() for other parameters.
   */
  DrawResult BuildWebRenderDisplayItems(nsPresContext*       aPresContext,
                                        mozilla::wr::DisplayListBuilder& aBuilder,
                                        const mozilla::layers::StackingContextHelper& aSc,
                                        nsTArray<layers::WebRenderParentCommand>& aParentCommands,
                                        mozilla::layers::WebRenderDisplayItemLayer* aLayer,
                                        const nsRect&        aDirtyRect,
                                        const nsRect&        aDest,
                                        const nsRect&        aFill,
                                        const nsPoint&       aAnchor,
                                        const nsSize&        aRepeatSize,
                                        const mozilla::CSSIntRect& aSrc,
                                        float                aOpacity = 1.0);

  /**
   * Helper method for creating a gfxDrawable from mPaintServerFrame or
   * mImageElementSurface.
   * Requires mType is eStyleImageType_Element.
   * Returns null if we cannot create the drawable.
   */
  already_AddRefed<gfxDrawable> DrawableForElement(const nsRect& aImageRect,
                                                   gfxContext&  aContext);

  nsIFrame*                 mForFrame;
  const nsStyleImage*       mImage;
  nsStyleImageType          mType;
  nsCOMPtr<imgIContainer>   mImageContainer;
  RefPtr<nsStyleGradient> mGradientData;
  nsIFrame*                 mPaintServerFrame;
  nsLayoutUtils::SurfaceFromElementResult mImageElementSurface;
  DrawResult                mPrepareResult;
  nsSize                    mSize; // unscaled size of the image, in app units
  uint32_t                  mFlags;
  mozilla::gfx::ExtendMode  mExtendMode;
  uint8_t                   mMaskOp;
};

} // namespace mozilla

#endif /* nsImageRenderer_h__ */
