/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>

#include "gfx2DGlue.h"
#include "gfxDrawable.h"
#include "gfxPlatform.h"
#include "gfxUtils.h"
#include "ImageRegion.h"
#include "SVGImageContext.h"

#include "OrientedImage.h"

using std::swap;

namespace mozilla {

using namespace gfx;
using layers::LayerManager;
using layers::ImageContainer;

namespace image {

NS_IMPL_ISUPPORTS_INHERITED0(OrientedImage, ImageWrapper)

NS_IMETHODIMP
OrientedImage::GetWidth(int32_t* aWidth)
{
  if (mOrientation.SwapsWidthAndHeight()) {
    return InnerImage()->GetHeight(aWidth);
  } else {
    return InnerImage()->GetWidth(aWidth);
  }
}

NS_IMETHODIMP
OrientedImage::GetHeight(int32_t* aHeight)
{
  if (mOrientation.SwapsWidthAndHeight()) {
    return InnerImage()->GetWidth(aHeight);
  } else {
    return InnerImage()->GetHeight(aHeight);
  }
}

NS_IMETHODIMP
OrientedImage::GetIntrinsicSize(nsSize* aSize)
{
  nsresult rv = InnerImage()->GetIntrinsicSize(aSize);

  if (mOrientation.SwapsWidthAndHeight()) {
    swap(aSize->width, aSize->height);
  }

  return rv;
}

NS_IMETHODIMP
OrientedImage::GetIntrinsicRatio(nsSize* aRatio)
{
  nsresult rv = InnerImage()->GetIntrinsicRatio(aRatio);

  if (mOrientation.SwapsWidthAndHeight()) {
    swap(aRatio->width, aRatio->height);
  }

  return rv;
}

NS_IMETHODIMP_(already_AddRefed<SourceSurface>)
OrientedImage::GetFrame(uint32_t aWhichFrame,
                        uint32_t aFlags)
{
  nsresult rv;

  if (mOrientation.IsIdentity()) {
    return InnerImage()->GetFrame(aWhichFrame, aFlags);
  }

  // Get the underlying dimensions.
  IntSize size;
  rv = InnerImage()->GetWidth(&size.width);
  NS_ENSURE_SUCCESS(rv, nullptr);
  rv = InnerImage()->GetHeight(&size.height);
  NS_ENSURE_SUCCESS(rv, nullptr);

  // Determine an appropriate format for the surface.
  gfx::SurfaceFormat surfaceFormat;
  if (InnerImage()->IsOpaque()) {
    surfaceFormat = gfx::SurfaceFormat::B8G8R8X8;
  } else {
    surfaceFormat = gfx::SurfaceFormat::B8G8R8A8;
  }

  // Create a surface to draw into.
  RefPtr<DrawTarget> target =
    gfxPlatform::GetPlatform()->
      CreateOffscreenContentDrawTarget(size, surfaceFormat);
  if (!target || !target->IsValid()) {
    NS_ERROR("Could not create a DrawTarget");
    return nullptr;
  }


  // Create our drawable.
  RefPtr<SourceSurface> innerSurface =
    InnerImage()->GetFrame(aWhichFrame, aFlags);
  NS_ENSURE_TRUE(innerSurface, nullptr);
  RefPtr<gfxDrawable> drawable =
    new gfxSurfaceDrawable(innerSurface, size);

  // Draw.
  RefPtr<gfxContext> ctx = gfxContext::CreateOrNull(target);
  MOZ_ASSERT(ctx); // already checked the draw target above
  ctx->Multiply(OrientationMatrix(size));
  gfxUtils::DrawPixelSnapped(ctx, drawable, size, ImageRegion::Create(size),
                             surfaceFormat, SamplingFilter::LINEAR);

  return target->Snapshot();
}

NS_IMETHODIMP_(already_AddRefed<SourceSurface>)
OrientedImage::GetFrameAtSize(const IntSize& aSize,
                              uint32_t aWhichFrame,
                              uint32_t aFlags)
{
  // XXX(seth): It'd be nice to support downscale-during-decode for this case,
  // but right now we just fall back to the intrinsic size.
  return GetFrame(aWhichFrame, aFlags);
}

NS_IMETHODIMP_(bool)
OrientedImage::IsImageContainerAvailable(LayerManager* aManager, uint32_t aFlags)
{
  if (mOrientation.IsIdentity()) {
    return InnerImage()->IsImageContainerAvailable(aManager, aFlags);
  }
  return false;
}

NS_IMETHODIMP_(already_AddRefed<ImageContainer>)
OrientedImage::GetImageContainer(LayerManager* aManager, uint32_t aFlags)
{
  // XXX(seth): We currently don't have a way of orienting the result of
  // GetImageContainer. We work around this by always returning null, but if it
  // ever turns out that OrientedImage is widely used on codepaths that can
  // actually benefit from GetImageContainer, it would be a good idea to fix
  // that method for performance reasons.

  if (mOrientation.IsIdentity()) {
    return InnerImage()->GetImageContainer(aManager, aFlags);
  }

  return nullptr;
}

struct MatrixBuilder
{
  explicit MatrixBuilder(bool aInvert) : mInvert(aInvert) { }

