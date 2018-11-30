/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_FrameTimeout_h
#define mozilla_image_FrameTimeout_h

#include <stdint.h>
#include "mozilla/Assertions.h"

namespace mozilla {
namespace image {

/**
 * FrameTimeout wraps a frame timeout value (measured in milliseconds) after
 * first normalizing it. This normalization is necessary because some tools
 * generate incorrect frame timeout values which we nevertheless have to
 * support. For this reason, code that deals with frame timeouts should always
 * use a FrameTimeout value rather than the raw value from the image header.
 */
struct FrameTimeout {
  /**
   * @return a FrameTimeout of zero. This should be used only for math
   * involving FrameTimeout values. You can't obtain a zero FrameTimeout from
   * FromRawMilliseconds().
   */
  static FrameTimeout Zero() { return FrameTimeout(0); }

  /// @return an infinite FrameTimeout.
  static FrameTimeout Forever() { return FrameTimeout(-1); }

  /// @return a FrameTimeout obtained by normalizing a raw timeout value.
  static FrameTimeout FromRawMilliseconds(int32_t aRawMilliseconds) {
    // Normalize all infinite timeouts to the same value.
    if (aRawMilliseconds < 0) {
      return FrameTimeout::Forever();
    }

    // Very small timeout values are problematic for two reasons: we don't want
    // to burn energy redrawing animated images extremely fast, and broken tools
    // generate these values when they actually want a "default" value, so such
    // images won't play back right without normalization. For some context,
    // see bug 890743, bug 125137, bug 139677, and bug 207059. The historical
    // behavior of IE and Opera was:
    //   IE 6/Win:
    //     10 - 50ms is normalized to 100ms.
    //     >50ms is used unnormalized.
    //   Opera 7 final/Win:
    //     10ms is normalized to 100ms.
    //     >10ms is used unnormalized.
    if (aRawMilliseconds >= 0 && aRawMilliseconds <= 10) {
      return FrameTimeout(100);
    }

    // The provided timeout value is OK as-is.
    return FrameTimeout(aRawMilliseconds);
  }

  bool operator==(const FrameTimeout& aOther) const {
    return mTimeout == aOther.mTimeout;
  }

  bool operator!=(const FrameTimeout& aOther) const {
    return !(*this == aOther);
  }

  FrameTimeout operator+(const FrameTimeout& aOther) {
    if (*this == Forever() || aOther == Forever()) {
      return Forever();
    }

    return FrameTimeout(mTimeout + aOther.mTimeout);
  }

  FrameTimeout& operator+=(const FrameTimeout& aOther) {
    *this = *this + aOther;
    return *this;
  }

  /**
   * @return this FrameTimeout's value in milliseconds. Illegal to call on a
   * an infinite FrameTimeout value.
   */
  uint32_t AsMilliseconds() const {
    if (*this == Forever()) {
      MOZ_ASSERT_UNREACHABLE(
          "Calling AsMilliseconds() on an infinite FrameTimeout");
      return 100;  // Fail to something sane.
    }

    return uint32_t(mTimeout);
  }

  /**
   * @return this FrameTimeout value encoded so that non-negative values
   * represent a timeout in milliseconds, and -1 represents an infinite
   * timeout.
   *
   * XXX(seth): This is a backwards compatibility hack that should be removed.
   */
  int32_t AsEncodedValueDeprecated() const { return mTimeout; }

 private:
  explicit FrameTimeout(int32_t aTimeout) : mTimeout(aTimeout) {}

  int32_t mTimeout;
};

}  // namespace image
}  // namespace mozilla

#endif  // mozilla_image_FrameTimeout_h
