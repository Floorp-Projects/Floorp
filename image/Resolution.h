/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_Resolution_h
#define mozilla_image_Resolution_h

#include "mozilla/Assertions.h"
#include <cmath>

namespace mozilla {
namespace image {

/**
 * The resolution of an image, in dppx.
 */
struct Resolution {
  Resolution() = default;
  Resolution(float aX, float aY) : mX(aX), mY(aY) {
    MOZ_ASSERT(mX != 0.0f);
    MOZ_ASSERT(mY != 0.0f);
  }

  bool operator==(const Resolution& aOther) const {
    return mX == aOther.mX && mY == aOther.mY;
  }
  bool operator!=(const Resolution& aOther) const { return !(*this == aOther); }

  float mX = 1.0f;
  float mY = 1.0f;

  void ScaleBy(float aScale) {
    if (MOZ_LIKELY(aScale != 0.0f)) {
      mX *= aScale;
      mY *= aScale;
    }
  }

  void ApplyXTo(int32_t& aWidth) const {
    if (mX != 1.0f) {
      aWidth = std::round(float(aWidth) / mX);
    }
  }

  void ApplyXTo(float& aWidth) const {
    if (mX != 1.0f) {
      aWidth /= mX;
    }
  }

  void ApplyYTo(int32_t& aHeight) const {
    if (mY != 1.0f) {
      aHeight = std::round(float(aHeight) / mY);
    }
  }

  void ApplyYTo(float& aHeight) const {
    if (mY != 1.0f) {
      aHeight /= mY;
    }
  }

  void ApplyTo(int32_t& aWidth, int32_t& aHeight) const {
    ApplyXTo(aWidth);
    ApplyYTo(aHeight);
  }

  void ApplyTo(float& aWidth, float& aHeight) const {
    ApplyXTo(aWidth);
    ApplyYTo(aHeight);
  }

  void ApplyInverseTo(int32_t& aWidth, int32_t& aHeight) {
    if (mX != 1.0f) {
      aWidth = std::round(float(aWidth) * mX);
    }
    if (mY != 1.0f) {
      aHeight = std::round(float(aHeight) * mY);
    }
  }
};

}  // namespace image

using ImageResolution = image::Resolution;

}  // namespace mozilla

#endif
