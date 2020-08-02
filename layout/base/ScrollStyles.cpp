/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ScrollStyles.h"
#include "nsStyleStruct.h"  // for nsStyleDisplay

namespace mozilla {

// https://drafts.csswg.org/css-overflow/#overflow-propagation
// "If `visible` is applied to the viewport, it must be interpreted as `auto`.
//  If `clip` is applied to the viewport, it must be interpreted as `hidden`."
static StyleOverflow MapOverflowValueForViewportPropagation(
    StyleOverflow aOverflow) {
  switch (aOverflow) {
    case StyleOverflow::Visible:
      return StyleOverflow::Auto;
    case StyleOverflow::Clip:
      return StyleOverflow::Hidden;
    default:
      return aOverflow;
  }
}

ScrollStyles::ScrollStyles(StyleOverflow aH, StyleOverflow aV)
    : mHorizontal(aH), mVertical(aV) {
  MOZ_ASSERT(mHorizontal == StyleOverflow::Auto ||
             mHorizontal == StyleOverflow::Hidden ||
             mHorizontal == StyleOverflow::Scroll);
  MOZ_ASSERT(mVertical == StyleOverflow::Auto ||
             mVertical == StyleOverflow::Hidden ||
             mVertical == StyleOverflow::Scroll);
}

ScrollStyles::ScrollStyles(const nsStyleDisplay& aDisplay,
                           MapOverflowToValidScrollStyleTag)
    : ScrollStyles(
          MapOverflowValueForViewportPropagation(aDisplay.mOverflowX),
          MapOverflowValueForViewportPropagation(aDisplay.mOverflowY)) {}

bool ScrollStyles::IsHiddenInBothDirections() const {
  return mHorizontal == StyleOverflow::Hidden &&
         mVertical == StyleOverflow::Hidden;
}

}  // namespace mozilla
