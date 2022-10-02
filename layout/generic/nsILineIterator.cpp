/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsILineIterator.h"
#include "nsIFrame.h"

namespace mozilla {

void LineFrameFinder::Scan(nsIFrame* aFrame) {
  if (mDone) {
    return;
  }

  if (!mFirstFrame) {
    mFirstFrame = aFrame;
  }

  const LogicalRect rect = aFrame->GetLogicalRect(mWM, mContainerSize);
  if (rect.ISize(mWM) == 0) {
    return;
  }
  // If pos.I() is inside this frame - this is it
  if (rect.IStart(mWM) <= mPos.I(mWM) && rect.IEnd(mWM) > mPos.I(mWM)) {
    mClosestFromStart = mClosestFromEnd = aFrame;
    mDone = true;
    return;
  }
  if (rect.IStart(mWM) < mPos.I(mWM)) {
    if (!mClosestFromStart ||
        rect.IEnd(mWM) >
            mClosestFromStart->GetLogicalRect(mWM, mContainerSize).IEnd(mWM)) {
      mClosestFromStart = aFrame;
    }
  } else {
    if (!mClosestFromEnd ||
        rect.IStart(mWM) <
            mClosestFromEnd->GetLogicalRect(mWM, mContainerSize).IStart(mWM)) {
      mClosestFromEnd = aFrame;
    }
  }
}

void LineFrameFinder::Finish(nsIFrame** aFrameFound,
                             bool* aPosIsBeforeFirstFrame,
                             bool* aPosIsAfterLastFrame) {
  if (!mClosestFromStart && !mClosestFromEnd) {
    // All frames were zero-width. Just take the first one.
    mClosestFromStart = mClosestFromEnd = mFirstFrame;
  }
  *aPosIsBeforeFirstFrame = mIsReversed ? !mClosestFromEnd : !mClosestFromStart;
  *aPosIsAfterLastFrame = mIsReversed ? !mClosestFromStart : !mClosestFromEnd;
  if (mClosestFromStart == mClosestFromEnd) {
    *aFrameFound = mClosestFromStart;
  } else if (!mClosestFromStart) {
    *aFrameFound = mClosestFromEnd;
  } else if (!mClosestFromEnd) {
    *aFrameFound = mClosestFromStart;
  } else {  // we're between two frames
    nscoord delta =
        mClosestFromEnd->GetLogicalRect(mWM, mContainerSize).IStart(mWM) -
        mClosestFromStart->GetLogicalRect(mWM, mContainerSize).IEnd(mWM);
    if (mPos.I(mWM) <
        mClosestFromStart->GetLogicalRect(mWM, mContainerSize).IEnd(mWM) +
            delta / 2) {
      *aFrameFound = mClosestFromStart;
    } else {
      *aFrameFound = mClosestFromEnd;
    }
  }
}

}  // namespace mozilla
