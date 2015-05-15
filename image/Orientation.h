/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_Orientation_h
#define mozilla_image_Orientation_h

#include <stdint.h>

namespace mozilla {
namespace image {

enum class Angle : uint8_t {
  D0,
  D90,
  D180,
  D270
};

enum class Flip : uint8_t {
  Unflipped,
  Horizontal
};

/**
 * A struct that describes an image's orientation as a rotation optionally
 * followed by a reflection. This may be used to be indicate an image's inherent
 * orientation or a desired orientation for the image.
 */
struct Orientation
{
  explicit Orientation(Angle aRotation = Angle::D0,
                       Flip mFlip = Flip::Unflipped)
    : rotation(aRotation)
    , flip(mFlip)
  { }

  bool IsIdentity() const {
    return (rotation == Angle::D0) && (flip == Flip::Unflipped);
  }

  bool SwapsWidthAndHeight() const {
    return (rotation == Angle::D90) || (rotation == Angle::D270);
  }

  bool operator==(const Orientation& aOther) const {
    return (rotation == aOther.rotation) && (flip == aOther.flip);
  }

  bool operator!=(const Orientation& aOther) const {
    return !(*this == aOther);
  }

  Angle rotation;
  Flip  flip;
};

} // namespace image
} // namespace mozilla

#endif // mozilla_image_Orientation_h
