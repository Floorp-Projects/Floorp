/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_IMAGELIB_ORIENTEDIMAGE_H_
#define MOZILLA_IMAGELIB_ORIENTEDIMAGE_H_

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

  virtual nsIntRect FrameRect(uint32_t aWhichFrame) MOZ_OVERRIDE;

  NS_IMETHOD GetWidth(int32_t* aWidth) MOZ_OVERRIDE;
  NS_IMETHOD GetHeight(int32_t* aHeight) MOZ_OVERRIDE;
  NS_IMETHOD GetIntrinsicSize(nsSize* aSize) MOZ_OVERRIDE;
  NS_IMETHOD GetIntrinsicRatio(nsSize* aRatio) MOZ_OVERRIDE;
  NS_IMETHOD_(TemporaryRef<SourceSurface>)
    GetFrame(uint32_t aWhichFrame, uint32_t aFlags) MOZ_OVERRIDE;
  NS_IMETHOD GetImageContainer(layers::LayerManager* aManager,
                               layers::ImageContainer** _retval) MOZ_OVERRIDE;
  NS_IMETHOD Draw(gfxContext* aContext,
                  const nsIntSize& aSize,
                  const ImageRegion& aRegion,
                  uint32_t aWhichFrame,
                  GraphicsFilter aFilter,
                  const Maybe<SVGImageContext>& aSVGContext,
                  uint32_t aFlags) MOZ_OVERRIDE;
  NS_IMETHOD_(nsIntRect) GetImageSpaceInvalidationRect(const nsIntRect& aRect) MOZ_OVERRIDE;
  nsIntSize OptimalImageSizeForDest(const gfxSize& aDest,
                                    uint32_t aWhichFrame,
                                    GraphicsFilter aFilter,
                                    uint32_t aFlags) MOZ_OVERRIDE;
 
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

#endif // MOZILLA_IMAGELIB_ORIENTEDIMAGE_H_
