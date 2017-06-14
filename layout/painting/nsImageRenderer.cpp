/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:ts=2:et:sw=2:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* utility functions for drawing borders and backgrounds */

#include "nsImageRenderer.h"

#include "mozilla/webrender/WebRenderAPI.h"

#include "gfxContext.h"
#include "gfxDrawable.h"
#include "ImageOps.h"
#include "mozilla/layers/StackingContextHelper.h"
#include "nsContentUtils.h"
#include "nsCSSRendering.h"
#include "nsCSSRenderingGradients.h"
#include "nsIFrame.h"
#include "nsStyleStructInlines.h"
#include "nsSVGEffects.h"
#include "nsSVGIntegrationUtils.h"

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::image;
using namespace mozilla::layers;

nsSize
CSSSizeOrRatio::ComputeConcreteSize() const
{
  NS_ASSERTION(CanComputeConcreteSize(), "Cannot compute");
  if (mHasWidth && mHasHeight) {
    return nsSize(mWidth, mHeight);
  }
  if (mHasWidth) {
    nscoord height = NSCoordSaturatingNonnegativeMultiply(
      mWidth,
      double(mRatio.height) / mRatio.width);
    return nsSize(mWidth, height);
  }

  MOZ_ASSERT(mHasHeight);
  nscoord width = NSCoordSaturatingNonnegativeMultiply(
    mHeight,
    double(mRatio.width) / mRatio.height);
  return nsSize(width, mHeight);
}

nsImageRenderer::nsImageRenderer(nsIFrame* aForFrame,
                                 const nsStyleImage* aImage,
                                 uint32_t aFlags)
  : mForFrame(aForFrame)
  , mImage(aImage)
  , mType(aImage->GetType())
  , mImageContainer(nullptr)
  , mGradientData(nullptr)
  , mPaintServerFrame(nullptr)
  , mPrepareResult(DrawResult::NOT_READY)
  , mSize(0, 0)
  , mFlags(aFlags)
  , mExtendMode(ExtendMode::CLAMP)
  , mMaskOp(NS_STYLE_MASK_MODE_MATCH_SOURCE)
{
}

nsImageRenderer::~nsImageRenderer()
{
}

static bool
ShouldTreatAsCompleteDueToSyncDecode(const nsStyleImage* aImage,
                                     uint32_t aFlags)
{
  if (!(aFlags & nsImageRenderer::FLAG_SYNC_DECODE_IMAGES)) {
    return false;
  }

  if (aImage->GetType() != eStyleImageType_Image) {
    return false;
  }

  imgRequestProxy* req = aImage->GetImageData();
  if (!req) {
    return false;
  }

  uint32_t status = 0;
  if (NS_FAILED(req->GetImageStatus(&status))) {
    return false;
  }

  if (status & imgIRequest::STATUS_ERROR) {
    // The image is "complete" since it's a corrupt image. If we created an
    // imgIContainer at all, return true.
    nsCOMPtr<imgIContainer> image;
    req->GetImage(getter_AddRefs(image));
    return bool(image);
  }

  if (!(status & imgIRequest::STATUS_LOAD_COMPLETE)) {
    // We must have loaded all of the image's data and the size must be
    // available, or else sync decoding won't be able to decode the image.
    return false;
  }

  return true;
}

bool
nsImageRenderer::PrepareImage()
{
  if (mImage->IsEmpty()) {
    mPrepareResult = DrawResult::BAD_IMAGE;
    return false;
  }

  if (!mImage->IsComplete()) {
    // Make sure the image is actually decoding.
    bool frameComplete = mImage->StartDecoding();

    // Check again to see if we finished.
    // We cannot prepare the image for rendering if it is not fully loaded.
    // Special case: If we requested a sync decode and the image has loaded, push
    // on through because the Draw() will do a sync decode then.
    if (!(frameComplete || mImage->IsComplete()) &&
        !ShouldTreatAsCompleteDueToSyncDecode(mImage, mFlags)) {
      mPrepareResult = DrawResult::NOT_READY;
      return false;
    }
  }

  switch (mType) {
    case eStyleImageType_Image: {
      MOZ_ASSERT(mImage->GetImageData(),
                 "must have image data, since we checked IsEmpty above");
      nsCOMPtr<imgIContainer> srcImage;
      DebugOnly<nsresult> rv =
        mImage->GetImageData()->GetImage(getter_AddRefs(srcImage));
      MOZ_ASSERT(NS_SUCCEEDED(rv) && srcImage,
                 "If GetImage() is failing, mImage->IsComplete() "
                 "should have returned false");

      if (!mImage->GetCropRect()) {
        mImageContainer.swap(srcImage);
      } else {
        nsIntRect actualCropRect;
        bool isEntireImage;
        bool success =
          mImage->ComputeActualCropRect(actualCropRect, &isEntireImage);
        if (!success || actualCropRect.IsEmpty()) {
          // The cropped image has zero size
          mPrepareResult = DrawResult::BAD_IMAGE;
          return false;
        }
        if (isEntireImage) {
          // The cropped image is identical to the source image
          mImageContainer.swap(srcImage);
        } else {
          nsCOMPtr<imgIContainer> subImage = ImageOps::Clip(srcImage,
                                                            actualCropRect,
                                                            Nothing());
          mImageContainer.swap(subImage);
        }
      }
      mPrepareResult = DrawResult::SUCCESS;
      break;
    }
    case eStyleImageType_Gradient:
      mGradientData = mImage->GetGradientData();
      mPrepareResult = DrawResult::SUCCESS;
      break;
    case eStyleImageType_Element:
    {
      nsAutoString elementId =
        NS_LITERAL_STRING("#") + nsDependentAtomString(mImage->GetElementId());
      nsCOMPtr<nsIURI> targetURI;
      nsCOMPtr<nsIURI> base = mForFrame->GetContent()->GetBaseURI();
      nsContentUtils::NewURIWithDocumentCharset(getter_AddRefs(targetURI), elementId,
                                                mForFrame->GetContent()->GetUncomposedDoc(), base);
      nsSVGPaintingProperty* property = nsSVGEffects::GetPaintingPropertyForURI(
          targetURI, mForFrame->FirstContinuation(),
          nsSVGEffects::BackgroundImageProperty());
      if (!property) {
        mPrepareResult = DrawResult::BAD_IMAGE;
        return false;
      }

      // If the referenced element is an <img>, <canvas>, or <video> element,
      // prefer SurfaceFromElement as it's more reliable.
      mImageElementSurface =
        nsLayoutUtils::SurfaceFromElement(property->GetReferencedElement());
      if (!mImageElementSurface.GetSourceSurface()) {
        mPaintServerFrame = property->GetReferencedFrame();
        if (!mPaintServerFrame) {
          mPrepareResult = DrawResult::BAD_IMAGE;
          return false;
        }
      }

      mPrepareResult = DrawResult::SUCCESS;
      break;
    }
    case eStyleImageType_Null:
    default:
      break;
  }

  return IsReady();
}

