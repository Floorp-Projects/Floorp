/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_Orientation_h
#define mozilla_image_Orientation_h

#include <stdint.h>
#include "mozilla/gfx/Rect.h"

namespace mozilla {

// Pixel values in an image considering orientation metadata, such as the size
// of an image as seen by consumers of the image.
//
// Any public methods on RasterImage that use untyped units are interpreted as
// oriented pixels.
struct OrientedPixel {};
template <>
struct IsPixel<OrientedPixel> : std::true_type {};
typedef gfx::IntPointTyped<OrientedPixel> OrientedIntPoint;
typedef gfx::IntSizeTyped<OrientedPixel> OrientedIntSize;
typedef gfx::IntRectTyped<OrientedPixel> OrientedIntRect;

// Pixel values in an image ignoring orientation metadata, such as are stored
// in surfaces and the raw pixel data in the image.
struct UnorientedPixel {};
template <>
struct IsPixel<UnorientedPixel> : std::true_type {};
typedef gfx::IntPointTyped<UnorientedPixel> UnorientedIntPoint;
typedef gfx::IntSizeTyped<UnorientedPixel> UnorientedIntSize;
typedef gfx::IntRectTyped<UnorientedPixel> UnorientedIntRect;

namespace image {

enum class Angle : uint8_t { D0, D90, D180, D270 };

enum class Flip : uint8_t { Unflipped, Horizontal };

/**
 * A struct that describes an image's orientation as a rotation optionally
 * followed by a reflection. This may be used to be indicate an image's inherent
 * orientation or a desired orientation for the image.
 *
 * When flipFirst = true, this indicates that the reflection is applied before
 * the rotation. (This is used by OrientedImage to represent the inverse of an
 * underlying image's Orientation.)
 */
struct Orientation {
  explicit Orientation(Angle aRotation = Angle::D0,
                       Flip aFlip = Flip::Unflipped, bool aFlipFirst = false)
      : rotation(aRotation), flip(aFlip), flipFirst(aFlipFirst) {}

  Orientation Reversed() const {
    return Orientation(InvertAngle(rotation), flip, !flipFirst);
  }

  bool IsIdentity() const {
    return (rotation == Angle::D0) && (flip == Flip::Unflipped);
  }

  bool SwapsWidthAndHeight() const {
    return (rotation == Angle::D90) || (rotation == Angle::D270);
  }

  bool operator==(const Orientation& aOther) const {
    return rotation == aOther.rotation && flip == aOther.flip &&
           flipFirst == aOther.flipFirst;
  }

  bool operator!=(const Orientation& aOther) const {
    return !(*this == aOther);
  }

  OrientedIntSize ToOriented(const UnorientedIntSize& aSize) const {
    if (SwapsWidthAndHeight()) {
      return OrientedIntSize(aSize.height, aSize.width);
    } else {
      return OrientedIntSize(aSize.width, aSize.height);
    }
  }

  UnorientedIntSize ToUnoriented(const OrientedIntSize& aSize) const {
    if (SwapsWidthAndHeight()) {
      return UnorientedIntSize(aSize.height, aSize.width);
    } else {
      return UnorientedIntSize(aSize.width, aSize.height);
    }
  }

  static Angle InvertAngle(Angle aAngle) {
    switch (aAngle) {
      case Angle::D90:
        return Angle::D270;
      case Angle::D270:
        return Angle::D90;
      default:
        return aAngle;
    }
  }

  Angle rotation;
  Flip flip;
  bool flipFirst;
};

}  // namespace image
}  // namespace mozilla

#endif  // mozilla_image_Orientation_h
