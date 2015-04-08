/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_src_OrientedImage_h
#define mozilla_image_src_OrientedImage_h

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
class OrientedImage : public ImageWrapper
{
  typedef gfx::SourceSurface SourceSurface;

public:
  NS_DECL_ISUPPORTS_INHERITED

  NS_IMETHOD GetWidth(int32_t* aWidth) override;
  NS_IMETHOD GetHeight(int32_t* aHeight) override;
  NS_IMETHOD GetIntrinsicSize(nsSize* aSize) override;
  NS_IMETHOD GetIntrinsicRatio(nsSize* aRatio) override;
  NS_IMETHOD_(TemporaryRef<SourceSurface>)
    GetFrame(uint32_t aWhichFrame, uint32_t aFlags) override;
  NS_IMETHOD_(already_AddRefed<layers::ImageContainer>)
    GetImageContainer(layers::LayerManager* aManager,
                      uint32_t aFlags) override;
  NS_IMETHOD_(DrawResult) Draw(gfxContext* aContext,
                               const nsIntSize& aSize,
                               const ImageRegion& aRegion,
                               uint32_t aWhichFrame,
                               GraphicsFilter aFilter,
                               const Maybe<SVGImageContext>& aSVGContext,
                               uint32_t aFlags) override;
  NS_IMETHOD_(nsIntRect) GetImageSpaceInvalidationRect(
                                           const nsIntRect& aRect) override;
  nsIntSize OptimalImageSizeForDest(const gfxSize& aDest,
                                    uint32_t aWhichFrame,
                                    GraphicsFilter aFilter,
                                    uint32_t aFlags) override;

protected:
  OrientedImage(Image* aImage, Orientation aOrientation)
    : ImageWrapper(aImage)
    , mOrientation(aOrientation)
  { }

  virtual ~OrientedImage() { }

  gfxMatrix OrientationMatrix(const nsIntSize& aSize, bool aInvert = false);

private:
  Orientation mOrientation;

  friend class ImageOps;
};

} // namespace image
} // namespace mozilla

#endif // mozilla_image_src_OrientedImage_h