CSSSizeOrRatio
nsImageRenderer::ComputeIntrinsicSize()
{
  NS_ASSERTION(IsReady(), "Ensure PrepareImage() has returned true "
                          "before calling me");

  CSSSizeOrRatio result;
  switch (mType) {
    case eStyleImageType_Image:
    {
      bool haveWidth, haveHeight;
      CSSIntSize imageIntSize;
      nsLayoutUtils::ComputeSizeForDrawing(mImageContainer, imageIntSize,
                                           result.mRatio, haveWidth, haveHeight);
      if (haveWidth) {
        result.SetWidth(nsPresContext::CSSPixelsToAppUnits(imageIntSize.width));
      }
      if (haveHeight) {
        result.SetHeight(nsPresContext::CSSPixelsToAppUnits(imageIntSize.height));
      }

      // If we know the aspect ratio and one of the dimensions,
      // we can compute the other missing width or height.
      if (!haveHeight && haveWidth && result.mRatio.width != 0) {
        nscoord intrinsicHeight =
          NSCoordSaturatingNonnegativeMultiply(imageIntSize.width,
                                               float(result.mRatio.height) /
                                               float(result.mRatio.width));
        result.SetHeight(nsPresContext::CSSPixelsToAppUnits(intrinsicHeight));
      } else if (haveHeight && !haveWidth && result.mRatio.height != 0) {
        nscoord intrinsicWidth =
          NSCoordSaturatingNonnegativeMultiply(imageIntSize.height,
                                               float(result.mRatio.width) /
                                               float(result.mRatio.height));
        result.SetWidth(nsPresContext::CSSPixelsToAppUnits(intrinsicWidth));
      }

      break;
    }
    case eStyleImageType_Element:
    {
      // XXX element() should have the width/height of the referenced element,
      //     and that element's ratio, if it matches.  If it doesn't match, it
      //     should have no width/height or ratio.  See element() in CSS images:
      //     <http://dev.w3.org/csswg/css-images-4/#element-notation>.
      //     Make sure to change nsStyleImageLayers::Size::DependsOnFrameSize
      //     when fixing this!
      if (mPaintServerFrame) {
        // SVG images have no intrinsic size
        if (!mPaintServerFrame->IsFrameOfType(nsIFrame::eSVG)) {
          // The intrinsic image size for a generic nsIFrame paint server is
          // the union of the border-box rects of all of its continuations,
          // rounded to device pixels.
          int32_t appUnitsPerDevPixel =
            mForFrame->PresContext()->AppUnitsPerDevPixel();
          result.SetSize(
            IntSizeToAppUnits(
              nsSVGIntegrationUtils::GetContinuationUnionSize(mPaintServerFrame).
                ToNearestPixels(appUnitsPerDevPixel),
              appUnitsPerDevPixel));
        }
      } else {
        NS_ASSERTION(mImageElementSurface.GetSourceSurface(),
                     "Surface should be ready.");
        IntSize surfaceSize = mImageElementSurface.mSize;
        result.SetSize(
          nsSize(nsPresContext::CSSPixelsToAppUnits(surfaceSize.width),
                 nsPresContext::CSSPixelsToAppUnits(surfaceSize.height)));
      }
      break;
    }
    case eStyleImageType_Gradient:
      // Per <http://dev.w3.org/csswg/css3-images/#gradients>, gradients have no
      // intrinsic dimensions.
    case eStyleImageType_Null:
    default:
      break;
  }

  return result;
}

