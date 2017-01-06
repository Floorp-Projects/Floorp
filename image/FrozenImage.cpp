/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FrozenImage.h"

namespace mozilla {

using namespace gfx;
using layers::ImageContainer;
using layers::LayerManager;

namespace image {

NS_IMPL_ISUPPORTS_INHERITED0(FrozenImage, ImageWrapper)

void
FrozenImage::IncrementAnimationConsumers()
{
  // Do nothing. This will prevent animation from starting if there are no other
  // instances of this image.
}

void
FrozenImage::DecrementAnimationConsumers()
{
  // Do nothing.
}

NS_IMETHODIMP
FrozenImage::GetAnimated(bool* aAnimated)
{
  bool dummy;
  nsresult rv = InnerImage()->GetAnimated(&dummy);
  if (NS_SUCCEEDED(rv)) {
    *aAnimated = false;
  }
  return rv;
}

NS_IMETHODIMP_(already_AddRefed<SourceSurface>)
FrozenImage::GetFrame(uint32_t aWhichFrame,
                      uint32_t aFlags)
{
  return InnerImage()->GetFrame(FRAME_FIRST, aFlags);
}

NS_IMETHODIMP_(already_AddRefed<SourceSurface>)
FrozenImage::GetFrameAtSize(const IntSize& aSize,
                            uint32_t aWhichFrame,
                            uint32_t aFlags)
{
  return InnerImage()->GetFrameAtSize(aSize, FRAME_FIRST, aFlags);
}

NS_IMETHODIMP_(bool)
FrozenImage::IsImageContainerAvailable(LayerManager* aManager, uint32_t aFlags)
{
  return false;
}

NS_IMETHODIMP_(already_AddRefed<ImageContainer>)
FrozenImage::GetImageContainer(layers::LayerManager* aManager, uint32_t aFlags)
{
  // XXX(seth): GetImageContainer does not currently support anything but the
  // current frame. We work around this by always returning null, but if it ever
  // turns out that FrozenImage is widely used on codepaths that can actually
  // benefit from GetImageContainer, it would be a good idea to fix that method
  // for performance reasons.
  return nullptr;
}

NS_IMETHODIMP_(DrawResult)
FrozenImage::Draw(gfxContext* aContext,
                  const nsIntSize& aSize,
                  const ImageRegion& aRegion,
                  uint32_t /* aWhichFrame - ignored */,
                  SamplingFilter aSamplingFilter,
                  const Maybe<SVGImageContext>& aSVGContext,
                  uint32_t aFlags,
                  float aOpacity)
{
  return InnerImage()->Draw(aContext, aSize, aRegion, FRAME_FIRST,
                            aSamplingFilter, aSVGContext, aFlags, aOpacity);
}

NS_IMETHODIMP_(void)
FrozenImage::RequestRefresh(const TimeStamp& aTime)
{
  // Do nothing.
}

NS_IMETHODIMP
FrozenImage::GetAnimationMode(uint16_t* aAnimationMode)
{
  *aAnimationMode = kNormalAnimMode;
  return NS_OK;
}

NS_IMETHODIMP
FrozenImage::SetAnimationMode(uint16_t aAnimationMode)
{
  // Do nothing.
  return NS_OK;
}

NS_IMETHODIMP
FrozenImage::ResetAnimation()
{
  // Do nothing.
  return NS_OK;
}

NS_IMETHODIMP_(float)
FrozenImage::GetFrameIndex(uint32_t aWhichFrame)
{
  MOZ_ASSERT(aWhichFrame <= FRAME_MAX_VALUE, "Invalid argument");
  return 0;
}

} // namespace image
} // namespace mozilla
