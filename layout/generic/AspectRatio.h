/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_AspectRatio_h
#define mozilla_AspectRatio_h

/* The aspect ratio of a box, in a "width / height" format. */

#include "mozilla/Attributes.h"
#include "nsCoord.h"
#include <algorithm>
#include <limits>

namespace mozilla {

struct AspectRatio {
  AspectRatio() : mRatio(0.0f) {}
  explicit AspectRatio(float aRatio) : mRatio(std::max(aRatio, 0.0f)) {}

  static AspectRatio FromSize(float aWidth, float aHeight) {
    if (aWidth == 0.0f || aHeight == 0.0f) {
      return AspectRatio();
    }
    return AspectRatio(aWidth / aHeight);
  }

  explicit operator bool() const { return mRatio != 0.0f; }

  nscoord ApplyTo(nscoord aCoord) const {
    MOZ_DIAGNOSTIC_ASSERT(*this);
    return NSCoordSaturatingNonnegativeMultiply(aCoord, mRatio);
  }

  float ApplyToFloat(float aFloat) const {
    MOZ_DIAGNOSTIC_ASSERT(*this);
    return mRatio * aFloat;
  }

  // Inverts the ratio, in order to get the height / width ratio.
  [[nodiscard]] AspectRatio Inverted() const {
    if (!*this) {
      return AspectRatio();
    }
    // Clamp to a small epsilon, in case mRatio is absurdly large & produces
    // 0.0f in the division here (so that valid ratios always generate other
    // valid ratios when inverted).
    return AspectRatio(
        std::max(std::numeric_limits<float>::epsilon(), 1.0f / mRatio));
  }

  bool operator==(const AspectRatio& aOther) const {
    return mRatio == aOther.mRatio;
  }

  bool operator!=(const AspectRatio& aOther) const {
    return !(*this == aOther);
  }

  bool operator<(const AspectRatio& aOther) const {
    return mRatio < aOther.mRatio;
  }

 private:
  // 0.0f represents no aspect ratio.
  float mRatio;
};

}  // namespace mozilla

#endif  // mozilla_AspectRatio_h