/* static */ nsSize
nsImageRenderer::ComputeConcreteSize(const CSSSizeOrRatio& aSpecifiedSize,
                                     const CSSSizeOrRatio& aIntrinsicSize,
                                     const nsSize& aDefaultSize)
{
  // The specified size is fully specified, just use that
  if (aSpecifiedSize.IsConcrete()) {
    return aSpecifiedSize.ComputeConcreteSize();
  }

  MOZ_ASSERT(!aSpecifiedSize.mHasWidth || !aSpecifiedSize.mHasHeight);

  if (!aSpecifiedSize.mHasWidth && !aSpecifiedSize.mHasHeight) {
    // no specified size, try using the intrinsic size
    if (aIntrinsicSize.CanComputeConcreteSize()) {
      return aIntrinsicSize.ComputeConcreteSize();
    }

    if (aIntrinsicSize.mHasWidth) {
      return nsSize(aIntrinsicSize.mWidth, aDefaultSize.height);
    }
    if (aIntrinsicSize.mHasHeight) {
      return nsSize(aDefaultSize.width, aIntrinsicSize.mHeight);
    }

    // couldn't use the intrinsic size either, revert to using the default size
    return ComputeConstrainedSize(aDefaultSize,
                                  aIntrinsicSize.mRatio,
                                  CONTAIN);
  }

  MOZ_ASSERT(aSpecifiedSize.mHasWidth || aSpecifiedSize.mHasHeight);

  // The specified height is partial, try to compute the missing part.
  if (aSpecifiedSize.mHasWidth) {
    nscoord height;
    if (aIntrinsicSize.HasRatio()) {
      height = NSCoordSaturatingNonnegativeMultiply(
        aSpecifiedSize.mWidth,
        double(aIntrinsicSize.mRatio.height) / aIntrinsicSize.mRatio.width);
    } else if (aIntrinsicSize.mHasHeight) {
      height = aIntrinsicSize.mHeight;
    } else {
      height = aDefaultSize.height;
    }
    return nsSize(aSpecifiedSize.mWidth, height);
  }

  MOZ_ASSERT(aSpecifiedSize.mHasHeight);
  nscoord width;
  if (aIntrinsicSize.HasRatio()) {
    width = NSCoordSaturatingNonnegativeMultiply(
      aSpecifiedSize.mHeight,
      double(aIntrinsicSize.mRatio.width) / aIntrinsicSize.mRatio.height);
  } else if (aIntrinsicSize.mHasWidth) {
    width = aIntrinsicSize.mWidth;
  } else {
    width = aDefaultSize.width;
  }
  return nsSize(width, aSpecifiedSize.mHeight);
}

/* static */ nsSize
nsImageRenderer::ComputeConstrainedSize(const nsSize& aConstrainingSize,
                                        const nsSize& aIntrinsicRatio,
                                        FitType aFitType)
{
  if (aIntrinsicRatio.width <= 0 && aIntrinsicRatio.height <= 0) {
    return aConstrainingSize;
  }

  float scaleX = double(aConstrainingSize.width) / aIntrinsicRatio.width;
  float scaleY = double(aConstrainingSize.height) / aIntrinsicRatio.height;
  nsSize size;
  if ((aFitType == CONTAIN) == (scaleX < scaleY)) {
    size.width = aConstrainingSize.width;
    size.height = NSCoordSaturatingNonnegativeMultiply(
                    aIntrinsicRatio.height, scaleX);
    // If we're reducing the size by less than one css pixel, then just use the
    // constraining size.
    if (aFitType == CONTAIN && aConstrainingSize.height - size.height < nsPresContext::AppUnitsPerCSSPixel()) {
      size.height = aConstrainingSize.height;
    }
  } else {
    size.width = NSCoordSaturatingNonnegativeMultiply(
                   aIntrinsicRatio.width, scaleY);
    if (aFitType == CONTAIN && aConstrainingSize.width - size.width < nsPresContext::AppUnitsPerCSSPixel()) {
      size.width = aConstrainingSize.width;
    }
    size.height = aConstrainingSize.height;
  }
  return size;
}

/**
 * mSize is the image's "preferred" size for this particular rendering, while
 * the drawn (aka concrete) size is the actual rendered size after accounting
 * for background-size etc..  The preferred size is most often the image's
 * intrinsic dimensions.  But for images with incomplete intrinsic dimensions,
 * the preferred size varies, depending on the specified and default sizes, see
 * nsImageRenderer::Compute*Size.
 *
 * This distinction is necessary because the components of a vector image are
 * specified with respect to its preferred size for a rendering situation, not
 * to its actual rendered size.  For example, consider a 4px wide background
 * vector image with no height which contains a left-aligned
 * 2px wide black rectangle with height 100%.  If the background-size width is
 * auto (or 4px), the vector image will render 4px wide, and the black rectangle
 * will be 2px wide.  If the background-size width is 8px, the vector image will
 * render 8px wide, and the black rectangle will be 4px wide -- *not* 2px wide.
 * In both cases mSize.width will be 4px; but in the first case the returned
 * width will be 4px, while in the second case the returned width will be 8px.
 */
