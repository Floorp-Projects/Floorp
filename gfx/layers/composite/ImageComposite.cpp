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
  , mDroppedFrames(0)
  , mLastChosenImageIndex(0)
{
}

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
ImageComposite::ChooseImageIndex()
{
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
    return -1;
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
    mDroppedFrames += result - mLastChosenImageIndex - 1;
  }
  mLastChosenImageIndex = result;
  return result;
}

const ImageComposite::TimedImage* ImageComposite::ChooseImage()
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
  mLastChosenImageIndex = 0;
}

uint32_t
ImageComposite::ScanForLastFrameIndex(const nsTArray<TimedImage>& aNewImages)
{
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
        dropped++;
      }
      break;
    }
    i++;
    j++;
  }
  if (dropped > 0) {
    mDroppedFrames += dropped;
  }
  if (newIndex >= aNewImages.Length()) {
    // Somehow none of those images should be rendered (can this happen?)
    // We will always return the last one for now.
    newIndex = aNewImages.Length() - 1;
  }
  return newIndex;
}

void
ImageComposite::SetImages(nsTArray<TimedImage>&& aNewImages)
{
  mLastChosenImageIndex = ScanForLastFrameIndex(aNewImages);
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
