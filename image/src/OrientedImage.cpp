/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>

#include "gfxDrawable.h"
#include "gfxPlatform.h"
#include "gfxUtils.h"

#include "OrientedImage.h"

using std::swap;

namespace mozilla {

using namespace gfx;
using layers::LayerManager;
using layers::ImageContainer;

namespace image {

NS_IMPL_ISUPPORTS_INHERITED0(OrientedImage, ImageWrapper)

nsIntRect
OrientedImage::FrameRect(uint32_t aWhichFrame)
{
  if (mOrientation.SwapsWidthAndHeight()) {
    nsIntRect innerRect = InnerImage()->FrameRect(aWhichFrame);
    return nsIntRect(innerRect.x, innerRect.y, innerRect.height, innerRect.width);
  } else {
    return InnerImage()->FrameRect(aWhichFrame);
  }
}

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

NS_IMETHODIMP_(TemporaryRef<SourceSurface>)
OrientedImage::GetFrame(uint32_t aWhichFrame,
                        uint32_t aFlags)
{
  nsresult rv;

  if (mOrientation.IsIdentity()) {
    return InnerImage()->GetFrame(aWhichFrame, aFlags);
  }

  // Get the underlying dimensions.
  int32_t width, height;
  rv = InnerImage()->GetWidth(&width);
  NS_ENSURE_SUCCESS(rv, nullptr);
  rv = InnerImage()->GetHeight(&height);
  NS_ENSURE_SUCCESS(rv, nullptr);
  if (mOrientation.SwapsWidthAndHeight()) {
    swap(width, height);
  }
  NS_ENSURE_SUCCESS(rv, nullptr);

  // Determine an appropriate format for the surface.
  gfx::SurfaceFormat surfaceFormat;
  if (InnerImage()->FrameIsOpaque(aWhichFrame)) {
    surfaceFormat = gfx::SurfaceFormat::B8G8R8X8;
  } else {
    surfaceFormat = gfx::SurfaceFormat::B8G8R8A8;
  }

  // Create a surface to draw into.
  RefPtr<DrawTarget> target =
    gfxPlatform::GetPlatform()->
      CreateOffscreenContentDrawTarget(IntSize(width, height), surfaceFormat);
  if (!target) {
    NS_ERROR("Could not create a DrawTarget");
    return nullptr;
  }


  // Create our drawable.
  RefPtr<SourceSurface> innerSurface =
    InnerImage()->GetFrame(aWhichFrame, aFlags);
  NS_ENSURE_TRUE(innerSurface, nullptr);
  nsRefPtr<gfxDrawable> drawable =
    new gfxSurfaceDrawable(innerSurface, gfxIntSize(width, height));

  // Draw.
  nsRefPtr<gfxContext> ctx = new gfxContext(target);
  gfxRect imageRect(0, 0, width, height);
  gfxUtils::DrawPixelSnapped(ctx, drawable, OrientationMatrix(nsIntSize(width, height)),
                             imageRect, imageRect, imageRect, imageRect,
                             surfaceFormat, GraphicsFilter::FILTER_FAST);
  
  return target->Snapshot();
}

NS_IMETHODIMP
OrientedImage::GetImageContainer(LayerManager* aManager, ImageContainer** _retval)
{
  // XXX(seth): We currently don't have a way of orienting the result of
  // GetImageContainer. We work around this by always returning null, but if it
  // ever turns out that OrientedImage is widely used on codepaths that can
  // actually benefit from GetImageContainer, it would be a good idea to fix
  // that method for performance reasons.

  if (mOrientation.IsIdentity()) {
    return InnerImage()->GetImageContainer(aManager, _retval);
  }

  *_retval = nullptr;
  return NS_OK;
}

gfxMatrix
OrientedImage::OrientationMatrix(const nsIntSize& aViewportSize)
{
    gfxMatrix matrix;

    int32_t width, height;
    if (InnerImage()->GetType() == imgIContainer::TYPE_VECTOR) {
      width = mOrientation.SwapsWidthAndHeight() ? aViewportSize.height : aViewportSize.width;
      height = mOrientation.SwapsWidthAndHeight() ? aViewportSize.width : aViewportSize.height;
    } else {
      nsresult rv = InnerImage()->GetWidth(&width);
      rv = NS_FAILED(rv) ? rv : InnerImage()->GetHeight(&height);
      if (NS_FAILED(rv)) {
        // Fall back to identity if the width and height aren't available.
        return matrix;
      }
    }

    // Apply reflection, if present. (This logically happens second, but we
    // apply it first because these transformations are all premultiplied.) A
    // translation is necessary to place the image back in the first quadrant.
    switch (mOrientation.flip) {
      case Flip::Unflipped:
        break;
      case Flip::Horizontal:
        if (mOrientation.SwapsWidthAndHeight()) {
          // In the coordinate system after the rotation the reflection we want
          // is across the x-axis rather than the y-axis.
          matrix.Translate(gfxPoint(0, height));
          matrix.Scale(1.0, -1.0);
        } else {
          matrix.Translate(gfxPoint(width, 0));
          matrix.Scale(-1.0, 1.0);
        }
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
        matrix.Translate(gfxPoint(0, height));
        matrix.Rotate(-0.5 * M_PI);
        break;
      case Angle::D180:
        matrix.Translate(gfxPoint(width, height));
        matrix.Rotate(-1.0 * M_PI);
        break;
      case Angle::D270:
        matrix.Translate(gfxPoint(width, 0));
        matrix.Rotate(-1.5 * M_PI);
        break;
      default:
        MOZ_ASSERT(false, "Invalid rotation value");
    }

    return matrix;
}

NS_IMETHODIMP
OrientedImage::Draw(gfxContext* aContext,
                    GraphicsFilter aFilter,
                    const gfxMatrix& aUserSpaceToImageSpace,
                    const gfxRect& aFill,
                    const nsIntRect& aSubimage,
                    const nsIntSize& aViewportSize,
                    const SVGImageContext* aSVGContext,
                    uint32_t aWhichFrame,
                    uint32_t aFlags)
{
  if (mOrientation.IsIdentity()) {
    return InnerImage()->Draw(aContext, aFilter, aUserSpaceToImageSpace,
                              aFill, aSubimage, aViewportSize, aSVGContext,
                              aWhichFrame, aFlags);
  }

  // Update the matrix to reorient the image.
  gfxMatrix matrix(OrientationMatrix(aViewportSize));
  gfxMatrix userSpaceToImageSpace(aUserSpaceToImageSpace * matrix);

  // Update the subimage.
  gfxRect gfxSubimage(matrix.TransformBounds(gfxRect(aSubimage.x, aSubimage.y, aSubimage.width, aSubimage.height)));
  nsIntRect subimage(gfxSubimage.x, gfxSubimage.y, gfxSubimage.width, gfxSubimage.height);

  // Update the viewport size. (This could be done using TransformBounds but
  // since it's only a size a swap is enough.)
  nsIntSize viewportSize(aViewportSize);
  if (mOrientation.SwapsWidthAndHeight()) {
    swap(viewportSize.width, viewportSize.height);
  }

  return InnerImage()->Draw(aContext, aFilter, userSpaceToImageSpace,
                            aFill, subimage, viewportSize, aSVGContext,
                            aWhichFrame, aFlags);
}

NS_IMETHODIMP_(nsIntRect)
OrientedImage::GetImageSpaceInvalidationRect(const nsIntRect& aRect)
{
  nsIntRect rect(InnerImage()->GetImageSpaceInvalidationRect(aRect));

  if (mOrientation.IsIdentity()) {
    return rect;
  }

  int32_t width, height;
  nsresult rv = InnerImage()->GetWidth(&width);
  rv = NS_FAILED(rv) ? rv : InnerImage()->GetHeight(&height);
  if (NS_FAILED(rv)) {
    // Fall back to identity if the width and height aren't available.
    return rect;
  }

  // Transform the invalidation rect into the correct orientation.
  gfxMatrix matrix(OrientationMatrix(nsIntSize(width, height)));
  if (!matrix.Invert()) {
    return nsIntRect();
  }
  gfxRect invalidRect(matrix.TransformBounds(gfxRect(rect.x, rect.y,
                                                     rect.width, rect.height)));
  invalidRect.RoundOut();

  return nsIntRect(invalidRect.x, invalidRect.y,
                   invalidRect.width, invalidRect.height);
}

} // namespace image
} // namespace mozilla