void
nsImageRenderer::SetPreferredSize(const CSSSizeOrRatio& aIntrinsicSize,
                                  const nsSize& aDefaultSize)
{
  mSize.width = aIntrinsicSize.mHasWidth
                  ? aIntrinsicSize.mWidth
                  : aDefaultSize.width;
  mSize.height = aIntrinsicSize.mHasHeight
                  ? aIntrinsicSize.mHeight
                  : aDefaultSize.height;
}

// Convert from nsImageRenderer flags to the flags we want to use for drawing in
// the imgIContainer namespace.
static uint32_t
ConvertImageRendererToDrawFlags(uint32_t aImageRendererFlags)
{
  uint32_t drawFlags = imgIContainer::FLAG_NONE;
  if (aImageRendererFlags & nsImageRenderer::FLAG_SYNC_DECODE_IMAGES) {
    drawFlags |= imgIContainer::FLAG_SYNC_DECODE;
  }
  if (aImageRendererFlags & nsImageRenderer::FLAG_PAINTING_TO_WINDOW) {
    drawFlags |= imgIContainer::FLAG_HIGH_QUALITY_SCALING;
  }
  return drawFlags;
}

/*
 *  SVG11: A luminanceToAlpha operation is equivalent to the following matrix operation:                                                   |
 *  | R' |     |      0        0        0  0  0 |   | R |
 *  | G' |     |      0        0        0  0  0 |   | G |
 *  | B' |  =  |      0        0        0  0  0 | * | B |
 *  | A' |     | 0.2125   0.7154   0.0721  0  0 |   | A |
 *  | 1  |     |      0        0        0  0  1 |   | 1 |
 */
static void
RGBALuminanceOperation(uint8_t *aData,
                       int32_t aStride,
                       const IntSize &aSize)
{
  int32_t redFactor = 55;    // 256 * 0.2125
  int32_t greenFactor = 183; // 256 * 0.7154
  int32_t blueFactor = 18;   // 256 * 0.0721

  for (int32_t y = 0; y < aSize.height; y++) {
    uint32_t *pixel = (uint32_t*)(aData + aStride * y);
    for (int32_t x = 0; x < aSize.width; x++) {
      *pixel = (((((*pixel & 0x00FF0000) >> 16) * redFactor) +
                 (((*pixel & 0x0000FF00) >>  8) * greenFactor) +
                  ((*pixel & 0x000000FF)        * blueFactor)) >> 8) << 24;
      pixel++;
    }
  }
}

DrawResult
nsImageRenderer::Draw(nsPresContext*       aPresContext,
                      gfxContext&          aRenderingContext,
                      const nsRect&        aDirtyRect,
                      const nsRect&        aDest,
                      const nsRect&        aFill,
                      const nsPoint&       aAnchor,
                      const nsSize&        aRepeatSize,
                      const CSSIntRect&    aSrc,
                      float                aOpacity)
{
  if (!IsReady()) {
    NS_NOTREACHED("Ensure PrepareImage() has returned true before calling me");
    return DrawResult::TEMPORARY_ERROR;
  }
  if (aDest.IsEmpty() || aFill.IsEmpty() ||
      mSize.width <= 0 || mSize.height <= 0) {
    return DrawResult::SUCCESS;
  }

  SamplingFilter samplingFilter = nsLayoutUtils::GetSamplingFilterForFrame(mForFrame);
  DrawResult result = DrawResult::SUCCESS;
  RefPtr<gfxContext> ctx = &aRenderingContext;
  IntRect tmpDTRect;

  if (ctx->CurrentOp() != CompositionOp::OP_OVER || mMaskOp == NS_STYLE_MASK_MODE_LUMINANCE) {
    gfxRect clipRect = ctx->GetClipExtents();
    tmpDTRect = RoundedOut(ToRect(clipRect));
    if (tmpDTRect.IsEmpty()) {
      return DrawResult::SUCCESS;
    }
    RefPtr<DrawTarget> tempDT =
      gfxPlatform::GetPlatform()->CreateSimilarSoftwareDrawTarget(ctx->GetDrawTarget(),
                                                                  tmpDTRect.Size(),
                                                                  SurfaceFormat::B8G8R8A8);
    if (!tempDT || !tempDT->IsValid()) {
      gfxDevCrash(LogReason::InvalidContext) << "ImageRenderer::Draw problem " << gfx::hexa(tempDT);
      return DrawResult::TEMPORARY_ERROR;
    }
    tempDT->SetTransform(Matrix::Translation(-tmpDTRect.TopLeft()));
    ctx = gfxContext::CreatePreservingTransformOrNull(tempDT);
    if (!ctx) {
      gfxDevCrash(LogReason::InvalidContext) << "ImageRenderer::Draw problem " << gfx::hexa(tempDT);
      return DrawResult::TEMPORARY_ERROR;
    }
  }

  switch (mType) {
    case eStyleImageType_Image:
    {
      CSSIntSize imageSize(nsPresContext::AppUnitsToIntCSSPixels(mSize.width),
                           nsPresContext::AppUnitsToIntCSSPixels(mSize.height));
      result =
        nsLayoutUtils::DrawBackgroundImage(*ctx, mForFrame,
                                           aPresContext,
                                           mImageContainer, imageSize,
                                           samplingFilter,
                                           aDest, aFill, aRepeatSize,
                                           aAnchor, aDirtyRect,
                                           ConvertImageRendererToDrawFlags(mFlags),
                                           mExtendMode, aOpacity);
      break;
    }
    case eStyleImageType_Gradient:
    {
      nsCSSGradientRenderer renderer =
        nsCSSGradientRenderer::Create(aPresContext, mGradientData, mSize);

      renderer.Paint(*ctx, aDest, aFill, aRepeatSize, aSrc, aDirtyRect, aOpacity);
      break;
    }
    case eStyleImageType_Element:
    {
      RefPtr<gfxDrawable> drawable = DrawableForElement(aDest, *ctx);
      if (!drawable) {
        NS_WARNING("Could not create drawable for element");
        return DrawResult::TEMPORARY_ERROR;
      }

      nsCOMPtr<imgIContainer> image(ImageOps::CreateFromDrawable(drawable));
      result =
        nsLayoutUtils::DrawImage(*ctx,
                                 aPresContext, image,
                                 samplingFilter, aDest, aFill, aAnchor, aDirtyRect,
                                 ConvertImageRendererToDrawFlags(mFlags),
                                 aOpacity);
      break;
    }
    case eStyleImageType_Null:
    default:
      break;
  }

  if (!tmpDTRect.IsEmpty()) {
    RefPtr<SourceSurface> surf = ctx->GetDrawTarget()->Snapshot();
    if (mMaskOp == NS_STYLE_MASK_MODE_LUMINANCE) {
      RefPtr<DataSourceSurface> maskData = surf->GetDataSurface();
      DataSourceSurface::MappedSurface map;
      if (!maskData->Map(DataSourceSurface::MapType::WRITE, &map)) {
        return result;
      }

      RGBALuminanceOperation(map.mData, map.mStride, maskData->GetSize());
      maskData->Unmap();
      surf = maskData;
    }

    DrawTarget* dt = aRenderingContext.GetDrawTarget();
    dt->DrawSurface(surf, Rect(tmpDTRect.x, tmpDTRect.y, tmpDTRect.width, tmpDTRect.height),
                    Rect(0, 0, tmpDTRect.width, tmpDTRect.height),
                    DrawSurfaceOptions(SamplingFilter::POINT),
                    DrawOptions(1.0f, aRenderingContext.CurrentOp()));
  }

  return result;
}

