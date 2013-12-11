/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ImageMetadata_h___
#define ImageMetadata_h___

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
    : mHotspotX(-1)
    , mHotspotY(-1)
    , mLoopCount(-1)
    , mIsNonPremultiplied(false)
  {}

  // Set the metadata this object represents on an image.
  void SetOnImage(RasterImage* image);

  void SetHotspot(uint16_t hotspotx, uint16_t hotspoty)
  {
    mHotspotX = hotspotx;
    mHotspotY = hotspoty;
  }
  void SetLoopCount(int32_t loopcount)
  {
    mLoopCount = loopcount;
  }

  void SetIsNonPremultiplied(bool nonPremult)
  {
    mIsNonPremultiplied = nonPremult;
  }

  void SetSize(int32_t width, int32_t height, Orientation orientation)
  {
    mSize.construct(nsIntSize(width, height));
    mOrientation.construct(orientation);
  }

  bool HasSize() const { return !mSize.empty(); }
  bool HasOrientation() const { return !mOrientation.empty(); }

  int32_t GetWidth() const { return mSize.ref().width; }
  int32_t GetHeight() const { return mSize.ref().height; }
  Orientation GetOrientation() const { return mOrientation.ref(); }

private:
  // The hotspot found on cursors, or -1 if none was found.
  int32_t mHotspotX;
  int32_t mHotspotY;

  // The loop count for animated images, or -1 for infinite loop.
  int32_t mLoopCount;

  Maybe<nsIntSize> mSize;
  Maybe<Orientation>  mOrientation;

  bool mIsNonPremultiplied;
};

} // namespace image
} // namespace mozilla

#endif // ImageMetadata_h___
