/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_OrientedImage_h
#define mozilla_image_OrientedImage_h

#include "ImageWrapper.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/RefPtr.h"
#include "Orientation.h"

namespace mozilla {
namespace image {

/**
 * An Image wrapper that rotates and/or flips an image according to a specified
 * Orientation.
 *
 * XXX(seth): There a known (performance, not correctness) issue with
 * GetImageContainer. See the comments for that method for more information.
 */
class OrientedImage : public ImageWrapper {
  typedef gfx::SourceSurface SourceSurface;

 public:
  NS_INLINE_DECL_REFCOUNTING_INHERITED(OrientedImage, ImageWrapper)

  NS_IMETHOD GetWidth(int32_t* aWidth) override;
  NS_IMETHOD GetHeight(int32_t* aHeight) override;
  nsresult GetNativeSizes(nsTArray<gfx::IntSize>& aNativeSizes) const override;
  NS_IMETHOD GetIntrinsicSize(nsSize* aSize) override;
  Maybe<AspectRatio> GetIntrinsicRatio() override;
  NS_IMETHOD_(already_AddRefed<SourceSurface>)
  GetFrame(uint32_t aWhichFrame, uint32_t aFlags) override;
  NS_IMETHOD_(already_AddRefed<SourceSurface>)
  GetFrameAtSize(const gfx::IntSize& aSize, uint32_t aWhichFrame,
                 uint32_t aFlags) override;
  NS_IMETHOD_(bool)
  IsImageContainerAvailable(layers::LayerManager* aManager,
                            uint32_t aFlags) override;
  NS_IMETHOD_(already_AddRefed<layers::ImageContainer>)
  GetImageContainer(layers::LayerManager* aManager, uint32_t aFlags) override;
  NS_IMETHOD_(bool)
  IsImageContainerAvailableAtSize(layers::LayerManager* aManager,
                                  const gfx::IntSize& aSize,
                                  uint32_t aFlags) override;
  NS_IMETHOD_(ImgDrawResult)
  GetImageContainerAtSize(layers::LayerManager* aManager,
                          const gfx::IntSize& aSize,
                          const Maybe<SVGImageContext>& aSVGContext,
                          uint32_t aFlags,
                          layers::ImageContainer** aOutContainer) override;
  NS_IMETHOD_(ImgDrawResult)
  Draw(gfxContext* aContext, const nsIntSize& aSize, const ImageRegion& aRegion,
       uint32_t aWhichFrame, gfx::SamplingFilter aSamplingFilter,
       const Maybe<SVGImageContext>& aSVGContext, uint32_t aFlags,
       float aOpacity) override;
  NS_IMETHOD_(nsIntRect)
  GetImageSpaceInvalidationRect(const nsIntRect& aRect) override;
  nsIntSize OptimalImageSizeForDest(const gfxSize& aDest, uint32_t aWhichFrame,
                                    gfx::SamplingFilter aSamplingFilter,
                                    uint32_t aFlags) override;

  /**
   * Computes a matrix that applies the rotation and reflection specified by
   * aOrientation, or that matrix's inverse if aInvert is true.
   *
   * @param aSize The scaled size of the inner image. (When outside code
   * specifies the scaled size, as with imgIContainer::Draw and its aSize
   * parameter, it's necessary to swap the width and height if
   * mOrientation.SwapsWidthAndHeight() is true.)
   *
   * @param aInvert If true, compute the inverse of the orientation matrix.
   * Prefer this approach to OrientationMatrix(..).Invert(), because it's more
   * numerically accurate.
   */
  static gfxMatrix OrientationMatrix(Orientation aOrientation,
                                     const nsIntSize& aSize,
                                     bool aInvert = false);

  /**
   * Returns a SourceSurface that is the result of rotating and flipping
   * aSurface according to aOrientation.
   */
  static already_AddRefed<SourceSurface> OrientSurface(Orientation aOrientation,
                                                       SourceSurface* aSurface);

 protected:
  OrientedImage(Image* aImage, Orientation aOrientation)
      : ImageWrapper(aImage), mOrientation(aOrientation) {}

  virtual ~OrientedImage() {}

  gfxMatrix OrientationMatrix(const nsIntSize& aSize, bool aInvert = false) {
    return OrientationMatrix(mOrientation, aSize, aInvert);
  }

 private:
  Orientation mOrientation;

  friend class ImageOps;
};

}  // namespace image
}  // namespace mozilla

#endif  // mozilla_image_OrientedImage_h