DrawResult
nsImageRenderer::BuildWebRenderDisplayItems(nsPresContext*       aPresContext,
                                            mozilla::wr::DisplayListBuilder&            aBuilder,
                                            const mozilla::layers::StackingContextHelper& aSc,
                                            nsTArray<WebRenderParentCommand>&           aParentCommands,
                                            mozilla::layers::WebRenderDisplayItemLayer* aLayer,
                                            const nsRect&        aDirtyRect,
                                            const nsRect&        aDest,
                                            const nsRect&        aFill,
                                            const nsPoint&       aAnchor,
                                            const nsSize&        aRepeatSize,
                                            const CSSIntRect&    aSrc,
                                            float                aOpacity)
{
  if (!IsReady()) {
    NS_NOTREACHED("Ensure PrepareImage() has returned true before calling me");
    return DrawResult::NOT_READY;
  }
  if (aDest.IsEmpty() || aFill.IsEmpty() ||
      mSize.width <= 0 || mSize.height <= 0) {
    return DrawResult::SUCCESS;
  }

  switch (mType) {
    case eStyleImageType_Gradient:
    {
      nsCSSGradientRenderer renderer =
        nsCSSGradientRenderer::Create(aPresContext, mGradientData, mSize);

      renderer.BuildWebRenderDisplayItems(aBuilder, aSc, aLayer, aDest, aFill, aRepeatSize, aSrc, aOpacity);
      break;
    }
    case eStyleImageType_Image:
    {
      RefPtr<layers::ImageContainer> container = mImageContainer->GetImageContainer(aLayer->WrManager(),
                                                                                    ConvertImageRendererToDrawFlags(mFlags));
      if (!container) {
        NS_WARNING("Failed to get image container");
        return DrawResult::NOT_READY;
      }
      Maybe<wr::ImageKey> key = aLayer->SendImageContainer(container, aParentCommands);
      if (key.isNothing()) {
        return DrawResult::BAD_IMAGE;
      }

      const int32_t appUnitsPerDevPixel = mForFrame->PresContext()->AppUnitsPerDevPixel();
      LayoutDeviceRect destRect = LayoutDeviceRect::FromAppUnits(
          aDest, appUnitsPerDevPixel);

      nsPoint firstTilePos = nsLayoutUtils::GetBackgroundFirstTilePos(aDest.TopLeft(),
                                                                      aFill.TopLeft(),
                                                                      aRepeatSize);
      LayoutDeviceRect fillRect = LayoutDeviceRect::FromAppUnits(
          nsRect(firstTilePos.x, firstTilePos.y,
                 aFill.XMost() - firstTilePos.x, aFill.YMost() - firstTilePos.y),
          appUnitsPerDevPixel);
      WrRect fill = aSc.ToRelativeWrRect(fillRect);
      WrRect clip = aSc.ToRelativeWrRect(
          LayoutDeviceRect::FromAppUnits(aFill, appUnitsPerDevPixel));

      LayoutDeviceSize gapSize = LayoutDeviceSize::FromAppUnits(
          aRepeatSize - aDest.Size(), appUnitsPerDevPixel);
      aBuilder.PushImage(fill, aBuilder.PushClipRegion(clip),
                         wr::ToWrSize(destRect.Size()), wr::ToWrSize(gapSize),
                         wr::ImageRendering::Auto, key.value());
      break;
    }
    default:
      break;
  }

  return DrawResult::SUCCESS;
}

