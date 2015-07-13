/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScrollbarStyles.h"
#include "nsStyleStruct.h" // for nsStyleDisplay and nsStyleBackground::Position

namespace mozilla {

  ScrollbarStyles::ScrollbarStyles(uint8_t aH, uint8_t aV,
                                   const nsStyleDisplay* aDisplay)
    : mHorizontal(aH), mVertical(aV),
      mScrollBehavior(aDisplay->mScrollBehavior),
      mScrollSnapTypeX(aDisplay->mScrollSnapTypeX),
      mScrollSnapTypeY(aDisplay->mScrollSnapTypeY),
      mScrollSnapPointsX(aDisplay->mScrollSnapPointsX),
      mScrollSnapPointsY(aDisplay->mScrollSnapPointsY),
      mScrollSnapDestinationX(aDisplay->mScrollSnapDestination.mXPosition),
      mScrollSnapDestinationY(aDisplay->mScrollSnapDestination.mYPosition) {}

  ScrollbarStyles::ScrollbarStyles(const nsStyleDisplay* aDisplay)
    : mHorizontal(aDisplay->mOverflowX), mVertical(aDisplay->mOverflowY),
      mScrollBehavior(aDisplay->mScrollBehavior),
      mScrollSnapTypeX(aDisplay->mScrollSnapTypeX),
      mScrollSnapTypeY(aDisplay->mScrollSnapTypeY),
      mScrollSnapPointsX(aDisplay->mScrollSnapPointsX),
      mScrollSnapPointsY(aDisplay->mScrollSnapPointsY),
      mScrollSnapDestinationX(aDisplay->mScrollSnapDestination.mXPosition),
      mScrollSnapDestinationY(aDisplay->mScrollSnapDestination.mYPosition) {}

} // namespace mozilla