  gfxMatrix Build() { return mMatrix; }

  void Scale(gfxFloat aX, gfxFloat aY)
  {
    if (mInvert) {
      mMatrix *= gfxMatrix::Scaling(1.0 / aX, 1.0 / aY);
    } else {
      mMatrix.Scale(aX, aY);
    }
  }

  void Rotate(gfxFloat aPhi)
  {
    if (mInvert) {
      mMatrix *= gfxMatrix::Rotation(-aPhi);
    } else {
      mMatrix.Rotate(aPhi);
    }
  }

  void Translate(gfxPoint aDelta)
  {
    if (mInvert) {
      mMatrix *= gfxMatrix::Translation(-aDelta);
    } else {
      mMatrix.Translate(aDelta);
    }
  }

private:
  gfxMatrix mMatrix;
  bool      mInvert;
};

/*
 * OrientationMatrix() computes a matrix that applies the rotation and
 * reflection specified by mOrientation, or that matrix's inverse if aInvert is
 * true.
 *
 * @param aSize The scaled size of the inner image. (When outside code specifies
 *              the scaled size, as with imgIContainer::Draw and its aSize
 *              parameter, it's necessary to swap the width and height if
 *              mOrientation.SwapsWidthAndHeight() is true.)
 * @param aInvert If true, compute the inverse of the orientation matrix. Prefer
 *                this approach to OrientationMatrix(..).Invert(), because it's
 *                more numerically accurate.
 */
gfxMatrix
OrientedImage::OrientationMatrix(const nsIntSize& aSize,
                                 bool aInvert /* = false */)
{
  MatrixBuilder builder(aInvert);

  // Apply reflection, if present. (This logically happens second, but we
  // apply it first because these transformations are all premultiplied.) A
  // translation is necessary to place the image back in the first quadrant.
  switch (mOrientation.flip) {
    case Flip::Unflipped:
      break;
    case Flip::Horizontal:
      if (mOrientation.SwapsWidthAndHeight()) {
        builder.Translate(gfxPoint(aSize.height, 0));
      } else {
        builder.Translate(gfxPoint(aSize.width, 0));
      }
      builder.Scale(-1.0, 1.0);
      break;
    default:
      MOZ_ASSERT(false, "Invalid flip value");
  }

  // Apply rotation, if present. Again, a translation is used to place the
  // image back in the first quadrant.
  switch (mOrientation.rotation) {
    case Angle::D0:
      break;
    case Angle::D90:
      builder.Translate(gfxPoint(aSize.height, 0));
      builder.Rotate(-1.5 * M_PI);
      break;
    case Angle::D180:
      builder.Translate(gfxPoint(aSize.width, aSize.height));
      builder.Rotate(-1.0 * M_PI);
      break;
    case Angle::D270:
      builder.Translate(gfxPoint(0, aSize.width));
      builder.Rotate(-0.5 * M_PI);
      break;
    default:
      MOZ_ASSERT(false, "Invalid rotation value");
  }

  return builder.Build();
}

static SVGImageContext
OrientViewport(const SVGImageContext& aOldContext,
               const Orientation& aOrientation)
{
  CSSIntSize viewportSize(aOldContext.GetViewportSize());
  if (aOrientation.SwapsWidthAndHeight()) {
    swap(viewportSize.width, viewportSize.height);
  }
  return SVGImageContext(viewportSize,
                         aOldContext.GetPreserveAspectRatio());
}

NS_IMETHODIMP_(DrawResult)
OrientedImage::Draw(gfxContext* aContext,
                    const nsIntSize& aSize,
                    const ImageRegion& aRegion,
                    uint32_t aWhichFrame,
                    SamplingFilter aSamplingFilter,
                    const Maybe<SVGImageContext>& aSVGContext,
                    uint32_t aFlags)
{
  if (mOrientation.IsIdentity()) {
    return InnerImage()->Draw(aContext, aSize, aRegion,
                              aWhichFrame, aSamplingFilter,
                              aSVGContext, aFlags);
  }

  // Update the image size to match the image's coordinate system. (This could
  // be done using TransformBounds but since it's only a size a swap is enough.)
  nsIntSize size(aSize);
  if (mOrientation.SwapsWidthAndHeight()) {
    swap(size.width, size.height);
  }

  // Update the matrix so that we transform the image into the orientation
  // expected by the caller before drawing.
  gfxMatrix matrix(OrientationMatrix(size));
  gfxContextMatrixAutoSaveRestore saveMatrix(aContext);
  aContext->Multiply(matrix);

  // The region is already in the orientation expected by the caller, but we
  // need it to be in the image's coordinate system, so we transform it using
  // the inverse of the orientation matrix.
  gfxMatrix inverseMatrix(OrientationMatrix(size, /* aInvert = */ true));
  ImageRegion region(aRegion);
  region.TransformBoundsBy(inverseMatrix);

  return InnerImage()->Draw(aContext, size, region, aWhichFrame, aSamplingFilter,
                            aSVGContext.map(OrientViewport, mOrientation),
                            aFlags);
}

nsIntSize
OrientedImage::OptimalImageSizeForDest(const gfxSize& aDest,
                                       uint32_t aWhichFrame,
                                       SamplingFilter aSamplingFilter,
                                       uint32_t aFlags)
{
  if (!mOrientation.SwapsWidthAndHeight()) {
    return InnerImage()->OptimalImageSizeForDest(aDest, aWhichFrame,
                                                 aSamplingFilter, aFlags);
  }

  // Swap the size for the calculation, then swap it back for the caller.
  gfxSize destSize(aDest.height, aDest.width);
  nsIntSize innerImageSize(InnerImage()->OptimalImageSizeForDest(destSize,
                                                                 aWhichFrame,
                                                                 aSamplingFilter,
                                                                 aFlags));
  return nsIntSize(innerImageSize.height, innerImageSize.width);
}

NS_IMETHODIMP_(nsIntRect)
OrientedImage::GetImageSpaceInvalidationRect(const nsIntRect& aRect)
{
  nsIntRect rect(InnerImage()->GetImageSpaceInvalidationRect(aRect));

  if (mOrientation.IsIdentity()) {
    return rect;
  }

  nsIntSize innerSize;
  nsresult rv = InnerImage()->GetWidth(&innerSize.width);
  rv = NS_FAILED(rv) ? rv : InnerImage()->GetHeight(&innerSize.height);
  if (NS_FAILED(rv)) {
    // Fall back to identity if the width and height aren't available.
    return rect;
  }

  // Transform the invalidation rect into the correct orientation.
  gfxMatrix matrix(OrientationMatrix(innerSize));
  gfxRect invalidRect(matrix.TransformBounds(gfxRect(rect.x, rect.y,
                                                     rect.width, rect.height)));
  invalidRect.RoundOut();

  return nsIntRect(invalidRect.x, invalidRect.y,
                   invalidRect.width, invalidRect.height);
}

} // namespace image
} // namespace mozilla
