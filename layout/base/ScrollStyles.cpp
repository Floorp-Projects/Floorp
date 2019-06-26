/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ScrollStyles.h"
#include "mozilla/WritingModes.h"
#include "nsStyleStruct.h"  // for nsStyleDisplay & nsStyleBackground::Position

namespace mozilla {

void ScrollStyles::InitializeScrollSnapStrictness(
    WritingMode aWritingMode, const nsStyleDisplay* aDisplay) {
  mScrollSnapStrictnessX = StyleScrollSnapStrictness::None;
  mScrollSnapStrictnessY = StyleScrollSnapStrictness::None;

  if (aDisplay->mScrollSnapType.strictness == StyleScrollSnapStrictness::None) {
    return;
  }

  switch (aDisplay->mScrollSnapType.axis) {
    case StyleScrollSnapAxis::X:
      mScrollSnapStrictnessX = aDisplay->mScrollSnapType.strictness;
      break;
    case StyleScrollSnapAxis::Y:
      mScrollSnapStrictnessY = aDisplay->mScrollSnapType.strictness;
      break;
    case StyleScrollSnapAxis::Block:
      if (aWritingMode.IsVertical()) {
        mScrollSnapStrictnessX = aDisplay->mScrollSnapType.strictness;
      } else {
        mScrollSnapStrictnessY = aDisplay->mScrollSnapType.strictness;
      }
      break;
    case StyleScrollSnapAxis::Inline:
      if (aWritingMode.IsVertical()) {
        mScrollSnapStrictnessY = aDisplay->mScrollSnapType.strictness;
      } else {
        mScrollSnapStrictnessX = aDisplay->mScrollSnapType.strictness;
      }
      break;
    case StyleScrollSnapAxis::Both:
      mScrollSnapStrictnessX = aDisplay->mScrollSnapType.strictness;
      mScrollSnapStrictnessY = aDisplay->mScrollSnapType.strictness;
      break;
  }
}

ScrollStyles::ScrollStyles(WritingMode aWritingMode, StyleOverflow aH,
                           StyleOverflow aV, const nsStyleDisplay* aDisplay)
    : mHorizontal(aH),
      mVertical(aV),
      mOverscrollBehaviorX(aDisplay->mOverscrollBehaviorX),
      mOverscrollBehaviorY(aDisplay->mOverscrollBehaviorY) {
  InitializeScrollSnapStrictness(aWritingMode, aDisplay);
}

ScrollStyles::ScrollStyles(WritingMode aWritingMode,
                           const nsStyleDisplay* aDisplay)
    : mHorizontal(aDisplay->mOverflowX),
      mVertical(aDisplay->mOverflowY),
      mOverscrollBehaviorX(aDisplay->mOverscrollBehaviorX),
      mOverscrollBehaviorY(aDisplay->mOverscrollBehaviorY) {
  InitializeScrollSnapStrictness(aWritingMode, aDisplay);
}

}  // namespace mozilla