already_AddRefed<gfxDrawable>
nsImageRenderer::DrawableForElement(const nsRect& aImageRect,
                                    gfxContext&  aContext)
{
  NS_ASSERTION(mType == eStyleImageType_Element,
               "DrawableForElement only makes sense if backed by an element");
  if (mPaintServerFrame) {
    // XXX(seth): In order to not pass FLAG_SYNC_DECODE_IMAGES here,
    // DrawableFromPaintServer would have to return a DrawResult indicating
    // whether any images could not be painted because they weren't fully
    // decoded. Even always passing FLAG_SYNC_DECODE_IMAGES won't eliminate all
    // problems, as it won't help if there are image which haven't finished
    // loading, but it's better than nothing.
    int32_t appUnitsPerDevPixel = mForFrame->PresContext()->AppUnitsPerDevPixel();
    nsRect destRect = aImageRect - aImageRect.TopLeft();
    nsIntSize roundedOut = destRect.ToOutsidePixels(appUnitsPerDevPixel).Size();
    IntSize imageSize(roundedOut.width, roundedOut.height);
    RefPtr<gfxDrawable> drawable =
      nsSVGIntegrationUtils::DrawableFromPaintServer(
        mPaintServerFrame, mForFrame, mSize, imageSize,
        aContext.GetDrawTarget(),
        aContext.CurrentMatrix(),
        nsSVGIntegrationUtils::FLAG_SYNC_DECODE_IMAGES);

    return drawable.forget();
  }
  NS_ASSERTION(mImageElementSurface.GetSourceSurface(), "Surface should be ready.");
  RefPtr<gfxDrawable> drawable = new gfxSurfaceDrawable(
                                mImageElementSurface.GetSourceSurface().get(),
                                mImageElementSurface.mSize);
  return drawable.forget();
}

DrawResult
nsImageRenderer::DrawLayer(nsPresContext*       aPresContext,
                           gfxContext&          aRenderingContext,
                           const nsRect&        aDest,
                           const nsRect&        aFill,
                           const nsPoint&       aAnchor,
                           const nsRect&        aDirty,
                           const nsSize&        aRepeatSize,
                           float                aOpacity)
{
  if (!IsReady()) {
    NS_NOTREACHED("Ensure PrepareImage() has returned true before calling me");
    return DrawResult::TEMPORARY_ERROR;
  }
  if (aDest.IsEmpty() || aFill.IsEmpty() ||
      mSize.width <= 0 || mSize.height <= 0) {
    return DrawResult::SUCCESS;
  }

  return Draw(aPresContext, aRenderingContext,
              aDirty, aDest, aFill, aAnchor, aRepeatSize,
              CSSIntRect(0, 0,
                         nsPresContext::AppUnitsToIntCSSPixels(mSize.width),
                         nsPresContext::AppUnitsToIntCSSPixels(mSize.height)),
              aOpacity);
}

DrawResult
nsImageRenderer::BuildWebRenderDisplayItemsForLayer(nsPresContext*       aPresContext,
                                                    mozilla::wr::DisplayListBuilder& aBuilder,
                                                    const mozilla::layers::StackingContextHelper& aSc,
                                                    nsTArray<WebRenderParentCommand>& aParentCommands,
                                                    WebRenderDisplayItemLayer*       aLayer,
                                                    const nsRect&        aDest,
                                                    const nsRect&        aFill,
                                                    const nsPoint&       aAnchor,
                                                    const nsRect&        aDirty,
                                                    const nsSize&        aRepeatSize,
                                                    float                aOpacity)
{
  if (!IsReady()) {
    NS_NOTREACHED("Ensure PrepareImage() has returned true before calling me");
    return mPrepareResult;
  }
  if (aDest.IsEmpty() || aFill.IsEmpty() ||
      mSize.width <= 0 || mSize.height <= 0) {
    return DrawResult::SUCCESS;
  }

  return BuildWebRenderDisplayItems(aPresContext, aBuilder, aSc, aParentCommands, aLayer,
                                    aDirty, aDest, aFill, aAnchor, aRepeatSize,
                                    CSSIntRect(0, 0,
                                               nsPresContext::AppUnitsToIntCSSPixels(mSize.width),
                                               nsPresContext::AppUnitsToIntCSSPixels(mSize.height)),
                                    aOpacity);
}

/**
 * Compute the size and position of the master copy of the image. I.e., a single
 * tile used to fill the dest rect.
 * aFill The destination rect to be filled
 * aHFill and aVFill are the repeat patterns for the component -
 * NS_STYLE_BORDER_IMAGE_REPEAT_* - i.e., how a tiling unit is used to fill aFill
 * aUnitSize The size of the source rect in dest coords.
 */
