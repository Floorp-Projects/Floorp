/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DashedCornerFinder.h"

#include <utility>

#include "BorderCache.h"
#include "BorderConsts.h"

namespace mozilla {

using namespace gfx;

struct BestDashLength {
  typedef mozilla::gfx::Float Float;

  Float dashLength;
  size_t count;

  BestDashLength() : dashLength(0.0f), count(0) {}

  BestDashLength(Float aDashLength, size_t aCount)
      : dashLength(aDashLength), count(aCount) {}
};

static const size_t DashedCornerCacheSize = 256;
nsTHashMap<FourFloatsHashKey, BestDashLength> DashedCornerCache;

DashedCornerFinder::DashedCornerFinder(const Bezier& aOuterBezier,
                                       const Bezier& aInnerBezier,
                                       Float aBorderWidthH, Float aBorderWidthV,
                                       const Size& aCornerDim)
    : mOuterBezier(aOuterBezier),
      mInnerBezier(aInnerBezier),
      mLastOuterP(aOuterBezier.mPoints[0]),
      mLastInnerP(aInnerBezier.mPoints[0]),
      mLastOuterT(0.0f),
      mLastInnerT(0.0f),
      mBestDashLength(DOT_LENGTH * DASH_LENGTH),
      mHasZeroBorderWidth(false),
      mHasMore(true),
      mMaxCount(aCornerDim.width + aCornerDim.height),
      mType(OTHER),
      mI(0),
      mCount(0) {
  NS_ASSERTION(aBorderWidthH > 0.0f || aBorderWidthV > 0.0f,
               "At least one side should have non-zero width.");

  DetermineType(aBorderWidthH, aBorderWidthV);

  Reset();
}

void DashedCornerFinder::DetermineType(Float aBorderWidthH,
                                       Float aBorderWidthV) {
  if (aBorderWidthH < aBorderWidthV) {
    // Always draw from wider side to thinner side.
    std::swap(mInnerBezier.mPoints[0], mInnerBezier.mPoints[3]);
    std::swap(mInnerBezier.mPoints[1], mInnerBezier.mPoints[2]);
    std::swap(mOuterBezier.mPoints[0], mOuterBezier.mPoints[3]);
    std::swap(mOuterBezier.mPoints[1], mOuterBezier.mPoints[2]);
    mLastOuterP = mOuterBezier.mPoints[0];
    mLastInnerP = mInnerBezier.mPoints[0];
  }

  // See the comment at mType declaration for each condition.

  Float borderRadiusA =
      fabs(mOuterBezier.mPoints[0].x - mOuterBezier.mPoints[3].x);
  Float borderRadiusB =
      fabs(mOuterBezier.mPoints[0].y - mOuterBezier.mPoints[3].y);
  if (aBorderWidthH == aBorderWidthV && borderRadiusA == borderRadiusB &&
      borderRadiusA > aBorderWidthH * 2.0f) {
    Float curveHeight = borderRadiusA - aBorderWidthH / 2.0;

    mType = PERFECT;
    Float borderLength = M_PI * curveHeight / 2.0f;

    Float dashWidth = aBorderWidthH * DOT_LENGTH * DASH_LENGTH;
    size_t count = ceil(borderLength / dashWidth);
    if (count % 2) {
      count++;
    }
    mCount = count / 2 + 1;
    mBestDashLength = borderLength / (aBorderWidthH * count);
  }

  Float minBorderWidth = std::min(aBorderWidthH, aBorderWidthV);
  if (minBorderWidth == 0.0f) {
    mHasZeroBorderWidth = true;
  }

  if (mType == OTHER && !mHasZeroBorderWidth) {
    Float minBorderRadius = std::min(borderRadiusA, borderRadiusB);
    Float maxBorderRadius = std::max(borderRadiusA, borderRadiusB);
    Float maxBorderWidth = std::max(aBorderWidthH, aBorderWidthV);

    FindBestDashLength(minBorderWidth, maxBorderWidth, minBorderRadius,
                       maxBorderRadius);
  }
}

bool DashedCornerFinder::HasMore(void) const {
  if (mHasZeroBorderWidth) {
    return mI < mMaxCount && mHasMore;
  }

  return mI < mCount;
}

DashedCornerFinder::Result DashedCornerFinder::Next(void) {
  Float lastOuterT, lastInnerT, outerT, innerT;

  if (mI == 0) {
    lastOuterT = 0.0f;
    lastInnerT = 0.0f;
  } else {
    if (mType == PERFECT) {
      lastOuterT = lastInnerT = (mI * 2.0f - 0.5f) / ((mCount - 1) * 2.0f);
    } else {
      Float last2OuterT = mLastOuterT;
      Float last2InnerT = mLastInnerT;

      (void)FindNext(mBestDashLength);

      //
      //          mLastOuterT   lastOuterT
      //                    |   |
      //                    v   v
      //            +---+---+---+---+ <- last2OuterT
      //            |   |###|###|   |
      //            |   |###|###|   |
      //            |   |###|###|   |
      //            +---+---+---+---+ <- last2InnerT
      //                    ^   ^
      //                    |   |
      //          mLastInnerT   lastInnerT
      lastOuterT = (mLastOuterT + last2OuterT) / 2.0f;
      lastInnerT = (mLastInnerT + last2InnerT) / 2.0f;
    }
  }

  if ((!mHasZeroBorderWidth && mI == mCount - 1) ||
      (mHasZeroBorderWidth && !mHasMore)) {
    outerT = 1.0f;
    innerT = 1.0f;
  } else {
    if (mType == PERFECT) {
      outerT = innerT = (mI * 2.0f + 0.5f) / ((mCount - 1) * 2.0f);
    } else {
      Float last2OuterT = mLastOuterT;
      Float last2InnerT = mLastInnerT;

      (void)FindNext(mBestDashLength);

      //
      //               outerT   last2OuterT
      //                    |   |
      //                    v   v
      // mLastOuterT -> +---+---+---+---+
      //                |   |###|###|   |
      //                |   |###|###|   |
      //                |   |###|###|   |
      // mLastInnerT -> +---+---+---+---+
      //                    ^   ^
      //                    |   |
      //               innerT   last2InnerT
      outerT = (mLastOuterT + last2OuterT) / 2.0f;
      innerT = (mLastInnerT + last2InnerT) / 2.0f;
    }
  }

  mI++;

  Bezier outerSectionBezier;
  Bezier innerSectionBezier;
  GetSubBezier(&outerSectionBezier, mOuterBezier, lastOuterT, outerT);
  GetSubBezier(&innerSectionBezier, mInnerBezier, lastInnerT, innerT);
  return DashedCornerFinder::Result(outerSectionBezier, innerSectionBezier);
}

void DashedCornerFinder::Reset(void) {
  mLastOuterP = mOuterBezier.mPoints[0];
  mLastInnerP = mInnerBezier.mPoints[0];
  mLastOuterT = 0.0f;
  mLastInnerT = 0.0f;
  mHasMore = true;
}

Float DashedCornerFinder::FindNext(Float dashLength) {
  Float upper = 1.0f;
  Float lower = mLastOuterT;

  Point OuterP, InnerP;
  // Start from upper bound to check if this is the last segment.
  Float outerT = upper;
  Float innerT;
  Float W = 0.0f;
  Float L = 0.0f;

  const Float LENGTH_MARGIN = 0.1f;
  for (size_t i = 0; i < MAX_LOOP; i++) {
    OuterP = GetBezierPoint(mOuterBezier, outerT);
    InnerP = FindBezierNearestPoint(mInnerBezier, OuterP, outerT, &innerT);

    // Calculate approximate dash length.
    //
    //   W = (W1 + W2) / 2
    //   L = (OuterL + InnerL) / 2
    //   dashLength = L / W
    //
    //              ____----+----____
    // OuterP ___---        |         ---___    mLastOuterP
    //    +---              |               ---+
    //    |                  |                 |
    //    |                  |                 |
    //     |               W |              W1 |
    //     |                  |                |
    //  W2 |                  |                |
    //     |                  |    ______------+
    //     |              ____+----             mLastInnerP
    //      |       ___---
    //      |  __---
    //      +--
    //    InnerP
    //                     OuterL
    //              ____---------____
    // OuterP ___---                  ---___    mLastOuterP
    //    +---                              ---+
    //    |                  L                 |
    //    |            ___----------______     |
    //     |      __---                   -----+
    //     |  __--                             |
    //     +--                                 |
    //     |                InnerL ______------+
    //     |              ____-----             mLastInnerP
    //      |       ___---
    //      |  __---
    //      +--
    //    InnerP
    Float W1 = (mLastOuterP - mLastInnerP).Length();
    Float W2 = (OuterP - InnerP).Length();
    Float OuterL = GetBezierLength(mOuterBezier, mLastOuterT, outerT);
    Float InnerL = GetBezierLength(mInnerBezier, mLastInnerT, innerT);
    W = (W1 + W2) / 2.0f;
    L = (OuterL + InnerL) / 2.0f;
    if (L > W * dashLength + LENGTH_MARGIN) {
      if (i > 0) {
        upper = outerT;
      }
    } else if (L < W * dashLength - LENGTH_MARGIN) {
      if (i == 0) {
        // This is the last segment with shorter dashLength.
        mHasMore = false;
        break;
      }
      lower = outerT;
    } else {
      break;
    }

    outerT = (upper + lower) / 2.0f;
  }

  mLastOuterP = OuterP;
  mLastInnerP = InnerP;
  mLastOuterT = outerT;
  mLastInnerT = innerT;

  if (W == 0.0f) {
    return 1.0f;
  }

  return L / W;
}

void DashedCornerFinder::FindBestDashLength(Float aMinBorderWidth,
                                            Float aMaxBorderWidth,
                                            Float aMinBorderRadius,
                                            Float aMaxBorderRadius) {
  // If dashLength is not calculateable, find it with binary search,
  // such that there exists i that OuterP_i == OuterP_n and
  // InnerP_i == InnerP_n with given dashLength.

  FourFloats key(aMinBorderWidth, aMaxBorderWidth, aMinBorderRadius,
                 aMaxBorderRadius);
  BestDashLength best;
  if (DashedCornerCache.Get(key, &best)) {
    mCount = best.count;
    mBestDashLength = best.dashLength;
    return;
  }

  Float lower = 1.0f;
  Float upper = DOT_LENGTH * DASH_LENGTH;
  Float dashLength = upper;
  size_t targetCount = 0;

  const Float LENGTH_MARGIN = 0.1f;
  for (size_t j = 0; j < MAX_LOOP; j++) {
    size_t count;
    Float actualDashLength;
    if (!GetCountAndLastDashLength(dashLength, &count, &actualDashLength)) {
      if (j == 0) {
        mCount = mMaxCount;
        break;
      }
    }

    if (j == 0) {
      if (count == 1) {
        // If only 1 segment fits, fill entire region
        //
        //   count = 1
        //   mCount = 1
        //   |   1   |
        //   +---+---+
        //   |###|###|
        //   |###|###|
        //   |###|###|
        //   +---+---+
        //       1
        mCount = 1;
        break;
      }

      // targetCount should be 2n.
      //
      //   targetCount = 2
      //   mCount = 2
      //   |   1   |   2   |
      //   +---+---+---+---+
      //   |###|   |   |###|
      //   |###|   |   |###|
      //   |###|   |   |###|
      //   +---+---+---+---+
      //     1           2
      //
      //   targetCount = 6
      //   mCount = 4
      //   |   1   |   2   |   3   |   4   |   5   |   6   |
      //   +---+---+---+---+---+---+---+---+---+---+---+---+
      //   |###|   |   |###|###|   |   |###|###|   |   |###|
      //   |###|   |   |###|###|   |   |###|###|   |   |###|
      //   |###|   |   |###|###|   |   |###|###|   |   |###|
      //   +---+---+---+---+---+---+---+---+---+---+---+---+
      //     1             2               3             4
      if (count % 2) {
        targetCount = count + 1;
      } else {
        targetCount = count;
      }

      mCount = targetCount / 2 + 1;
    }

    if (count == targetCount) {
      mBestDashLength = dashLength;

      // actualDashLength won't be greater than dashLength.
      if (actualDashLength > dashLength - LENGTH_MARGIN) {
        break;
      }

      // We started from upper bound, no need to update range when j == 0.
      if (j > 0) {
        upper = dashLength;
      }
    } else {
      // |j == 0 && count != targetCount| means that |targetCount = count + 1|,
      // and we started from upper bound, no need to update range when j == 0.
      if (j > 0) {
        if (count > targetCount) {
          lower = dashLength;
        } else {
          upper = dashLength;
        }
      }
    }

    dashLength = (upper + lower) / 2.0f;
  }

  if (DashedCornerCache.Count() > DashedCornerCacheSize) {
    DashedCornerCache.Clear();
  }
  DashedCornerCache.InsertOrUpdate(key,
                                   BestDashLength(mBestDashLength, mCount));
}

bool DashedCornerFinder::GetCountAndLastDashLength(Float aDashLength,
                                                   size_t* aCount,
                                                   Float* aActualDashLength) {
  // Return the number of segments and the last segment's dashLength for
  // the given dashLength.

  Reset();

  for (size_t i = 0; i < mMaxCount; i++) {
    Float actualDashLength = FindNext(aDashLength);
    if (mLastOuterT >= 1.0f) {
      *aCount = i + 1;
      *aActualDashLength = actualDashLength;
      return true;
    }
  }

  return false;
}

}  // namespace mozilla
