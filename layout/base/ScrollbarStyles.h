/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ScrollbarStyles_h
#define ScrollbarStyles_h

#include <stdint.h>
#include "nsStyleConsts.h"
#include "mozilla/dom/WindowBinding.h"

namespace mozilla {

struct ScrollbarStyles
{
  // Always one of NS_STYLE_OVERFLOW_SCROLL, NS_STYLE_OVERFLOW_HIDDEN,
  // or NS_STYLE_OVERFLOW_AUTO.
  uint8_t mHorizontal;
  uint8_t mVertical;
  // Always one of NS_STYLE_SCROLL_BEHAVIOR_AUTO or
  // NS_STYLE_SCROLL_BEHAVIOR_SMOOTH
  uint8_t mScrollBehavior;
  ScrollbarStyles(uint8_t aH, uint8_t aV, uint8_t aB) : mHorizontal(aH),
                                                        mVertical(aV),
                                                        mScrollBehavior(aB) {}
  ScrollbarStyles() {}
  bool operator==(const ScrollbarStyles& aStyles) const {
    return aStyles.mHorizontal == mHorizontal && aStyles.mVertical == mVertical &&
           aStyles.mScrollBehavior == mScrollBehavior;
  }
  bool operator!=(const ScrollbarStyles& aStyles) const {
    return aStyles.mHorizontal != mHorizontal || aStyles.mVertical != mVertical ||
           aStyles.mScrollBehavior != mScrollBehavior;
  }
  bool IsHiddenInBothDirections() const {
    return mHorizontal == NS_STYLE_OVERFLOW_HIDDEN &&
           mVertical == NS_STYLE_OVERFLOW_HIDDEN;
  }
  bool IsSmoothScroll(dom::ScrollBehavior aBehavior) const {
    return aBehavior == dom::ScrollBehavior::Smooth ||
             (aBehavior == dom::ScrollBehavior::Auto &&
               mScrollBehavior == NS_STYLE_SCROLL_BEHAVIOR_SMOOTH);
  }
};

}

#endif
