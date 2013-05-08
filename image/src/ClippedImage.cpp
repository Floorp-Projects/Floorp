/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxDrawable.h"
#include "gfxPlatform.h"
#include "gfxUtils.h"
#include "mozilla/dom/SVGSVGElement.h"

#include "ClippedImage.h"

using mozilla::layers::LayerManager;
using mozilla::layers::ImageContainer;

namespace mozilla {
namespace image {

class ClippedImageCachedSurface
{
public:
  ClippedImageCachedSurface(mozilla::gfx::DrawTarget* aSurface,
                            const nsIntSize& aViewportSize,
                            const SVGImageContext* aSVGContext,
                            float aFrame,
                            uint32_t aFlags)
    : mSurface(aSurface)
    , mViewportSize(aViewportSize)
    , mFrame(aFrame)
    , mFlags(aFlags)
  {
    MOZ_ASSERT(mSurface, "Must have a valid surface");
    if (aSVGContext) {
      mSVGContext.construct(*aSVGContext);
    }
  }

  bool Matches(const nsIntSize& aViewportSize,
               const SVGImageContext* aSVGContext,
               float aFrame,
               uint32_t aFlags)
  {
    bool matchesSVGContext = (!aSVGContext && mSVGContext.empty()) ||
                             *aSVGContext == mSVGContext.ref();
    return mViewportSize == aViewportSize &&
           matchesSVGContext &&
           mFrame == aFrame &&
           mFlags == aFlags;
  }

  already_AddRefed<gfxASurface> Surface() {
    nsRefPtr<gfxASurface> surf = gfxPlatform::GetPlatform()->GetThebesSurfaceForDrawTarget(mSurface);
    return surf.forget();
  }

private:
  nsRefPtr<mozilla::gfx::DrawTarget> mSurface;
  const nsIntSize                    mViewportSize;
  Maybe<SVGImageContext>             mSVGContext;
  const float                        mFrame;
  const uint32_t                     mFlags;
};

class DrawSingleTileCallback : public gfxDrawingCallback
{
public:
  DrawSingleTileCallback(ClippedImage* aImage,
                         const nsIntRect& aClip,
                         const nsIntSize& aViewportSize,
                         const SVGImageContext* aSVGContext,
                         uint32_t aWhichFrame,
                         uint32_t aFlags)
    : mImage(aImage)
    , mClip(aClip)
    , mViewportSize(aViewportSize)
    , mSVGContext(aSVGContext)
    , mWhichFrame(aWhichFrame)
    , mFlags(aFlags)
  {
    MOZ_ASSERT(mImage, "Must have an image to clip");
  }

