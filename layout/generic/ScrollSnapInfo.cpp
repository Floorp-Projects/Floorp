/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScrollSnapInfo.h"

#include "nsStyleStruct.h"
#include "mozilla/WritingModes.h"

namespace mozilla {

ScrollSnapInfo::ScrollSnapInfo()
    : mScrollSnapStrictnessX(StyleScrollSnapStrictness::None),
      mScrollSnapStrictnessY(StyleScrollSnapStrictness::None) {}

bool ScrollSnapInfo::HasScrollSnapping() const {
  return mScrollSnapStrictnessY != StyleScrollSnapStrictness::None ||
         mScrollSnapStrictnessX != StyleScrollSnapStrictness::None;
}

bool ScrollSnapInfo::HasSnapPositions() const {
  if (!HasScrollSnapping()) {
    return false;
  }

  for (const auto& target : mSnapTargets) {
    if ((target.mSnapPositionX &&
         mScrollSnapStrictnessX != StyleScrollSnapStrictness::None) ||
        (target.mSnapPositionY &&
         mScrollSnapStrictnessY != StyleScrollSnapStrictness::None)) {
      return true;
    }
  }
  return false;
}

void ScrollSnapInfo::InitializeScrollSnapStrictness(
    WritingMode aWritingMode, const nsStyleDisplay* aDisplay) {
  if (aDisplay->mScrollSnapType.strictness == StyleScrollSnapStrictness::None) {
    return;
  }

  mScrollSnapStrictnessX = StyleScrollSnapStrictness::None;
  mScrollSnapStrictnessY = StyleScrollSnapStrictness::None;

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

}  // namespace mozilla
