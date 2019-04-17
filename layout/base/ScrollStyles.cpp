/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ScrollStyles.h"
#include "mozilla/WritingModes.h"
#include "nsStyleStruct.h"  // for nsStyleDisplay & nsStyleBackground::Position

namespace mozilla {

void ScrollStyles::InitializeScrollSnapType(WritingMode aWritingMode,
                                            const nsStyleDisplay* aDisplay) {
  mScrollSnapTypeX = StyleScrollSnapStrictness::None;
  mScrollSnapTypeY = StyleScrollSnapStrictness::None;

  if (aDisplay->mScrollSnapType.strictness == StyleScrollSnapStrictness::None) {
    return;
  }

  switch (aDisplay->mScrollSnapType.axis) {
    case StyleScrollSnapAxis::X:
      mScrollSnapTypeX = aDisplay->mScrollSnapType.strictness;
      break;
    case StyleScrollSnapAxis::Y:
      mScrollSnapTypeY = aDisplay->mScrollSnapType.strictness;
      break;
    case StyleScrollSnapAxis::Block:
      if (aWritingMode.IsVertical()) {
        mScrollSnapTypeX = aDisplay->mScrollSnapType.strictness;
      } else {
        mScrollSnapTypeY = aDisplay->mScrollSnapType.strictness;
      }
      break;
    case StyleScrollSnapAxis::Inline:
      if (aWritingMode.IsVertical()) {
        mScrollSnapTypeY = aDisplay->mScrollSnapType.strictness;
      } else {
        mScrollSnapTypeX = aDisplay->mScrollSnapType.strictness;
      }
      break;
    case StyleScrollSnapAxis::Both:
      mScrollSnapTypeX = aDisplay->mScrollSnapType.strictness;
      mScrollSnapTypeY = aDisplay->mScrollSnapType.strictness;
      break;
  }
}

ScrollStyles::ScrollStyles(WritingMode aWritingMode, StyleOverflow aH,
                           StyleOverflow aV, const nsStyleDisplay* aDisplay)
    : mHorizontal(aH),
      mVertical(aV),
      mScrollBehavior(aDisplay->mScrollBehavior),
      mOverscrollBehaviorX(aDisplay->mOverscrollBehaviorX),
      mOverscrollBehaviorY(aDisplay->mOverscrollBehaviorY),
      mScrollSnapPointsX(aDisplay->mScrollSnapPointsX),
      mScrollSnapPointsY(aDisplay->mScrollSnapPointsY),
      mScrollSnapDestinationX(aDisplay->mScrollSnapDestination.horizontal),
      mScrollSnapDestinationY(aDisplay->mScrollSnapDestination.vertical) {
  InitializeScrollSnapType(aWritingMode, aDisplay);
}

ScrollStyles::ScrollStyles(WritingMode aWritingMode,
                           const nsStyleDisplay* aDisplay)
    : mHorizontal(aDisplay->mOverflowX),
      mVertical(aDisplay->mOverflowY),
      mScrollBehavior(aDisplay->mScrollBehavior),
      mOverscrollBehaviorX(aDisplay->mOverscrollBehaviorX),
      mOverscrollBehaviorY(aDisplay->mOverscrollBehaviorY),
      mScrollSnapPointsX(aDisplay->mScrollSnapPointsX),
      mScrollSnapPointsY(aDisplay->mScrollSnapPointsY),
      mScrollSnapDestinationX(aDisplay->mScrollSnapDestination.horizontal),
      mScrollSnapDestinationY(aDisplay->mScrollSnapDestination.vertical) {
  InitializeScrollSnapType(aWritingMode, aDisplay);
}

}  // namespace mozilla
