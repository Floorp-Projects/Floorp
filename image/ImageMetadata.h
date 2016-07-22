/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_ImageMetadata_h
#define mozilla_image_ImageMetadata_h

#include <stdint.h>
#include "mozilla/Maybe.h"
#include "nsSize.h"
#include "Orientation.h"

namespace mozilla {
namespace image {

class RasterImage;

// The metadata about an image that decoders accumulate as they decode.
class ImageMetadata
{
public:
  ImageMetadata()
    : mLoopCount(-1)
    , mFirstFrameTimeout(FrameTimeout::Forever())
    , mHasAnimation(false)
  { }

  void SetHotspot(uint16_t aHotspotX, uint16_t aHotspotY)
  {
    mHotspot = Some(gfx::IntPoint(aHotspotX, aHotspotY));
  }
  gfx::IntPoint GetHotspot() const { return *mHotspot; }
  bool HasHotspot() const { return mHotspot.isSome(); }

  void SetLoopCount(int32_t loopcount)
  {
    mLoopCount = loopcount;
  }
  int32_t GetLoopCount() const { return mLoopCount; }

  void SetLoopLength(FrameTimeout aLength) { mLoopLength = Some(aLength); }
  FrameTimeout GetLoopLength() const { return *mLoopLength; }
  bool HasLoopLength() const { return mLoopLength.isSome(); }

  void SetFirstFrameTimeout(FrameTimeout aTimeout) { mFirstFrameTimeout = aTimeout; }
  FrameTimeout GetFirstFrameTimeout() const { return mFirstFrameTimeout; }

  void SetFirstFrameRefreshArea(const gfx::IntRect& aRefreshArea)
  {
    mFirstFrameRefreshArea = Some(aRefreshArea);
  }
  gfx::IntRect GetFirstFrameRefreshArea() const { return *mFirstFrameRefreshArea; }
  bool HasFirstFrameRefreshArea() const { return mFirstFrameRefreshArea.isSome(); }

  void SetSize(int32_t width, int32_t height, Orientation orientation)
  {
    if (!HasSize()) {
      mSize.emplace(nsIntSize(width, height));
      mOrientation.emplace(orientation);
    }
  }
  nsIntSize GetSize() const { return *mSize; }
  bool HasSize() const { return mSize.isSome(); }

  Orientation GetOrientation() const { return *mOrientation; }
  bool HasOrientation() const { return mOrientation.isSome(); }

  void SetHasAnimation() { mHasAnimation = true; }
  bool HasAnimation() const { return mHasAnimation; }

private:
  /// The hotspot found on cursors, if present.
  Maybe<gfx::IntPoint> mHotspot;

  /// The loop count for animated images, or -1 for infinite loop.
  int32_t mLoopCount;

  // The total length of a single loop through an animated image.
  Maybe<FrameTimeout> mLoopLength;

  /// The timeout of an animated image's first frame.
  FrameTimeout mFirstFrameTimeout;

  // The area of the image that needs to be invalidated when the animation
  // loops.
  Maybe<gfx::IntRect> mFirstFrameRefreshArea;

  Maybe<nsIntSize> mSize;
  Maybe<Orientation> mOrientation;

  bool mHasAnimation : 1;
};

} // namespace image
} // namespace mozilla

#endif // mozilla_image_ImageMetadata_h
