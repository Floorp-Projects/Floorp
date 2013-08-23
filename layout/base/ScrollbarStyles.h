/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ScrollbarStyles_h
#define ScrollbarStyles_h

#include <stdint.h>

namespace mozilla {

struct ScrollbarStyles
{
  // Always one of NS_STYLE_OVERFLOW_SCROLL, NS_STYLE_OVERFLOW_HIDDEN,
  // or NS_STYLE_OVERFLOW_AUTO.
  uint8_t mHorizontal;
  uint8_t mVertical;
  ScrollbarStyles(uint8_t h, uint8_t v) : mHorizontal(h), mVertical(v) {}
  ScrollbarStyles() {}
  bool operator==(const ScrollbarStyles& aStyles) const {
    return aStyles.mHorizontal == mHorizontal && aStyles.mVertical == mVertical;
  }
  bool operator!=(const ScrollbarStyles& aStyles) const {
    return aStyles.mHorizontal != mHorizontal || aStyles.mVertical != mVertical;
  }
};

}

#endif
