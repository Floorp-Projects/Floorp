/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImageComposite.h"

#include <inttypes.h>

#include "GeckoProfiler.h"
#include "gfxPlatform.h"

namespace mozilla {

using namespace gfx;

namespace layers {

/* static */ const float ImageComposite::BIAS_TIME_MS = 1.0f;

ImageComposite::ImageComposite() = default;

ImageComposite::~ImageComposite() = default;

TimeStamp ImageComposite::GetBiasedTime(const TimeStamp& aInput) const {
  switch (mBias) {
    case ImageComposite::BIAS_NEGATIVE:
      return aInput - TimeDuration::FromMilliseconds(BIAS_TIME_MS);
    case ImageComposite::BIAS_POSITIVE:
      return aInput + TimeDuration::FromMilliseconds(BIAS_TIME_MS);
    default:
      return aInput;
  }
}

void ImageComposite::UpdateBias(size_t aImageIndex) {
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
  if (!nextImageTime.IsNull() && nextImageTime - compositionTime < threshold &&
      nextImageTime - compositionTime > -threshold) {
    // The next frame's time is very close to our composition time (probably
    // just after the current composition time, but due to previously set
    // positive bias, it could be just before the current composition time too).
    // We're in a dangerous situation because jitter might cause frames to
    // fall one side or the other of the composition times, causing many frames
    // to be skipped or duplicated.
    // Specifically, the next composite is at risk of picking the "next + 1"
    // frame rather than the "next" frame, which would cause the "next" frame to
    // be skipped. Try to prevent that by adding a positive bias to the frame
    // times during the next composite; if the inter-frame time is almost
    // exactly equal to the inter-composition time, that should ensure that the
    // next + 1 frame falls just *after* the next composition time, and the next
    // composite should then pick the next frame rather than the next + 1 frame.
    mBias = ImageComposite::BIAS_POSITIVE;
    return;
  }
  mBias = ImageComposite::BIAS_NONE;
}

int ImageComposite::ChooseImageIndex() {
  // ChooseImageIndex is called for all images in the layer when it is visible.
  // Change to this behaviour would break dropped frames counting calculation:
  // We rely on this assumption to determine if during successive runs an
  // image is returned that isn't the one following immediately the previous one
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
    return 0;
  }

