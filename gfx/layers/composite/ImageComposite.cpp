/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImageComposite.h"

namespace mozilla {

using namespace gfx;

namespace layers {

/* static */ const float ImageComposite::BIAS_TIME_MS = 1.0f;

ImageComposite::ImageComposite()
  : mLastFrameID(-1)
  , mLastProducerID(-1)
  , mBias(BIAS_NONE)
{}

ImageComposite::~ImageComposite()
{
}

TimeStamp
ImageComposite::GetBiasedTime(const TimeStamp& aInput) const
{
  switch (mBias) {
  case ImageComposite::BIAS_NEGATIVE:
    return aInput - TimeDuration::FromMilliseconds(BIAS_TIME_MS);
  case ImageComposite::BIAS_POSITIVE:
    return aInput + TimeDuration::FromMilliseconds(BIAS_TIME_MS);
  default:
    return aInput;
  }
}

void
ImageComposite::UpdateBias(size_t aImageIndex)
{
  MOZ_ASSERT(aImageIndex < ImagesCount());

  TimeStamp compositionTime = GetCompositionTime();
  TimeStamp compositedImageTime = mImages[aImageIndex].mTimeStamp;
  TimeStamp nextImageTime = aImageIndex + 1 < ImagesCount()
                              ? mImages[aImageIndex + 1].mTimeStamp
                              : TimeStamp();

  if (compositedImageTime.IsNull()) {
    mBias = ImageComposite::BIAS_NONE;
    return;
  }
  TimeDuration threshold = TimeDuration::FromMilliseconds(1.0);
  if (compositionTime - compositedImageTime < threshold &&
      compositionTime - compositedImageTime > -threshold) {
    // The chosen frame's time is very close to the composition time (probably
    // just before the current composition time, but due to previously set
    // negative bias, it could be just after the current composition time too).
    // If the inter-frame time is almost exactly equal to (a multiple of)
    // the inter-composition time, then we're in a dangerous situation because
    // jitter might cause frames to fall one side or the other of the
    // composition times, causing many frames to be skipped or duplicated.
    // Try to prevent that by adding a negative bias to the frame times during
    // the next composite; that should ensure the next frame's time is treated
    // as falling just before a composite time.
    mBias = ImageComposite::BIAS_NEGATIVE;
    return;
  }
  if (!nextImageTime.IsNull() &&
      nextImageTime - compositionTime < threshold &&
      nextImageTime - compositionTime > -threshold) {
    // The next frame's time is very close to our composition time (probably
    // just after the current composition time, but due to previously set
    // positive bias, it could be just before the current composition time too).
    // We're in a dangerous situation because jitter might cause frames to
    // fall one side or the other of the composition times, causing many frames
    // to be skipped or duplicated.
    // Try to prevent that by adding a negative bias to the frame times during
    // the next composite; that should ensure the next frame's time is treated
    // as falling just before a composite time.
    mBias = ImageComposite::BIAS_POSITIVE;
    return;
  }
  mBias = ImageComposite::BIAS_NONE;
}

int
ImageComposite::ChooseImageIndex() const
{
  if (mImages.IsEmpty()) {
    return -1;
  }
  TimeStamp now = GetCompositionTime();

  if (now.IsNull()) {
    // Not in a composition, so just return the last image we composited
    // (if it's one of the current images).
    for (uint32_t i = 0; i < mImages.Length(); ++i) {
      if (mImages[i].mFrameID == mLastFrameID &&
          mImages[i].mProducerID == mLastProducerID) {
        return i;
      }
    }
    return -1;
  }

  uint32_t result = 0;
  while (result + 1 < mImages.Length() &&
         GetBiasedTime(mImages[result + 1].mTimeStamp) <= now) {
    ++result;
  }
  return result;
}

const ImageComposite::TimedImage* ImageComposite::ChooseImage() const
{
  int index = ChooseImageIndex();
  return index >= 0 ? &mImages[index] : nullptr;
}

void
ImageComposite::RemoveImagesWithTextureHost(TextureHost* aTexture)
{
  for (int32_t i = mImages.Length() - 1; i >= 0; --i) {
    if (mImages[i].mTextureHost == aTexture) {
      aTexture->UnbindTextureSource();
      mImages.RemoveElementAt(i);
    }
  }
}

void
ImageComposite::ClearImages()
{
  mImages.Clear();
}

void
ImageComposite::SetImages(nsTArray<TimedImage>&& aNewImages)
{
  mImages = std::move(aNewImages);
}

const ImageComposite::TimedImage*
ImageComposite::GetImage(size_t aIndex) const
{
  if (aIndex >= mImages.Length()) {
    return nullptr;
  }
  return &mImages[aIndex];
}

} // namespace layers
} // namespace mozilla
