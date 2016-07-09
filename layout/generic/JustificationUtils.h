/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_JustificationUtils_h_
#define mozilla_JustificationUtils_h_

#include "mozilla/Attributes.h"
#include "nsCoord.h"

namespace mozilla {

/**
 * Jutification Algorithm
 *
 * The justification algorithm is based on expansion opportunities
 * between justifiable clusters.  By this algorithm, there is one
 * expansion opportunity at each side of a justifiable cluster, and
 * at most one opportunity between two clusters. For example, if there
 * is a line in a Chinese document is: "你好世界hello world", then
 * the expansion opportunities (marked as '*') would be:
 *
 *                    你*好*世*界*hello*' '*world
 *
 * The spacing left in a line will then be distributed equally to each
 * opportunities. Because we want that, only justifiable clusters get
 * expanded, and the split point between two justifiable clusters would
 * be at the middle of the spacing, each expansion opportunities will be
 * filled by two justification gaps. The example above would be:
 *
 *              你 | 好 | 世 | 界  |hello|  ' '  |world
 *
 * In the algorithm, information about expansion opportunities is stored
 * in structure JustificationInfo, and the assignment of justification
 * gaps is in structure JustificationAssignment.
 */

struct JustificationInfo
{
  // Number of expansion opportunities inside a span. It doesn't include
  // any opportunities between this span and the one before or after.
  int32_t mInnerOpportunities;
  // The justifiability of the start and end sides of the span.
  bool mIsStartJustifiable;
  bool mIsEndJustifiable;

  constexpr JustificationInfo()
    : mInnerOpportunities(0)
    , mIsStartJustifiable(false)
    , mIsEndJustifiable(false)
  {
  }

  // Claim that the last opportunity should be cancelled
  // because the trailing space just gets trimmed.
  void CancelOpportunityForTrimmedSpace()
  {
    if (mInnerOpportunities > 0) {
      mInnerOpportunities--;
    } else {
      // There is no inner opportunities, hence the whole frame must
      // contain only the trimmed space, because any content before
      // space would cause an inner opportunity. The space made each
      // side justifiable, which should be cancelled now.
      mIsStartJustifiable = false;
      mIsEndJustifiable = false;
    }
  }
};

struct JustificationAssignment
{
  // There are at most 2 gaps per end, so it is enough to use 2 bits.
  uint8_t mGapsAtStart : 2;
  uint8_t mGapsAtEnd : 2;

  constexpr JustificationAssignment()
    : mGapsAtStart(0)
    , mGapsAtEnd(0)
  {
  }

  int32_t TotalGaps() const { return mGapsAtStart + mGapsAtEnd; }
};

struct JustificationApplicationState
{
  struct
  {
    // The total number of justification gaps to be processed.
    int32_t mCount;
    // The number of justification gaps which have been handled.
    int32_t mHandled;
  } mGaps;

  struct
  {
    // The total spacing left in a line before justification.
    nscoord mAvailable;
    // The spacing has been consumed by handled justification gaps.
    nscoord mConsumed;
  } mWidth;

  JustificationApplicationState(int32_t aGaps, nscoord aWidth)
  {
    mGaps.mCount = aGaps;
    mGaps.mHandled = 0;
    mWidth.mAvailable = aWidth;
    mWidth.mConsumed = 0;
  }

  bool IsJustifiable() const
  {
    return mGaps.mCount > 0 && mWidth.mAvailable > 0;
  }

  nscoord Consume(int32_t aGaps)
  {
    mGaps.mHandled += aGaps;
    nscoord newAllocate = (mWidth.mAvailable * mGaps.mHandled) / mGaps.mCount;
    nscoord deltaWidth = newAllocate - mWidth.mConsumed;
    mWidth.mConsumed = newAllocate;
    return deltaWidth;
  }
};

class JustificationUtils
{
public:
  // Compute justification gaps should be applied on a unit.
  static int32_t CountGaps(const JustificationInfo& aInfo,
                           const JustificationAssignment& aAssign)
  {
    // Justification gaps include two gaps for each inner opportunities
    // and the gaps given assigned to the ends.
    return aInfo.mInnerOpportunities * 2 + aAssign.TotalGaps();
  }
};

} // namespace mozilla

#endif /* !defined(mozilla_JustificationUtils_h_) */