  virtual bool operator()(gfxContext* aContext,
                          const gfxRect& aFillRect,
                          const gfxPattern::GraphicsFilter& aFilter,
                          const gfxMatrix& aTransform)
  {
    // Draw the image. |gfxCallbackDrawable| always calls this function with
    // arguments that guarantee we never tile.
    mImage->DrawSingleTile(aContext, aFilter, aTransform, aFillRect, mClip,
                           mViewportSize, mSVGContext, mWhichFrame, mFlags);

    return true;
  }

private:
  nsRefPtr<ClippedImage> mImage;
  const nsIntRect        mClip;
  const nsIntSize        mViewportSize;
  const SVGImageContext* mSVGContext;
  const uint32_t         mWhichFrame;
  const uint32_t         mFlags;
};

ClippedImage::ClippedImage(Image* aImage,
                           nsIntRect aClip)
  : ImageWrapper(aImage)
  , mClip(aClip)
{
  MOZ_ASSERT(aImage != nullptr, "ClippedImage requires an existing Image");
}

ClippedImage::~ClippedImage()
{ }

bool
ClippedImage::ShouldClip()
{
  // We need to evaluate the clipping region against the image's width and height
  // once they're available to determine if it's valid and whether we actually
  // need to do any work. We may fail if the image's width and height aren't
  // available yet, in which case we'll try again later.
  if (mShouldClip.empty()) {
    int32_t width, height;
    if (InnerImage()->HasError()) {
      // If there's a problem with the inner image we'll let it handle everything.
      mShouldClip.construct(false);
    } else if (NS_SUCCEEDED(InnerImage()->GetWidth(&width)) && width > 0 &&
               NS_SUCCEEDED(InnerImage()->GetHeight(&height)) && height > 0) {
      // Clamp the clipping region to the size of the underlying image.
      mClip = mClip.Intersect(nsIntRect(0, 0, width, height));

      // If the clipping region is the same size as the underlying image we
      // don't have to do anything.
      mShouldClip.construct(!mClip.IsEqualInterior(nsIntRect(0, 0, width, height)));
    } else if (InnerImage()->GetStatusTracker().IsLoading()) {
      // The image just hasn't finished loading yet. We don't yet know whether
      // clipping with be needed or not for now. Just return without memoizing
      // anything.
      return false;
    } else {
      // We have a fully loaded image without a clearly defined width and
      // height. This can happen with SVG images.
      mShouldClip.construct(false);
    }
  }

  MOZ_ASSERT(!mShouldClip.empty(), "Should have computed a result");
  return mShouldClip.ref();
}

NS_IMPL_ISUPPORTS1(ClippedImage, imgIContainer)

nsIntRect
ClippedImage::FrameRect(uint32_t aWhichFrame)
{
  if (!ShouldClip()) {
    return InnerImage()->FrameRect(aWhichFrame);
  }

  return nsIntRect(0, 0, mClip.width, mClip.height);
}

NS_IMETHODIMP
ClippedImage::GetWidth(int32_t* aWidth)
{
  if (!ShouldClip()) {
    return InnerImage()->GetWidth(aWidth);
  }

  *aWidth = mClip.width;
  return NS_OK;
}

NS_IMETHODIMP
ClippedImage::GetHeight(int32_t* aHeight)
{
  if (!ShouldClip()) {
    return InnerImage()->GetHeight(aHeight);
  }

  *aHeight = mClip.height;
  return NS_OK;
}

NS_IMETHODIMP
ClippedImage::GetIntrinsicSize(nsSize* aSize)
{
  if (!ShouldClip()) {
    return InnerImage()->GetIntrinsicSize(aSize);
  }

  *aSize = nsSize(mClip.width, mClip.height);
  return NS_OK;
}

NS_IMETHODIMP
ClippedImage::GetIntrinsicRatio(nsSize* aRatio)
{
  if (!ShouldClip()) {
    return InnerImage()->GetIntrinsicRatio(aRatio);
  }

  *aRatio = nsSize(mClip.width, mClip.height);
  return NS_OK;
}

NS_IMETHODIMP
ClippedImage::GetFrame(uint32_t aWhichFrame,
                       uint32_t aFlags,
                       gfxASurface** _retval)
{
  return GetFrameInternal(mClip.Size(), nullptr, aWhichFrame, aFlags, _retval);
}

nsresult
ClippedImage::GetFrameInternal(const nsIntSize& aViewportSize,
                               const SVGImageContext* aSVGContext,
                               uint32_t aWhichFrame,
                               uint32_t aFlags,
                               gfxASurface** _retval)
{
  if (!ShouldClip()) {
    return InnerImage()->GetFrame(aWhichFrame, aFlags, _retval);
  }

  float frameToDraw = InnerImage()->GetFrameIndex(aWhichFrame);
  if (!mCachedSurface || !mCachedSurface->Matches(aViewportSize,
                                                  aSVGContext,
                                                  frameToDraw,
                                                  aFlags)) {
    // Create a surface to draw into.
    mozilla::RefPtr<mozilla::gfx::DrawTarget> target;
    target = gfxPlatform::GetPlatform()->
      CreateOffscreenDrawTarget(gfx::IntSize(mClip.width, mClip.height),
                                gfx::FORMAT_B8G8R8A8);
    nsRefPtr<gfxASurface> surface = gfxPlatform::GetPlatform()->
      GetThebesSurfaceForDrawTarget(target);

    // Create our callback.
    nsRefPtr<gfxDrawingCallback> drawTileCallback =
      new DrawSingleTileCallback(this, mClip, aViewportSize, aSVGContext, aWhichFrame, aFlags);
    nsRefPtr<gfxDrawable> drawable =
      new gfxCallbackDrawable(drawTileCallback, mClip.Size());

    // Actually draw. The callback will end up invoking DrawSingleTile.
    nsRefPtr<gfxContext> ctx = new gfxContext(surface);
    gfxRect imageRect(0, 0, mClip.width, mClip.height);
    gfxUtils::DrawPixelSnapped(ctx, drawable, gfxMatrix(),
                               imageRect, imageRect, imageRect, imageRect,
                               gfxASurface::ImageFormatARGB32,
                               gfxPattern::FILTER_FAST);

    // Cache the resulting surface.
    mCachedSurface = new ClippedImageCachedSurface(target,
                                                   aViewportSize,
                                                   aSVGContext,
                                                   frameToDraw,
                                                   aFlags);
  }

  MOZ_ASSERT(mCachedSurface, "Should have a cached surface now");
  nsRefPtr<gfxASurface> surf = mCachedSurface->Surface();
  surf.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
ClippedImage::GetImageContainer(LayerManager* aManager, ImageContainer** _retval)
{
  // XXX(seth): We currently don't have a way of clipping the result of
  // GetImageContainer. We work around this by always returning null, but if it
  // ever turns out that ClippedImage is widely used on codepaths that can
  // actually benefit from GetImageContainer, it would be a good idea to fix
  // that method for performance reasons.

  if (!ShouldClip()) {
    return InnerImage()->GetImageContainer(aManager, _retval);
  }

  *_retval = nullptr;
  return NS_OK;
}

bool
ClippedImage::MustCreateSurface(gfxContext* aContext,
                                const gfxMatrix& aTransform,
                                const gfxRect& aSourceRect,
                                const nsIntRect& aSubimage,
                                const uint32_t aFlags) const
{
  gfxRect gfxImageRect(0, 0, mClip.width, mClip.height);
  nsIntRect intImageRect(0, 0, mClip.width, mClip.height);
  bool willTile = !gfxImageRect.Contains(aSourceRect) &&
                  !(aFlags & imgIContainer::FLAG_CLAMP);
  bool willResample = (aContext->CurrentMatrix().HasNonIntegerTranslation() ||
                       aTransform.HasNonIntegerTranslation()) &&
                      (willTile || !aSubimage.Contains(intImageRect));
  return willTile || willResample;
}

NS_IMETHODIMP
ClippedImage::Draw(gfxContext* aContext,
                   gfxPattern::GraphicsFilter aFilter,
                   const gfxMatrix& aUserSpaceToImageSpace,
                   const gfxRect& aFill,
                   const nsIntRect& aSubimage,
                   const nsIntSize& aViewportSize,
                   const SVGImageContext* aSVGContext,
                   uint32_t aWhichFrame,
                   uint32_t aFlags)
{
  if (!ShouldClip()) {
    return InnerImage()->Draw(aContext, aFilter, aUserSpaceToImageSpace,
                              aFill, aSubimage, aViewportSize, aSVGContext,
                              aWhichFrame, aFlags);
  }

  // Check for tiling. If we need to tile then we need to create a
  // gfxCallbackDrawable to handle drawing for us.
  gfxRect sourceRect = aUserSpaceToImageSpace.Transform(aFill);
  if (MustCreateSurface(aContext, aUserSpaceToImageSpace, sourceRect, aSubimage, aFlags)) {
    // Create a temporary surface containing a single tile of this image.
    // GetFrame will call DrawSingleTile internally.
    nsRefPtr<gfxASurface> surface;
    GetFrameInternal(aViewportSize, aSVGContext, aWhichFrame, aFlags, getter_AddRefs(surface));
    NS_ENSURE_TRUE(surface, NS_ERROR_FAILURE);

    // Create a drawable from that surface.
    nsRefPtr<gfxSurfaceDrawable> drawable =
      new gfxSurfaceDrawable(surface, gfxIntSize(mClip.width, mClip.height));

    // Draw.
    gfxRect imageRect(0, 0, mClip.width, mClip.height);
    gfxRect subimage(aSubimage.x, aSubimage.y, aSubimage.width, aSubimage.height);
    gfxUtils::DrawPixelSnapped(aContext, drawable, aUserSpaceToImageSpace,
                               subimage, sourceRect, imageRect, aFill,
                               gfxASurface::ImageFormatARGB32, aFilter);

    return NS_OK;
  }

  // Determine the appropriate subimage for the inner image.
  nsIntRect innerSubimage(aSubimage);
  innerSubimage.MoveBy(mClip.x, mClip.y);
  innerSubimage.Intersect(mClip);

  return DrawSingleTile(aContext, aFilter, aUserSpaceToImageSpace, aFill, innerSubimage,
                        aViewportSize, aSVGContext, aWhichFrame, aFlags);
}

gfxFloat
ClippedImage::ClampFactor(const gfxFloat aToClamp, const int aReference) const
{
  return aToClamp > aReference ? aReference / aToClamp
                               : 1.0;
}

nsresult
ClippedImage::DrawSingleTile(gfxContext* aContext,
                             gfxPattern::GraphicsFilter aFilter,
                             const gfxMatrix& aUserSpaceToImageSpace,
                             const gfxRect& aFill,
                             const nsIntRect& aSubimage,
                             const nsIntSize& aViewportSize,
                             const SVGImageContext* aSVGContext,
                             uint32_t aWhichFrame,
                             uint32_t aFlags)
{
  MOZ_ASSERT(!MustCreateSurface(aContext, aUserSpaceToImageSpace,
                                aUserSpaceToImageSpace.Transform(aFill),
                                aSubimage - nsIntPoint(mClip.x, mClip.y), aFlags),
             "DrawSingleTile shouldn't need to create a surface");

  // Make the viewport reflect the original image's size.
  nsIntSize viewportSize(aViewportSize);
  int32_t imgWidth, imgHeight;
  if (NS_SUCCEEDED(InnerImage()->GetWidth(&imgWidth)) &&
      NS_SUCCEEDED(InnerImage()->GetHeight(&imgHeight))) {
    viewportSize = nsIntSize(imgWidth, imgHeight);
  } else {
    MOZ_ASSERT(false, "If ShouldClip() led us to draw then we should never get here");
  }

  // Add a translation to the transform to reflect the clipping region.
  gfxMatrix transform(aUserSpaceToImageSpace);
  transform.Multiply(gfxMatrix().Translate(gfxPoint(mClip.x, mClip.y)));

  // "Clamp the source rectangle" to the clipping region's width and height.
  // Really, this means modifying the transform to get the results we want.
  gfxRect sourceRect = transform.Transform(aFill);
  if (sourceRect.width > mClip.width || sourceRect.height > mClip.height) {
    gfxMatrix clampSource;
    clampSource.Translate(gfxPoint(sourceRect.x, sourceRect.y));
    clampSource.Scale(ClampFactor(sourceRect.width, mClip.width),
                      ClampFactor(sourceRect.height, mClip.height));
    clampSource.Translate(gfxPoint(-sourceRect.x, -sourceRect.y));
    transform.Multiply(clampSource);
  }

  return InnerImage()->Draw(aContext, aFilter, transform, aFill, aSubimage,
                            viewportSize, aSVGContext, aWhichFrame, aFlags);
}

NS_IMETHODIMP
ClippedImage::RequestDiscard()
{
  // We're very aggressive about discarding.
  mCachedSurface = nullptr;

  return InnerImage()->RequestDiscard();
}

} // namespace image
} // namespace mozilla
