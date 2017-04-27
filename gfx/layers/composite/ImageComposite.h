/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_IMAGECOMPOSITE_H
#define MOZILLA_GFX_IMAGECOMPOSITE_H

#include "CompositableHost.h"           // for CompositableTextureHostRef
#include "mozilla/gfx/2D.h"
#include "mozilla/TimeStamp.h"          // for TimeStamp
#include "nsTArray.h"

namespace mozilla {
namespace layers {

/**
 * Implements Image selection logic.
 */
class ImageComposite
{
public:
  static const float BIAS_TIME_MS;

  explicit ImageComposite();
  ~ImageComposite();

  int32_t GetFrameID()
  {
    const TimedImage* img = ChooseImage();
    return img ? img->mFrameID : -1;
  }

  int32_t GetProducerID()
  {
    const TimedImage* img = ChooseImage();
    return img ? img->mProducerID : -1;
  }

  int32_t GetLastFrameID() const { return mLastFrameID; }
  int32_t GetLastProducerID() const { return mLastProducerID; }

  enum Bias {
    // Don't apply bias to frame times
    BIAS_NONE,
    // Apply a negative bias to frame times to keep them before the vsync time
    BIAS_NEGATIVE,
    // Apply a positive bias to frame times to keep them after the vsync time
    BIAS_POSITIVE,
  };

  static TimeStamp GetBiasedTime(const TimeStamp& aInput, ImageComposite::Bias aBias);

protected:
  static Bias UpdateBias(const TimeStamp& aCompositionTime,
                         const TimeStamp& aCompositedImageTime,
                         const TimeStamp& aNextImageTime, // may be null
                         ImageComposite::Bias aBias);

  virtual TimeStamp GetCompositionTime() const = 0;

  struct TimedImage {
    CompositableTextureHostRef mTextureHost;
    TimeStamp mTimeStamp;
    gfx::IntRect mPictureRect;
    int32_t mFrameID;
    int32_t mProducerID;
  };

  /**
   * ChooseImage is guaranteed to return the same TimedImage every time it's
   * called during the same composition, up to the end of Composite() ---
   * it depends only on mImages, mCompositor->GetCompositionTime(), and mBias.
   * mBias is updated at the end of Composite().
   */
  const TimedImage* ChooseImage() const;
  TimedImage* ChooseImage();
  int ChooseImageIndex() const;

  nsTArray<TimedImage> mImages;
  int32_t mLastFrameID;
  int32_t mLastProducerID;
  /**
   * Bias to apply to the next frame.
   */
  Bias mBias;
};

} // namespace layers
} // namespace mozilla

#endif // MOZILLA_GFX_IMAGECOMPOSITE_H