static nsRect
ComputeTile(nsRect&              aFill,
            uint8_t              aHFill,
            uint8_t              aVFill,
            const nsSize&        aUnitSize,
            nsSize&              aRepeatSize)
{
  nsRect tile;
  switch (aHFill) {
  case NS_STYLE_BORDER_IMAGE_REPEAT_STRETCH:
    tile.x = aFill.x;
    tile.width = aFill.width;
    aRepeatSize.width = tile.width;
    break;
  case NS_STYLE_BORDER_IMAGE_REPEAT_REPEAT:
    tile.x = aFill.x + aFill.width/2 - aUnitSize.width/2;
    tile.width = aUnitSize.width;
    aRepeatSize.width = tile.width;
    break;
  case NS_STYLE_BORDER_IMAGE_REPEAT_ROUND:
    tile.x = aFill.x;
    tile.width = nsCSSRendering::ComputeRoundedSize(aUnitSize.width,
                                                    aFill.width);
    aRepeatSize.width = tile.width;
    break;
  case NS_STYLE_BORDER_IMAGE_REPEAT_SPACE:
    {
      nscoord space;
      aRepeatSize.width =
        nsCSSRendering::ComputeBorderSpacedRepeatSize(aUnitSize.width,
                                                      aFill.width, space);
      tile.x = aFill.x + space;
      tile.width = aUnitSize.width;
      aFill.x = tile.x;
      aFill.width = aFill.width - space * 2;
    }
    break;
  default:
    NS_NOTREACHED("unrecognized border-image fill style");
  }

  switch (aVFill) {
  case NS_STYLE_BORDER_IMAGE_REPEAT_STRETCH:
    tile.y = aFill.y;
    tile.height = aFill.height;
    aRepeatSize.height = tile.height;
    break;
  case NS_STYLE_BORDER_IMAGE_REPEAT_REPEAT:
    tile.y = aFill.y + aFill.height/2 - aUnitSize.height/2;
    tile.height = aUnitSize.height;
    aRepeatSize.height = tile.height;
    break;
  case NS_STYLE_BORDER_IMAGE_REPEAT_ROUND:
    tile.y = aFill.y;
    tile.height = nsCSSRendering::ComputeRoundedSize(aUnitSize.height,
                                                     aFill.height);
    aRepeatSize.height = tile.height;
    break;
  case NS_STYLE_BORDER_IMAGE_REPEAT_SPACE:
    {
      nscoord space;
      aRepeatSize.height =
        nsCSSRendering::ComputeBorderSpacedRepeatSize(aUnitSize.height,
                                                      aFill.height, space);
      tile.y = aFill.y + space;
      tile.height = aUnitSize.height;
      aFill.y = tile.y;
      aFill.height = aFill.height - space * 2;
    }
    break;
  default:
    NS_NOTREACHED("unrecognized border-image fill style");
  }

  return tile;
}

/**
 * Returns true if the given set of arguments will require the tiles which fill
 * the dest rect to be scaled from the source tile. See comment on ComputeTile
 * for argument descriptions.
 */
static bool
RequiresScaling(const nsRect&        aFill,
                uint8_t              aHFill,
                uint8_t              aVFill,
                const nsSize&        aUnitSize)
{
  // If we have no tiling in either direction, we can skip the intermediate
  // scaling step.
  return (aHFill != NS_STYLE_BORDER_IMAGE_REPEAT_STRETCH ||
          aVFill != NS_STYLE_BORDER_IMAGE_REPEAT_STRETCH) &&
         (aUnitSize.width != aFill.width ||
          aUnitSize.height != aFill.height);
}

