/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClippedImage.h"

#include <algorithm>
#include <new>      // Workaround for bug in VS10; see bug 981264.
#include <cmath>
#include <utility>

#include "gfxDrawable.h"
#include "gfxPlatform.h"
#include "gfxUtils.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/Move.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Pair.h"
#include "mozilla/Tuple.h"

#include "ImageRegion.h"
#include "Orientation.h"
#include "SVGImageContext.h"

namespace mozilla {

using namespace gfx;
using layers::LayerManager;
using layers::ImageContainer;
using std::make_pair;
using std::max;
using std::modf;
using std::pair;

namespace image {

class ClippedImageCachedSurface
{
public:
  ClippedImageCachedSurface(already_AddRefed<SourceSurface> aSurface,
                            const nsIntSize& aSize,
                            const Maybe<SVGImageContext>& aSVGContext,
                            float aFrame,
                            uint32_t aFlags,
                            DrawResult aDrawResult)
    : mSurface(aSurface)
    , mSize(aSize)
    , mSVGContext(aSVGContext)
    , mFrame(aFrame)
    , mFlags(aFlags)
    , mDrawResult(aDrawResult)
  {
    MOZ_ASSERT(mSurface, "Must have a valid surface");
  }

  bool Matches(const nsIntSize& aSize,
               const Maybe<SVGImageContext>& aSVGContext,
               float aFrame,
               uint32_t aFlags) const
  {
    return mSize == aSize &&
           mSVGContext == aSVGContext &&
           mFrame == aFrame &&
           mFlags == aFlags;
  }

  already_AddRefed<SourceSurface> Surface() const
  {
    RefPtr<SourceSurface> surf(mSurface);
    return surf.forget();
  }

  DrawResult GetDrawResult() const
  {
    return mDrawResult;
  }

  bool NeedsRedraw() const
  {
    return mDrawResult != DrawResult::SUCCESS &&
           mDrawResult != DrawResult::BAD_IMAGE;
  }

private:
  RefPtr<SourceSurface>  mSurface;
  const nsIntSize        mSize;
  Maybe<SVGImageContext> mSVGContext;
  const float            mFrame;
  const uint32_t         mFlags;
  const DrawResult       mDrawResult;
};

class DrawSingleTileCallback : public gfxDrawingCallback
{
public:
  DrawSingleTileCallback(ClippedImage* aImage,
                         const nsIntSize& aSize,
                         const Maybe<SVGImageContext>& aSVGContext,
                         uint32_t aWhichFrame,
                         uint32_t aFlags,
                         float aOpacity)
    : mImage(aImage)
    , mSize(aSize)
    , mSVGContext(aSVGContext)
    , mWhichFrame(aWhichFrame)
    , mFlags(aFlags)
    , mDrawResult(DrawResult::NOT_READY)
    , mOpacity(aOpacity)
  {
    MOZ_ASSERT(mImage, "Must have an image to clip");
  }

  virtual bool operator()(gfxContext* aContext,
                          const gfxRect& aFillRect,
                          const SamplingFilter aSamplingFilter,
                          const gfxMatrix& aTransform)
  {
    MOZ_ASSERT(aTransform.IsIdentity(),
               "Caller is probably CreateSamplingRestrictedDrawable, "
               "which should not happen");

    // Draw the image. |gfxCallbackDrawable| always calls this function with
    // arguments that guarantee we never tile.
    mDrawResult =
      mImage->DrawSingleTile(aContext, mSize, ImageRegion::Create(aFillRect),
                             mWhichFrame, aSamplingFilter, mSVGContext, mFlags,
                             mOpacity);

    return true;
  }

