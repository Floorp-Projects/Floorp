/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImageComposite.h"

// this is also defined in ImageHost.cpp
#define BIAS_TIME_MS 1.0

namespace mozilla {

using namespace gfx;

namespace layers {

ImageComposite::ImageComposite()
  : mLastFrameID(-1)
  , mLastProducerID(-1)
  , mBias(BIAS_NONE)
{}

ImageComposite::~ImageComposite()
{
}

static TimeStamp
GetBiasedTime(const TimeStamp& aInput, ImageComposite::Bias aBias)
{
  switch (aBias) {
  case ImageComposite::BIAS_NEGATIVE:
    return aInput - TimeDuration::FromMilliseconds(BIAS_TIME_MS);
  case ImageComposite::BIAS_POSITIVE:
    return aInput + TimeDuration::FromMilliseconds(BIAS_TIME_MS);
  default:
    return aInput;
  }
}

/* static */ ImageComposite::Bias
ImageComposite::UpdateBias(const TimeStamp& aCompositionTime,
                           const TimeStamp& aCompositedImageTime,
                           const TimeStamp& aNextImageTime, // may be null
                           ImageComposite::Bias aBias)
{
  if (aCompositedImageTime.IsNull()) {
    return ImageComposite::BIAS_NONE;
  }
  TimeDuration threshold = TimeDuration::FromMilliseconds(1.0);
  if (aCompositionTime - aCompositedImageTime < threshold &&
      aCompositionTime - aCompositedImageTime > -threshold) {
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
    return ImageComposite::BIAS_NEGATIVE;
  }
  if (!aNextImageTime.IsNull() &&
      aNextImageTime - aCompositionTime < threshold &&
      aNextImageTime - aCompositionTime > -threshold) {
    // The next frame's time is very close to our composition time (probably
    // just after the current composition time, but due to previously set
    // positive bias, it could be just before the current composition time too).
    // We're in a dangerous situation because jitter might cause frames to
    // fall one side or the other of the composition times, causing many frames
    // to be skipped or duplicated.
    // Try to prevent that by adding a negative bias to the frame times during
    // the next composite; that should ensure the next frame's time is treated
    // as falling just before a composite time.
    return ImageComposite::BIAS_POSITIVE;
  }
  return ImageComposite::BIAS_NONE;
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
      GetBiasedTime(mImages[result + 1].mTimeStamp, mBias) <= now) {
    ++result;
  }
  return result;
}

const ImageComposite::TimedImage* ImageComposite::ChooseImage() const
{
  int index = ChooseImageIndex();
  return index >= 0 ? &mImages[index] : nullptr;
}

ImageComposite::TimedImage* ImageComposite::ChooseImage()
{
  int index = ChooseImageIndex();
  return index >= 0 ? &mImages[index] : nullptr;
}

} // namespace layers
} // namespace mozilla

#undef BIAS_TIME_MS
