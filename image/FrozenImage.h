/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_FrozenImage_h
#define mozilla_image_FrozenImage_h

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
class FrozenImage : public ImageWrapper {
  typedef gfx::SourceSurface SourceSurface;

 public:
  NS_INLINE_DECL_REFCOUNTING_INHERITED(FrozenImage, ImageWrapper)

  virtual void IncrementAnimationConsumers() override;
  virtual void DecrementAnimationConsumers() override;

  bool IsNonAnimated() const;

  NS_IMETHOD GetAnimated(bool* aAnimated) override;
  NS_IMETHOD_(already_AddRefed<SourceSurface>)
  GetFrame(uint32_t aWhichFrame, uint32_t aFlags) override;
  NS_IMETHOD_(already_AddRefed<SourceSurface>)
  GetFrameAtSize(const gfx::IntSize& aSize, uint32_t aWhichFrame,
                 uint32_t aFlags) override;
  NS_IMETHOD_(bool)
  IsImageContainerAvailable(WindowRenderer* aRenderer,
                            uint32_t aFlags) override;
  NS_IMETHOD_(ImgDrawResult)
  GetImageProvider(WindowRenderer* aRenderer, const gfx::IntSize& aSize,
                   const Maybe<SVGImageContext>& aSVGContext,
                   const Maybe<ImageIntRegion>& aRegion, uint32_t aFlags,
                   WebRenderImageProvider** aProvider) override;
  NS_IMETHOD_(ImgDrawResult)
  Draw(gfxContext* aContext, const nsIntSize& aSize, const ImageRegion& aRegion,
       uint32_t aWhichFrame, gfx::SamplingFilter aSamplingFilter,
       const Maybe<SVGImageContext>& aSVGContext, uint32_t aFlags,
       float aOpacity) override;
  NS_IMETHOD StartDecoding(uint32_t aFlags, uint32_t aWhichFrame) override;
  NS_IMETHOD_(bool)
  StartDecodingWithResult(uint32_t aFlags, uint32_t aWhichFrame) override;
  NS_IMETHOD_(DecodeResult)
  RequestDecodeWithResult(uint32_t aFlags, uint32_t aWhichFrame) override;
  NS_IMETHOD RequestDecodeForSize(const nsIntSize& aSize, uint32_t aFlags,
                                  uint32_t aWhichFrame) override;
  NS_IMETHOD_(void) RequestRefresh(const TimeStamp& aTime) override;
  NS_IMETHOD GetAnimationMode(uint16_t* aAnimationMode) override;
  NS_IMETHOD SetAnimationMode(uint16_t aAnimationMode) override;
  NS_IMETHOD ResetAnimation() override;
  NS_IMETHOD_(float) GetFrameIndex(uint32_t aWhichFrame) override;

 protected:
  explicit FrozenImage(Image* aImage) : ImageWrapper(aImage) {}
  virtual ~FrozenImage() {}

 private:
  friend class ImageOps;
};

}  // namespace image
}  // namespace mozilla

#endif  // mozilla_image_FrozenImage_h
