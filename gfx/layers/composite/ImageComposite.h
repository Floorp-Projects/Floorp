/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_IMAGECOMPOSITE_H
#define MOZILLA_GFX_IMAGECOMPOSITE_H

#include "CompositableHost.h"  // for CompositableTextureHostRef
#include "mozilla/gfx/2D.h"
#include "mozilla/TimeStamp.h"  // for TimeStamp
#include "nsTArray.h"

namespace mozilla {
namespace layers {

/**
 * Implements Image selection logic.
 */
class ImageComposite {
 public:
  static const float BIAS_TIME_MS;

  explicit ImageComposite();
  virtual ~ImageComposite();

  int32_t GetFrameID() {
    const TimedImage* img = ChooseImage();
    return img ? img->mFrameID : -1;
  }

  int32_t GetProducerID() {
    const TimedImage* img = ChooseImage();
    return img ? img->mProducerID : -1;
  }

  int32_t GetLastFrameID() const { return mLastFrameID; }
  int32_t GetLastProducerID() const { return mLastProducerID; }
  uint32_t GetDroppedFramesAndReset() {
    uint32_t dropped = mDroppedFrames;
    mDroppedFrames = 0;
    return dropped;
  }

  enum Bias {
    // Don't apply bias to frame times
    BIAS_NONE,
    // Apply a negative bias to frame times to keep them before the vsync time
    BIAS_NEGATIVE,
    // Apply a positive bias to frame times to keep them after the vsync time
    BIAS_POSITIVE,
  };

 protected:
  void UpdateBias(size_t aImageIndex);

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
  const TimedImage* ChooseImage();
  int ChooseImageIndex();
  const TimedImage* GetImage(size_t aIndex) const;
  size_t ImagesCount() const { return mImages.Length(); }
  const nsTArray<TimedImage>& Images() const { return mImages; }

  void RemoveImagesWithTextureHost(TextureHost* aTexture);
  void ClearImages();
  void SetImages(nsTArray<TimedImage>&& aNewImages);

  int32_t mLastFrameID;
  int32_t mLastProducerID;

 private:
  nsTArray<TimedImage> mImages;
  TimeStamp GetBiasedTime(const TimeStamp& aInput) const;
  // Scan new images and look for common ones in the existing mImages array.
  // Will determine if an image has been dropped through gaps between images and
  // adjust mDroppedFrames accordingly.
  // Return the index of what the last returned image would have been.
  uint32_t ScanForLastFrameIndex(const nsTArray<TimedImage>& aNewImages);

  /**
   * Bias to apply to the next frame.
   */
  Bias mBias;
  uint32_t mDroppedFrames;
  uint32_t mLastChosenImageIndex;
};

}  // namespace layers
}  // namespace mozilla

#endif  // MOZILLA_GFX_IMAGECOMPOSITE_H
