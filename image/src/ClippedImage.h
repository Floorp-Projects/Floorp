/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_IMAGELIB_CLIPPEDIMAGE_H_
#define MOZILLA_IMAGELIB_CLIPPEDIMAGE_H_

#include "ImageWrapper.h"
#include "mozilla/Maybe.h"

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
public:
  NS_DECL_ISUPPORTS

  virtual ~ClippedImage();

  virtual nsIntRect FrameRect(uint32_t aWhichFrame) MOZ_OVERRIDE;

  NS_IMETHOD GetWidth(int32_t* aWidth) MOZ_OVERRIDE;
  NS_IMETHOD GetHeight(int32_t* aHeight) MOZ_OVERRIDE;
  NS_IMETHOD GetIntrinsicSize(nsSize* aSize) MOZ_OVERRIDE;
  NS_IMETHOD GetIntrinsicRatio(nsSize* aRatio) MOZ_OVERRIDE;
  NS_IMETHOD_(already_AddRefed<gfxASurface>) GetFrame(uint32_t aWhichFrame,
                                                      uint32_t aFlags) MOZ_OVERRIDE;
  NS_IMETHOD GetImageContainer(mozilla::layers::LayerManager* aManager,
                               mozilla::layers::ImageContainer** _retval) MOZ_OVERRIDE;
  NS_IMETHOD Draw(gfxContext* aContext,
                  GraphicsFilter aFilter,
                  const gfxMatrix& aUserSpaceToImageSpace,
                  const gfxRect& aFill,
                  const nsIntRect& aSubimage,
                  const nsIntSize& aViewportSize,
                  const SVGImageContext* aSVGContext,
                  uint32_t aWhichFrame,
                  uint32_t aFlags) MOZ_OVERRIDE;
  NS_IMETHOD RequestDiscard() MOZ_OVERRIDE;
  NS_IMETHOD_(Orientation) GetOrientation() MOZ_OVERRIDE;

protected:
  ClippedImage(Image* aImage, nsIntRect aClip);

private:
  already_AddRefed<gfxASurface> GetFrameInternal(const nsIntSize& aViewportSize,
                                                 const SVGImageContext* aSVGContext,
                                                 uint32_t aWhichFrame,
                                                 uint32_t aFlags);
  bool ShouldClip();
  bool MustCreateSurface(gfxContext* aContext,
                         const gfxMatrix& aTransform,
                         const gfxRect& aSourceRect,
                         const nsIntRect& aSubimage,
                         const uint32_t aFlags) const;
  gfxFloat ClampFactor(const gfxFloat aToClamp, const int aReference) const;
  nsresult DrawSingleTile(gfxContext* aContext,
                          GraphicsFilter aFilter,
                          const gfxMatrix& aUserSpaceToImageSpace,
                          const gfxRect& aFill,
                          const nsIntRect& aSubimage,
                          const nsIntSize& aViewportSize,
                          const SVGImageContext* aSVGContext,
                          uint32_t aWhichFrame,
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

#endif // MOZILLA_IMAGELIB_CLIPPEDIMAGE_H_
