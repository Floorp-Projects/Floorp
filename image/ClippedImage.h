/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_ClippedImage_h
#define mozilla_image_ClippedImage_h

#include "ImageWrapper.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/Maybe.h"
#include "mozilla/RefPtr.h"

namespace mozilla {
namespace image {

class ClippedImageCachedSurface;
class DrawSingleTileCallback;

/**
 * An Image wrapper that clips an image against a rectangle. Right now only
 * absolute coordinates in pixels are supported.
 *
 * XXX(seth): There a known (performance, not correctness) issue with
 * GetImageContainer. See the comments for that method for more information.
 */
class ClippedImage : public ImageWrapper
{
  typedef gfx::SourceSurface SourceSurface;

public:
  NS_DECL_ISUPPORTS_INHERITED

  NS_IMETHOD GetWidth(int32_t* aWidth) override;
  NS_IMETHOD GetHeight(int32_t* aHeight) override;
  NS_IMETHOD GetIntrinsicSize(nsSize* aSize) override;
  NS_IMETHOD GetIntrinsicRatio(nsSize* aRatio) override;
  NS_IMETHOD_(already_AddRefed<SourceSurface>)
    GetFrame(uint32_t aWhichFrame, uint32_t aFlags) override;
  NS_IMETHOD_(already_AddRefed<SourceSurface>)
    GetFrameAtSize(const gfx::IntSize& aSize,
                   uint32_t aWhichFrame,
                   uint32_t aFlags) override;
  NS_IMETHOD_(bool) IsImageContainerAvailable(layers::LayerManager* aManager,
                                              uint32_t aFlags) override;
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
  NS_IMETHOD RequestDiscard() override;
  NS_IMETHOD_(Orientation) GetOrientation() override;
  NS_IMETHOD_(nsIntRect) GetImageSpaceInvalidationRect(const nsIntRect& aRect)
    override;
  nsIntSize OptimalImageSizeForDest(const gfxSize& aDest,
                                    uint32_t aWhichFrame,
                                    GraphicsFilter aFilter,
                                    uint32_t aFlags) override;

protected:
  ClippedImage(Image* aImage, nsIntRect aClip);

  virtual ~ClippedImage();

private:
  already_AddRefed<SourceSurface>
    GetFrameInternal(const nsIntSize& aSize,
                     const Maybe<SVGImageContext>& aSVGContext,
                     uint32_t aWhichFrame,
                     uint32_t aFlags);
  bool ShouldClip();
  DrawResult DrawSingleTile(gfxContext* aContext,
                            const nsIntSize& aSize,
                            const ImageRegion& aRegion,
                            uint32_t aWhichFrame,
                            GraphicsFilter aFilter,
                            const Maybe<SVGImageContext>& aSVGContext,
                            uint32_t aFlags);

  // If we are forced to draw a temporary surface, we cache it here.
  nsAutoPtr<ClippedImageCachedSurface> mCachedSurface;

  nsIntRect   mClip;              // The region to clip to.
  Maybe<bool> mShouldClip;        // Memoized ShouldClip() if present.

  friend class DrawSingleTileCallback;
  friend class ImageOps;
};

} // namespace image
} // namespace mozilla

#endif // mozilla_image_ClippedImage_h