  uint32_t result = mLastChosenImageIndex;
  while (result + 1 < mImages.Length() &&
         GetBiasedTime(mImages[result + 1].mTimeStamp) <= now) {
    ++result;
  }
  if (result - mLastChosenImageIndex > 1) {
    // We're not returning the same image as the last call to ChooseImageIndex
    // or the immediately next one. We can assume that the frames not returned
    // have been dropped as they were too late to be displayed
    for (size_t idx = mLastChosenImageIndex; idx <= result; idx++) {
      if (IsImagesUpdateRateFasterThanCompositedRate(mImages[result],
                                                     mImages[idx])) {
        continue;
      }
      mDroppedFrames++;
      PROFILER_ADD_MARKER("Video frames dropped", GRAPHICS);
    }
  }
  mLastChosenImageIndex = result;
  return result;
}

const ImageComposite::TimedImage* ImageComposite::ChooseImage() {
  int index = ChooseImageIndex();
  return index >= 0 ? &mImages[index] : nullptr;
}

void ImageComposite::RemoveImagesWithTextureHost(TextureHost* aTexture) {
  for (int32_t i = mImages.Length() - 1; i >= 0; --i) {
    if (mImages[i].mTextureHost == aTexture) {
      aTexture->UnbindTextureSource();
      mImages.RemoveElementAt(i);
    }
  }
}

void ImageComposite::ClearImages() {
  mImages.Clear();
  mLastChosenImageIndex = 0;
}

uint32_t ImageComposite::ScanForLastFrameIndex(
    const nsTArray<TimedImage>& aNewImages) {
  if (mImages.IsEmpty()) {
    return 0;
  }
  uint32_t i = mLastChosenImageIndex;
  uint32_t newIndex = 0;
  uint32_t dropped = 0;
  // See if the new array of images have any images in common with the
  // previous list that we haven't played yet.
  uint32_t j = 0;
  while (i < mImages.Length() && j < aNewImages.Length()) {
    if (mImages[i].mProducerID != aNewImages[j].mProducerID) {
      // This is new content, can stop.
      newIndex = j;
      break;
    }
    int32_t oldFrameID = mImages[i].mFrameID;
    int32_t newFrameID = aNewImages[j].mFrameID;
    if (oldFrameID > newFrameID) {
      // This is an image we have already returned, we don't need to present
      // it again and can start from this index next time.
      newIndex = ++j;
      continue;
    }
    if (oldFrameID < mLastFrameID) {
      // we have already returned that frame previously, ignore.
      i++;
      continue;
    }
    if (oldFrameID < newFrameID) {
      // This is a new image, all images prior the new one and not yet
      // rendered can be considered as dropped. Those images have a FrameID
      // inferior to the new image.
      for (++i; i < mImages.Length() && mImages[i].mFrameID < newFrameID &&
                mImages[i].mProducerID == aNewImages[j].mProducerID;
           i++) {
        if (IsImagesUpdateRateFasterThanCompositedRate(aNewImages[j],
                                                       mImages[i])) {
          continue;
        }
        dropped++;
      }
      break;
    }
    i++;
    j++;
  }
  if (dropped > 0) {
    mDroppedFrames += dropped;
    PROFILER_ADD_MARKER("Video frames dropped", GRAPHICS);
  }
  if (newIndex >= aNewImages.Length()) {
    // Somehow none of those images should be rendered (can this happen?)
    // We will always return the last one for now.
    newIndex = aNewImages.Length() - 1;
  }
  return newIndex;
}

void ImageComposite::SetImages(nsTArray<TimedImage>&& aNewImages) {
  if (!aNewImages.IsEmpty()) {
    DetectTimeStampJitter(&aNewImages[0]);
  }
  mLastChosenImageIndex = ScanForLastFrameIndex(aNewImages);
  mImages = std::move(aNewImages);
}

void ImageComposite::UpdateCompositedFrame(int aImageIndex,
                                           const TimedImage* aImage,
                                           base::ProcessId aProcessId,
                                           const CompositableHandle& aHandle) {
  TimeStamp compositionTime = GetCompositionTime();
  MOZ_RELEASE_ASSERT(compositionTime,
                     "Should only be called during a composition");

#if MOZ_GECKO_PROFILER
  nsCString descr;
  if (profiler_can_accept_markers()) {
    nsCString relativeTimeString;
    if (aImage->mTimeStamp) {
      relativeTimeString.AppendPrintf(
          " [relative timestamp %.1lfms]",
          (aImage->mTimeStamp - compositionTime).ToMilliseconds());
    }
    static const char* kBiasStrings[] = {"NONE", "NEGATIVE", "POSITIVE"};
    descr.AppendPrintf("frameID %" PRId32 " (producerID %" PRId32
                       ") [bias %s]%s",
                       aImage->mFrameID, aImage->mProducerID,
                       kBiasStrings[mBias], relativeTimeString.get());
    if (mLastProducerID != aImage->mProducerID) {
      descr.AppendPrintf(", previous producerID: %" PRId32, mLastProducerID);
    } else if (mLastFrameID != aImage->mFrameID) {
      descr.AppendPrintf(", previous frameID: %" PRId32, mLastFrameID);
    } else {
      descr.AppendLiteral(", no change");
    }
  }
  AUTO_PROFILER_TEXT_MARKER_CAUSE("UpdateCompositedFrame", descr, GRAPHICS,
                                  Nothing(), nullptr);
#endif

  if (mLastFrameID == aImage->mFrameID &&
      mLastProducerID == aImage->mProducerID) {
    return;
  }

  mLastFrameID = aImage->mFrameID;
  mLastProducerID = aImage->mProducerID;

  if (aHandle) {
    ImageCompositeNotificationInfo info;
    info.mImageBridgeProcessId = aProcessId;
    info.mNotification = ImageCompositeNotification(
        aHandle, aImage->mTimeStamp, GetCompositionTime(), mLastFrameID,
        mLastProducerID);
    AppendImageCompositeNotification(info);
  }

  // Update mBias. This can change which frame ChooseImage(Index) would
  // return, and we don't want to do that until we've finished compositing
  // since callers of ChooseImage(Index) assume the same image will be chosen
  // during a given composition.
  UpdateBias(aImageIndex);
}

const ImageComposite::TimedImage* ImageComposite::GetImage(
    size_t aIndex) const {
  if (aIndex >= mImages.Length()) {
    return nullptr;
  }
  return &mImages[aIndex];
}

bool ImageComposite::IsImagesUpdateRateFasterThanCompositedRate(
    const TimedImage& aNewImage, const TimedImage& aOldImage) const {
  MOZ_ASSERT(aNewImage.mFrameID >= aOldImage.mFrameID);
  const uint32_t compositedRate = gfxPlatform::TargetFrameRate();
  if (compositedRate == 0) {
    return true;
  }
  const double compositedInterval = 1.0 / compositedRate;
  return aNewImage.mTimeStamp - aOldImage.mTimeStamp <
         TimeDuration::FromSeconds(compositedInterval);
}

void ImageComposite::DetectTimeStampJitter(const TimedImage* aNewImage) {
#if MOZ_GECKO_PROFILER
  if (!profiler_can_accept_markers()) {
    return;
  }

  // Find aNewImage in mImages and compute its timestamp delta, if found.
  // Ideally, a given video frame should never change its timestamp (jitter
  // should be zero). However, we re-adjust video frame timestamps based on the
  // audio clock. If the audio clock drifts compared to the system clock, or if
  // there are bugs or inaccuracies in the computation of these timestamps,
  // jitter will be non-zero.
  Maybe<TimeDuration> jitter;
  for (const auto& img : mImages) {
    if (img.mProducerID == aNewImage->mProducerID &&
        img.mFrameID == aNewImage->mFrameID) {
      jitter = Some(aNewImage->mTimeStamp - img.mTimeStamp);
      break;
    }
  }
  if (jitter) {
    TimeStamp now = TimeStamp::Now();
    nsPrintfCString text("%.2lfms", jitter->ToMilliseconds());
    profiler_add_text_marker("VideoFrameTimeStampJitter", text,
                             JS::ProfilingCategoryPair::GRAPHICS, now, now);
  }
#endif
}

}  // namespace layers
}  // namespace mozilla
