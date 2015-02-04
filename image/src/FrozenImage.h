/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_IMAGELIB_FROZENIMAGE_H_
#define MOZILLA_IMAGELIB_FROZENIMAGE_H_

#include "ImageWrapper.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/RefPtr.h"

namespace mozilla {
namespace image {

/**
 * An Image wrapper that disables animation, freezing the image at its first
 * frame. It does this using two strategies. If this is the only instance of the
 * image, animation will never start, because IncrementAnimationConsumers is
 * ignored. If there is another instance that is animated, that's still OK,
 * because any imgIContainer method that is affected by animation gets its
 * aWhichFrame argument set to FRAME_FIRST when it passes through FrozenImage.
 *
 * XXX(seth): There a known (performance, not correctness) issue with
 * GetImageContainer. See the comments for that method for more information.
 */
class FrozenImage : public ImageWrapper
{
  typedef gfx::SourceSurface SourceSurface;

public:
  NS_DECL_ISUPPORTS_INHERITED

  virtual void IncrementAnimationConsumers() MOZ_OVERRIDE;
  virtual void DecrementAnimationConsumers() MOZ_OVERRIDE;

  NS_IMETHOD GetAnimated(bool* aAnimated) MOZ_OVERRIDE;
  NS_IMETHOD_(TemporaryRef<SourceSurface>)
    GetFrame(uint32_t aWhichFrame, uint32_t aFlags) MOZ_OVERRIDE;
  NS_IMETHOD GetImageContainer(layers::LayerManager* aManager,
                               layers::ImageContainer** _retval) MOZ_OVERRIDE;
  NS_IMETHOD_(DrawResult) Draw(gfxContext* aContext,
                               const nsIntSize& aSize,
                               const ImageRegion& aRegion,
                               uint32_t aWhichFrame,
                               GraphicsFilter aFilter,
                               const Maybe<SVGImageContext>& aSVGContext,
                               uint32_t aFlags) MOZ_OVERRIDE;
  NS_IMETHOD_(void) RequestRefresh(const TimeStamp& aTime) MOZ_OVERRIDE;
  NS_IMETHOD GetAnimationMode(uint16_t* aAnimationMode) MOZ_OVERRIDE;
  NS_IMETHOD SetAnimationMode(uint16_t aAnimationMode) MOZ_OVERRIDE;
  NS_IMETHOD ResetAnimation() MOZ_OVERRIDE;
  NS_IMETHOD_(float) GetFrameIndex(uint32_t aWhichFrame) MOZ_OVERRIDE;

protected:
  explicit FrozenImage(Image* aImage) : ImageWrapper(aImage) { }
  virtual ~FrozenImage() { }

private:
  friend class ImageOps;
};

} // namespace image
} // namespace mozilla

#endif // MOZILLA_IMAGELIB_FROZENIMAGE_H_