DrawResult
nsImageRenderer::DrawBorderImageComponent(nsPresContext*       aPresContext,
                                          gfxContext&          aRenderingContext,
                                          const nsRect&        aDirtyRect,
                                          const nsRect&        aFill,
                                          const CSSIntRect&    aSrc,
                                          uint8_t              aHFill,
                                          uint8_t              aVFill,
                                          const nsSize&        aUnitSize,
                                          uint8_t              aIndex,
                                          const Maybe<nsSize>& aSVGViewportSize,
                                          const bool           aHasIntrinsicRatio)
{
  if (!IsReady()) {
    NS_NOTREACHED("Ensure PrepareImage() has returned true before calling me");
    return DrawResult::BAD_ARGS;
  }
  if (aFill.IsEmpty() || aSrc.IsEmpty()) {
    return DrawResult::SUCCESS;
  }

  if (mType == eStyleImageType_Image || mType == eStyleImageType_Element) {
    nsCOMPtr<imgIContainer> subImage;

    // To draw one portion of an image into a border component, we stretch that
    // portion to match the size of that border component and then draw onto.
    // However, preserveAspectRatio attribute of a SVG image may break this rule.
    // To get correct rendering result, we add
    // FLAG_FORCE_PRESERVEASPECTRATIO_NONE flag here, to tell mImage to ignore
    // preserveAspectRatio attribute, and always do non-uniform stretch.
    uint32_t drawFlags = ConvertImageRendererToDrawFlags(mFlags) |
                           imgIContainer::FLAG_FORCE_PRESERVEASPECTRATIO_NONE;
    // For those SVG image sources which don't have fixed aspect ratio (i.e.
    // without viewport size and viewBox), we should scale the source uniformly
    // after the viewport size is decided by "Default Sizing Algorithm".
    if (!aHasIntrinsicRatio) {
      drawFlags = drawFlags | imgIContainer::FLAG_FORCE_UNIFORM_SCALING;
    }
    // Retrieve or create the subimage we'll draw.
    nsIntRect srcRect(aSrc.x, aSrc.y, aSrc.width, aSrc.height);
    if (mType == eStyleImageType_Image) {
      if ((subImage = mImage->GetSubImage(aIndex)) == nullptr) {
        subImage = ImageOps::Clip(mImageContainer, srcRect, aSVGViewportSize);
        mImage->SetSubImage(aIndex, subImage);
      }
    } else {
      // This path, for eStyleImageType_Element, is currently slower than it
      // needs to be because we don't cache anything. (In particular, if we have
      // to draw to a temporary surface inside ClippedImage, we don't cache that
      // temporary surface since we immediately throw the ClippedImage we create
      // here away.) However, if we did cache, we'd need to know when to
      // invalidate that cache, and it's not clear that it's worth the trouble
      // since using border-image with -moz-element is rare.

      RefPtr<gfxDrawable> drawable =
        DrawableForElement(nsRect(nsPoint(), mSize),
                           aRenderingContext);
      if (!drawable) {
        NS_WARNING("Could not create drawable for element");
        return DrawResult::TEMPORARY_ERROR;
      }

      nsCOMPtr<imgIContainer> image(ImageOps::CreateFromDrawable(drawable));
      subImage = ImageOps::Clip(image, srcRect, aSVGViewportSize);
    }

    MOZ_ASSERT(!aSVGViewportSize ||
               subImage->GetType() == imgIContainer::TYPE_VECTOR);

    SamplingFilter samplingFilter = nsLayoutUtils::GetSamplingFilterForFrame(mForFrame);

    if (!RequiresScaling(aFill, aHFill, aVFill, aUnitSize)) {
      return nsLayoutUtils::DrawSingleImage(aRenderingContext,
                                            aPresContext,
                                            subImage,
                                            samplingFilter,
                                            aFill, aDirtyRect,
                                            /* no SVGImageContext */ Nothing(),
                                            drawFlags);
    }

    nsSize repeatSize;
    nsRect fillRect(aFill);
    nsRect tile = ComputeTile(fillRect, aHFill, aVFill, aUnitSize, repeatSize);
    CSSIntSize imageSize(srcRect.width, srcRect.height);
    return nsLayoutUtils::DrawBackgroundImage(aRenderingContext,
                                              mForFrame, aPresContext,
                                              subImage, imageSize, samplingFilter,
                                              tile, fillRect, repeatSize,
                                              tile.TopLeft(), aDirtyRect,
                                              drawFlags,
                                              ExtendMode::CLAMP, 1.0);
  }

  nsSize repeatSize(aFill.Size());
  nsRect fillRect(aFill);
  nsRect destTile = RequiresScaling(fillRect, aHFill, aVFill, aUnitSize)
                  ? ComputeTile(fillRect, aHFill, aVFill, aUnitSize, repeatSize)
                  : fillRect;
  return Draw(aPresContext, aRenderingContext, aDirtyRect, destTile,
              fillRect, destTile.TopLeft(), repeatSize, aSrc);
}

bool
nsImageRenderer::IsRasterImage()
{
  if (mType != eStyleImageType_Image || !mImageContainer)
    return false;
  return mImageContainer->GetType() == imgIContainer::TYPE_RASTER;
}

bool
nsImageRenderer::IsAnimatedImage()
{
  if (mType != eStyleImageType_Image || !mImageContainer)
    return false;
  bool animated = false;
  if (NS_SUCCEEDED(mImageContainer->GetAnimated(&animated)) && animated)
    return true;

  return false;
}

already_AddRefed<imgIContainer>
nsImageRenderer::GetImage()
{
  if (mType != eStyleImageType_Image || !mImageContainer) {
    return nullptr;
  }

  nsCOMPtr<imgIContainer> image = mImageContainer;
  return image.forget();
}

bool
nsImageRenderer::IsImageContainerAvailable(layers::LayerManager* aManager, uint32_t aFlags)
{
  if (!mImageContainer) {
    return false;
  }
  return mImageContainer->IsImageContainerAvailable(aManager, aFlags);
}

void
nsImageRenderer::PurgeCacheForViewportChange(
  const Maybe<nsSize>& aSVGViewportSize, const bool aHasIntrinsicRatio)
{
  // Check if we should flush the cached data - only vector images need to do
  // the check since they might not have fixed ratio.
  if (mImageContainer &&
      mImageContainer->GetType() == imgIContainer::TYPE_VECTOR) {
    mImage->PurgeCacheForViewportChange(aSVGViewportSize, aHasIntrinsicRatio);
  }
}

already_AddRefed<nsStyleGradient>
nsImageRenderer::GetGradientData()
{
  RefPtr<nsStyleGradient> res = mGradientData;
  return res.forget();
}