  DrawResult GetDrawResult() { return mDrawResult; }

private:
  RefPtr<ClippedImage>        mImage;
  const nsIntSize               mSize;
  const Maybe<SVGImageContext>& mSVGContext;
  const uint32_t                mWhichFrame;
  const uint32_t                mFlags;
  DrawResult                    mDrawResult;
  float                         mOpacity;
};

ClippedImage::ClippedImage(Image* aImage,
                           nsIntRect aClip,
                           const Maybe<nsSize>& aSVGViewportSize)
  : ImageWrapper(aImage)
  , mClip(aClip)
{
  MOZ_ASSERT(aImage != nullptr, "ClippedImage requires an existing Image");
  MOZ_ASSERT_IF(aSVGViewportSize,
                aImage->GetType() == imgIContainer::TYPE_VECTOR);
  if (aSVGViewportSize) {
    mSVGViewportSize = Some(aSVGViewportSize->ToNearestPixels(
                                        nsPresContext::AppUnitsPerCSSPixel()));
  }
}

ClippedImage::~ClippedImage()
{ }

bool
ClippedImage::ShouldClip()
{
  // We need to evaluate the clipping region against the image's width and
  // height once they're available to determine if it's valid and whether we
  // actually need to do any work. We may fail if the image's width and height
  // aren't available yet, in which case we'll try again later.
  if (mShouldClip.isNothing()) {
    int32_t width, height;
    RefPtr<ProgressTracker> progressTracker =
      InnerImage()->GetProgressTracker();
    if (InnerImage()->HasError()) {
      // If there's a problem with the inner image we'll let it handle
      // everything.
      mShouldClip.emplace(false);
    } else if (mSVGViewportSize && !mSVGViewportSize->IsEmpty()) {
      // Clamp the clipping region to the size of the SVG viewport.
      nsIntRect svgViewportRect(nsIntPoint(0,0), *mSVGViewportSize);

      mClip = mClip.Intersect(svgViewportRect);

      // If the clipping region is the same size as the SVG viewport size
      // we don't have to do anything.
      mShouldClip.emplace(!mClip.IsEqualInterior(svgViewportRect));
    } else if (NS_SUCCEEDED(InnerImage()->GetWidth(&width)) && width > 0 &&
               NS_SUCCEEDED(InnerImage()->GetHeight(&height)) && height > 0) {
      // Clamp the clipping region to the size of the underlying image.
      mClip = mClip.Intersect(nsIntRect(0, 0, width, height));

      // If the clipping region is the same size as the underlying image we
      // don't have to do anything.
      mShouldClip.emplace(!mClip.IsEqualInterior(nsIntRect(0, 0, width,
                                                           height)));
    } else if (progressTracker &&
               !(progressTracker->GetProgress() & FLAG_LOAD_COMPLETE)) {
      // The image just hasn't finished loading yet. We don't yet know whether
      // clipping with be needed or not for now. Just return without memorizing
      // anything.
      return false;
    } else {
      // We have a fully loaded image without a clearly defined width and
      // height. This can happen with SVG images.
      mShouldClip.emplace(false);
    }
  }

  MOZ_ASSERT(mShouldClip.isSome(), "Should have computed a result");
  return *mShouldClip;
}

NS_IMPL_ISUPPORTS_INHERITED0(ClippedImage, ImageWrapper)

NS_IMETHODIMP
ClippedImage::GetWidth(int32_t* aWidth)
{
  if (!ShouldClip()) {
    return InnerImage()->GetWidth(aWidth);
  }

  *aWidth = mClip.Width();
  return NS_OK;
}

NS_IMETHODIMP
ClippedImage::GetHeight(int32_t* aHeight)
{
  if (!ShouldClip()) {
    return InnerImage()->GetHeight(aHeight);
  }

  *aHeight = mClip.Height();
  return NS_OK;
}

NS_IMETHODIMP
ClippedImage::GetIntrinsicSize(nsSize* aSize)
{
  if (!ShouldClip()) {
    return InnerImage()->GetIntrinsicSize(aSize);
  }

  *aSize = nsSize(mClip.Width(), mClip.Height());
  return NS_OK;
}

NS_IMETHODIMP
ClippedImage::GetIntrinsicRatio(nsSize* aRatio)
{
  if (!ShouldClip()) {
    return InnerImage()->GetIntrinsicRatio(aRatio);
  }

  *aRatio = nsSize(mClip.Width(), mClip.Height());
  return NS_OK;
}

NS_IMETHODIMP_(already_AddRefed<SourceSurface>)
ClippedImage::GetFrame(uint32_t aWhichFrame,
                       uint32_t aFlags)
{
  DrawResult result;
  RefPtr<SourceSurface> surface;
  Tie(result, surface) = GetFrameInternal(mClip.Size(), Nothing(), aWhichFrame, aFlags, 1.0);
  return surface.forget();
}

NS_IMETHODIMP_(already_AddRefed<SourceSurface>)
ClippedImage::GetFrameAtSize(const IntSize& aSize,
                             uint32_t aWhichFrame,
                             uint32_t aFlags)
{
  // XXX(seth): It'd be nice to support downscale-during-decode for this case,
  // but right now we just fall back to the intrinsic size.
  return GetFrame(aWhichFrame, aFlags);
}

Pair<DrawResult, RefPtr<SourceSurface>>
ClippedImage::GetFrameInternal(const nsIntSize& aSize,
                               const Maybe<SVGImageContext>& aSVGContext,
                               uint32_t aWhichFrame,
                               uint32_t aFlags,
                               float aOpacity)
{
  if (!ShouldClip()) {
    RefPtr<SourceSurface> surface = InnerImage()->GetFrame(aWhichFrame, aFlags);
    return MakePair(surface ? DrawResult::SUCCESS : DrawResult::NOT_READY,
                    Move(surface));
  }

  float frameToDraw = InnerImage()->GetFrameIndex(aWhichFrame);
  if (!mCachedSurface ||
      !mCachedSurface->Matches(aSize, aSVGContext, frameToDraw, aFlags) ||
      mCachedSurface->NeedsRedraw()) {
    // Create a surface to draw into.
    RefPtr<DrawTarget> target = gfxPlatform::GetPlatform()->
      CreateOffscreenContentDrawTarget(IntSize(aSize.width, aSize.height),
                                       SurfaceFormat::B8G8R8A8);
    if (!target || !target->IsValid()) {
      NS_ERROR("Could not create a DrawTarget");
      return MakePair(DrawResult::TEMPORARY_ERROR, RefPtr<SourceSurface>());
    }

    RefPtr<gfxContext> ctx = gfxContext::CreateOrNull(target);
    MOZ_ASSERT(ctx); // already checked the draw target above

    // Create our callback.
    RefPtr<DrawSingleTileCallback> drawTileCallback =
      new DrawSingleTileCallback(this, aSize, aSVGContext, aWhichFrame, aFlags,
                                 aOpacity);
    RefPtr<gfxDrawable> drawable =
      new gfxCallbackDrawable(drawTileCallback, aSize);

    // Actually draw. The callback will end up invoking DrawSingleTile.
    gfxUtils::DrawPixelSnapped(ctx, drawable, SizeDouble(aSize),
                               ImageRegion::Create(aSize),
                               SurfaceFormat::B8G8R8A8,
                               SamplingFilter::LINEAR,
                               imgIContainer::FLAG_CLAMP);

    // Cache the resulting surface.
    mCachedSurface =
      MakeUnique<ClippedImageCachedSurface>(target->Snapshot(), aSize, aSVGContext,
                                            frameToDraw, aFlags,
                                            drawTileCallback->GetDrawResult());
  }

  MOZ_ASSERT(mCachedSurface, "Should have a cached surface now");
  RefPtr<SourceSurface> surface = mCachedSurface->Surface();
  return MakePair(mCachedSurface->GetDrawResult(), Move(surface));
}

NS_IMETHODIMP_(bool)
ClippedImage::IsImageContainerAvailable(LayerManager* aManager, uint32_t aFlags)
{
  if (!ShouldClip()) {
    return InnerImage()->IsImageContainerAvailable(aManager, aFlags);
  }
  return false;
}

NS_IMETHODIMP_(already_AddRefed<ImageContainer>)
ClippedImage::GetImageContainer(LayerManager* aManager, uint32_t aFlags)
{
  // XXX(seth): We currently don't have a way of clipping the result of
  // GetImageContainer. We work around this by always returning null, but if it
  // ever turns out that ClippedImage is widely used on codepaths that can
  // actually benefit from GetImageContainer, it would be a good idea to fix
  // that method for performance reasons.

  if (!ShouldClip()) {
    return InnerImage()->GetImageContainer(aManager, aFlags);
  }

  return nullptr;
}

NS_IMETHODIMP_(bool)
ClippedImage::IsImageContainerAvailableAtSize(LayerManager* aManager,
                                              const IntSize& aSize,
                                              uint32_t aFlags)
{
  if (!ShouldClip()) {
    return InnerImage()->IsImageContainerAvailableAtSize(aManager, aSize, aFlags);
  }
  return false;
}

NS_IMETHODIMP_(already_AddRefed<ImageContainer>)
ClippedImage::GetImageContainerAtSize(LayerManager* aManager,
                                      const IntSize& aSize,
                                      const Maybe<SVGImageContext>& aSVGContext,
                                      uint32_t aFlags)
{
  // XXX(seth): We currently don't have a way of clipping the result of
  // GetImageContainer. We work around this by always returning null, but if it
  // ever turns out that ClippedImage is widely used on codepaths that can
  // actually benefit from GetImageContainer, it would be a good idea to fix
  // that method for performance reasons.

  if (!ShouldClip()) {
    return InnerImage()->GetImageContainerAtSize(aManager, aSize,
                                                 aSVGContext, aFlags);
  }

  return nullptr;
}

static bool
MustCreateSurface(gfxContext* aContext,
                  const nsIntSize& aSize,
                  const ImageRegion& aRegion,
                  const uint32_t aFlags)
{
  gfxRect imageRect(0, 0, aSize.width, aSize.height);
  bool willTile = !imageRect.Contains(aRegion.Rect()) &&
                  !(aFlags & imgIContainer::FLAG_CLAMP);
  bool willResample = aContext->CurrentMatrix().HasNonIntegerTranslation() &&
                      (willTile || !aRegion.RestrictionContains(imageRect));
  return willTile || willResample;
}

NS_IMETHODIMP_(DrawResult)
ClippedImage::Draw(gfxContext* aContext,
                   const nsIntSize& aSize,
                   const ImageRegion& aRegion,
                   uint32_t aWhichFrame,
                   SamplingFilter aSamplingFilter,
                   const Maybe<SVGImageContext>& aSVGContext,
                   uint32_t aFlags,
                   float aOpacity)
{
  if (!ShouldClip()) {
    return InnerImage()->Draw(aContext, aSize, aRegion, aWhichFrame,
                              aSamplingFilter, aSVGContext, aFlags, aOpacity);
  }

  // Check for tiling. If we need to tile then we need to create a
  // gfxCallbackDrawable to handle drawing for us.
  if (MustCreateSurface(aContext, aSize, aRegion, aFlags)) {
    // Create a temporary surface containing a single tile of this image.
    // GetFrame will call DrawSingleTile internally.
    DrawResult result;
    RefPtr<SourceSurface> surface;
    Tie(result, surface) =
      GetFrameInternal(aSize, aSVGContext, aWhichFrame, aFlags, aOpacity);
    if (!surface) {
      MOZ_ASSERT(result != DrawResult::SUCCESS);
      return result;
    }

    // Create a drawable from that surface.
    RefPtr<gfxSurfaceDrawable> drawable =
      new gfxSurfaceDrawable(surface, aSize);

    // Draw.
    gfxUtils::DrawPixelSnapped(aContext, drawable, SizeDouble(aSize), aRegion,
                               SurfaceFormat::B8G8R8A8, aSamplingFilter,
                               aOpacity);

    return result;
  }

  return DrawSingleTile(aContext, aSize, aRegion, aWhichFrame,
                        aSamplingFilter, aSVGContext, aFlags, aOpacity);
}

DrawResult
ClippedImage::DrawSingleTile(gfxContext* aContext,
                             const nsIntSize& aSize,
                             const ImageRegion& aRegion,
                             uint32_t aWhichFrame,
                             SamplingFilter aSamplingFilter,
                             const Maybe<SVGImageContext>& aSVGContext,
                             uint32_t aFlags,
                             float aOpacity)
{
  MOZ_ASSERT(!MustCreateSurface(aContext, aSize, aRegion, aFlags),
             "Shouldn't need to create a surface");

  gfxRect clip(mClip.x, mClip.y, mClip.Width(), mClip.Height());
  nsIntSize size(aSize), innerSize(aSize);
  bool needScale = false;
  if (mSVGViewportSize && !mSVGViewportSize->IsEmpty()) {
    innerSize = *mSVGViewportSize;
    needScale = true;
  } else if (NS_SUCCEEDED(InnerImage()->GetWidth(&innerSize.width)) &&
             NS_SUCCEEDED(InnerImage()->GetHeight(&innerSize.height))) {
    needScale = true;
  } else {
    MOZ_ASSERT_UNREACHABLE(
               "If ShouldClip() led us to draw then we should never get here");
  }

  if (needScale) {
    double scaleX = aSize.width / clip.Width();
    double scaleY = aSize.height / clip.Height();

    // Map the clip and size to the scale requested by the caller.
    clip.Scale(scaleX, scaleY);
    size = innerSize;
    size.Scale(scaleX, scaleY);
  }

  // We restrict our drawing to only the clipping region, and translate so that
  // the clipping region is placed at the position the caller expects.
  ImageRegion region(aRegion);
  region.MoveBy(clip.x, clip.y);
  region = region.Intersect(clip);

  gfxContextMatrixAutoSaveRestore saveMatrix(aContext);
  aContext->Multiply(gfxMatrix::Translation(-clip.x, -clip.y));

  auto unclipViewport = [&](const SVGImageContext& aOldContext) {
    // Map the viewport to the inner image. Note that we don't take the aSize
    // parameter of imgIContainer::Draw into account, just the clipping region.
    // The size in pixels at which the output will ultimately be drawn is
    // irrelevant here since the purpose of the SVG viewport size is to
    // determine what *region* of the SVG document will be drawn.
    SVGImageContext context(aOldContext);
    auto oldViewport = aOldContext.GetViewportSize();
    if (oldViewport) {
      CSSIntSize newViewport;
      newViewport.width =
        ceil(oldViewport->width * double(innerSize.width) / mClip.Width());
      newViewport.height =
        ceil(oldViewport->height * double(innerSize.height) / mClip.Height());
      context.SetViewportSize(Some(newViewport));
    }
    return context;
  };

  return InnerImage()->Draw(aContext, size, region,
                            aWhichFrame, aSamplingFilter,
                            aSVGContext.map(unclipViewport),
                            aFlags, aOpacity);
}

NS_IMETHODIMP
ClippedImage::RequestDiscard()
{
  // We're very aggressive about discarding.
  mCachedSurface = nullptr;

  return InnerImage()->RequestDiscard();
}

NS_IMETHODIMP_(Orientation)
ClippedImage::GetOrientation()
{
  // XXX(seth): This should not actually be here; this is just to work around a
  // what appears to be a bug in MSVC's linker.
  return InnerImage()->GetOrientation();
}

nsIntSize
ClippedImage::OptimalImageSizeForDest(const gfxSize& aDest,
                                      uint32_t aWhichFrame,
                                      SamplingFilter aSamplingFilter,
                                      uint32_t aFlags)
{
  if (!ShouldClip()) {
    return InnerImage()->OptimalImageSizeForDest(aDest, aWhichFrame,
                                                 aSamplingFilter, aFlags);
  }

  int32_t imgWidth, imgHeight;
  bool needScale = false;
  bool forceUniformScaling = false;
  if (mSVGViewportSize && !mSVGViewportSize->IsEmpty()) {
    imgWidth = mSVGViewportSize->width;
    imgHeight = mSVGViewportSize->height;
    needScale = true;
    forceUniformScaling = (aFlags & imgIContainer::FLAG_FORCE_UNIFORM_SCALING);
  } else if (NS_SUCCEEDED(InnerImage()->GetWidth(&imgWidth)) &&
             NS_SUCCEEDED(InnerImage()->GetHeight(&imgHeight))) {
    needScale = true;
  }

  if (needScale) {
    // To avoid ugly sampling artifacts, ClippedImage needs the image size to
    // be chosen such that the clipping region lies on pixel boundaries.

    // First, we select a scale that's good for ClippedImage. An integer
    // multiple of the size of the clipping region is always fine.
    IntSize scale = IntSize::Ceil(aDest.width / mClip.Width(),
                                  aDest.height / mClip.Height());

    if (forceUniformScaling) {
      scale.width = scale.height = max(scale.height, scale.width);
    }

    // Determine the size we'd prefer to render the inner image at, and ask the
    // inner image what size we should actually use.
    gfxSize desiredSize(imgWidth * scale.width, imgHeight * scale.height);
    nsIntSize innerDesiredSize =
      InnerImage()->OptimalImageSizeForDest(desiredSize, aWhichFrame,
                                            aSamplingFilter, aFlags);

    // To get our final result, we take the inner image's desired size and
    // determine how large the clipped region would be at that scale. (Again, we
    // ensure an integer multiple of the size of the clipping region.)
    IntSize finalScale = IntSize::Ceil(double(innerDesiredSize.width) / imgWidth,
                                       double(innerDesiredSize.height) / imgHeight);
    return mClip.Size() * finalScale;
  }

  MOZ_ASSERT(false,
             "If ShouldClip() led us to draw then we should never get here");
  return InnerImage()->OptimalImageSizeForDest(aDest, aWhichFrame,
                                               aSamplingFilter, aFlags);
}

NS_IMETHODIMP_(nsIntRect)
ClippedImage::GetImageSpaceInvalidationRect(const nsIntRect& aRect)
{
  if (!ShouldClip()) {
    return InnerImage()->GetImageSpaceInvalidationRect(aRect);
  }

  nsIntRect rect(InnerImage()->GetImageSpaceInvalidationRect(aRect));
  rect = rect.Intersect(mClip);
  rect.MoveBy(-mClip.x, -mClip.y);
  return rect;
}

} // namespace image
} // namespace mozilla
