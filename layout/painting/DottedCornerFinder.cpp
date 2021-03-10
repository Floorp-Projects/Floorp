/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DottedCornerFinder.h"

#include <utility>

#include "BorderCache.h"
#include "BorderConsts.h"

namespace mozilla {

using namespace gfx;

static inline Float Square(Float x) { return x * x; }

static Point PointRotateCCW90(const Point& aP) { return Point(aP.y, -aP.x); }

struct BestOverlap {
  Float overlap;
  size_t count;

  BestOverlap() : overlap(0.0f), count(0) {}

  BestOverlap(Float aOverlap, size_t aCount)
      : overlap(aOverlap), count(aCount) {}
};

static const size_t DottedCornerCacheSize = 256;
nsTHashMap<FourFloatsHashKey, BestOverlap> DottedCornerCache;

DottedCornerFinder::DottedCornerFinder(const Bezier& aOuterBezier,
                                       const Bezier& aInnerBezier,
                                       Corner aCorner, Float aBorderRadiusX,
                                       Float aBorderRadiusY, const Point& aC0,
                                       Float aR0, const Point& aCn, Float aRn,
                                       const Size& aCornerDim)
    : mOuterBezier(aOuterBezier),
      mInnerBezier(aInnerBezier),
      mCorner(aCorner),
      mNormalSign((aCorner == C_TL || aCorner == C_BR) ? -1.0f : 1.0f),
      mC0(aC0),
      mCn(aCn),
      mR0(aR0),
      mRn(aRn),
      mMaxR(std::max(aR0, aRn)),
      mCenterCurveOrigin(mC0.x, mCn.y),
      mCenterCurveR(0.0),
      mInnerCurveOrigin(mInnerBezier.mPoints[0].x, mInnerBezier.mPoints[3].y),
      mBestOverlap(0.0f),
      mHasZeroBorderWidth(false),
      mHasMore(true),
      mMaxCount(aCornerDim.width + aCornerDim.height),
      mType(OTHER),
      mI(0),
      mCount(0) {
  NS_ASSERTION(mR0 > 0.0f || mRn > 0.0f,
               "At least one side should have non-zero radius.");

  mInnerWidth = fabs(mInnerBezier.mPoints[0].x - mInnerBezier.mPoints[3].x);
  mInnerHeight = fabs(mInnerBezier.mPoints[0].y - mInnerBezier.mPoints[3].y);

  DetermineType(aBorderRadiusX, aBorderRadiusY);

  Reset();
}

static bool IsSingleCurve(Float aMinR, Float aMaxR, Float aMinBorderRadius,
                          Float aMaxBorderRadius) {
  return aMinR > 0.0f && aMinBorderRadius > aMaxR * 4.0f &&
         aMinBorderRadius / aMaxBorderRadius > 0.5f;
}

void DottedCornerFinder::DetermineType(Float aBorderRadiusX,
                                       Float aBorderRadiusY) {
  // Calculate parameters for the center curve before swap.
  Float centerCurveWidth = fabs(mC0.x - mCn.x);
  Float centerCurveHeight = fabs(mC0.y - mCn.y);
  Point cornerPoint(mCn.x, mC0.y);

  bool swapped = false;
  if (mR0 < mRn) {
    // Always draw from wider side to thinner side.
    std::swap(mC0, mCn);
    std::swap(mR0, mRn);
    std::swap(mInnerBezier.mPoints[0], mInnerBezier.mPoints[3]);
    std::swap(mInnerBezier.mPoints[1], mInnerBezier.mPoints[2]);
    std::swap(mOuterBezier.mPoints[0], mOuterBezier.mPoints[3]);
    std::swap(mOuterBezier.mPoints[1], mOuterBezier.mPoints[2]);
    mNormalSign = -mNormalSign;
    swapped = true;
  }

  // See the comment at mType declaration for each condition.

  Float minR = std::min(mR0, mRn);
  Float minBorderRadius = std::min(aBorderRadiusX, aBorderRadiusY);
  Float maxBorderRadius = std::max(aBorderRadiusX, aBorderRadiusY);
  if (IsSingleCurve(minR, mMaxR, minBorderRadius, maxBorderRadius)) {
    if (mR0 == mRn) {
      Float borderLength;
      if (minBorderRadius == maxBorderRadius) {
        mType = PERFECT;
        borderLength = M_PI * centerCurveHeight / 2.0f;

        mCenterCurveR = centerCurveWidth;
      } else {
        mType = SINGLE_CURVE_AND_RADIUS;
        borderLength =
            GetQuarterEllipticArcLength(centerCurveWidth, centerCurveHeight);
      }

      Float diameter = mR0 * 2.0f;
      size_t count = round(borderLength / diameter);
      if (count % 2) {
        count++;
      }
      mCount = count / 2 - 1;
      if (mCount > 0) {
        mBestOverlap = 1.0f - borderLength / (diameter * count);
      }
    } else {
      mType = SINGLE_CURVE;
    }
  }

  if (mType == SINGLE_CURVE_AND_RADIUS || mType == SINGLE_CURVE) {
    Size cornerSize(centerCurveWidth, centerCurveHeight);
    GetBezierPointsForCorner(&mCenterBezier, mCorner, cornerPoint, cornerSize);
    if (swapped) {
      std::swap(mCenterBezier.mPoints[0], mCenterBezier.mPoints[3]);
      std::swap(mCenterBezier.mPoints[1], mCenterBezier.mPoints[2]);
    }
  }

  if (minR == 0.0f) {
    mHasZeroBorderWidth = true;
  }

  if ((mType == SINGLE_CURVE || mType == OTHER) && !mHasZeroBorderWidth) {
    FindBestOverlap(minR, minBorderRadius, maxBorderRadius);
  }
}

bool DottedCornerFinder::HasMore(void) const {
  if (mHasZeroBorderWidth) {
    return mI < mMaxCount && mHasMore;
  }

  return mI < mCount;
}

DottedCornerFinder::Result DottedCornerFinder::Next(void) {
  mI++;

  if (mType == PERFECT) {
    Float phi = mI * 4.0f * mR0 * (1 - mBestOverlap) / mCenterCurveR;
    if (mCorner == C_TL) {
      phi = -M_PI / 2.0f - phi;
    } else if (mCorner == C_TR) {
      phi = -M_PI / 2.0f + phi;
    } else if (mCorner == C_BR) {
      phi = M_PI / 2.0f - phi;
    } else {
      phi = M_PI / 2.0f + phi;
    }

    Point C(mCenterCurveOrigin.x + mCenterCurveR * cos(phi),
            mCenterCurveOrigin.y + mCenterCurveR * sin(phi));
    return DottedCornerFinder::Result(C, mR0);
  }

  // Find unfilled and filled circles.
  (void)FindNext(mBestOverlap);
  if (mHasMore) {
    (void)FindNext(mBestOverlap);
  }

  return Result(mLastC, mLastR);
}

void DottedCornerFinder::Reset(void) {
  mLastC = mC0;
  mLastR = mR0;
  mLastT = 0.0f;
  mHasMore = true;
}

void DottedCornerFinder::FindPointAndRadius(Point& C, Float& r,
                                            const Point& innerTangent,
                                            const Point& normal, Float t) {
  // Find radius for the given tangent point on the inner curve such that the
  // circle is also tangent to the outer curve.

  NS_ASSERTION(mType == OTHER, "Wrong mType");

  Float lower = 0.0f;
  Float upper = mMaxR;
  const Float DIST_MARGIN = 0.1f;
  for (size_t i = 0; i < MAX_LOOP; i++) {
    r = (upper + lower) / 2.0f;
    C = innerTangent + normal * r;

    Point Near = FindBezierNearestPoint(mOuterBezier, C, t);
    Float distSquare = (C - Near).LengthSquare();

    if (distSquare > Square(r + DIST_MARGIN)) {
      lower = r;
    } else if (distSquare < Square(r - DIST_MARGIN)) {
      upper = r;
    } else {
      break;
    }
  }
}

Float DottedCornerFinder::FindNext(Float overlap) {
  Float lower = mLastT;
  Float upper = 1.0f;
  Float t;

  Point C = mLastC;
  Float r = 0.0f;

  Float factor = (1.0f - overlap);

  Float circlesDist = 0.0f;
  Float expectedDist = 0.0f;

  const Float DIST_MARGIN = 0.1f;
  if (mType == SINGLE_CURVE_AND_RADIUS) {
    r = mR0;

    expectedDist = (r + mLastR) * factor;

    // Find C_i on the center curve.
    for (size_t i = 0; i < MAX_LOOP; i++) {
      t = (upper + lower) / 2.0f;
      C = GetBezierPoint(mCenterBezier, t);

      // Check overlap along arc.
      circlesDist = GetBezierLength(mCenterBezier, mLastT, t);
      if (circlesDist < expectedDist - DIST_MARGIN) {
        lower = t;
      } else if (circlesDist > expectedDist + DIST_MARGIN) {
        upper = t;
      } else {
        break;
      }
    }
  } else if (mType == SINGLE_CURVE) {
    // Find C_i on the center curve, and calculate r_i.
    for (size_t i = 0; i < MAX_LOOP; i++) {
      t = (upper + lower) / 2.0f;
      C = GetBezierPoint(mCenterBezier, t);

      Point Diff = GetBezierDifferential(mCenterBezier, t);
      Float DiffLength = Diff.Length();
      if (DiffLength == 0.0f) {
        // Basically this shouldn't happen.
        // If differential is 0, we cannot calculate tangent circle,
        // skip this point.
        t = (t + upper) / 2.0f;
        continue;
      }

      Point normal = PointRotateCCW90(Diff / DiffLength) * (-mNormalSign);
      r = CalculateDistanceToEllipticArc(C, normal, mInnerCurveOrigin,
                                         mInnerWidth, mInnerHeight);

      // Check overlap along arc.
      circlesDist = GetBezierLength(mCenterBezier, mLastT, t);
      expectedDist = (r + mLastR) * factor;
      if (circlesDist < expectedDist - DIST_MARGIN) {
        lower = t;
      } else if (circlesDist > expectedDist + DIST_MARGIN) {
        upper = t;
      } else {
        break;
      }
    }
  } else {
    Float distSquareMax = Square(mMaxR * 3.0f);
    Float circlesDistSquare = 0.0f;

    // Find C_i and r_i.
    for (size_t i = 0; i < MAX_LOOP; i++) {
      t = (upper + lower) / 2.0f;
      Point innerTangent = GetBezierPoint(mInnerBezier, t);
      if ((innerTangent - mLastC).LengthSquare() > distSquareMax) {
        // It's clear that this tangent point is too far, skip it.
        upper = t;
        continue;
      }

      Point Diff = GetBezierDifferential(mInnerBezier, t);
      Float DiffLength = Diff.Length();
      if (DiffLength == 0.0f) {
        // Basically this shouldn't happen.
        // If differential is 0, we cannot calculate tangent circle,
        // skip this point.
        t = (t + upper) / 2.0f;
        continue;
      }

      Point normal = PointRotateCCW90(Diff / DiffLength) * mNormalSign;
      FindPointAndRadius(C, r, innerTangent, normal, t);

      // Check overlap with direct distance.
      circlesDistSquare = (C - mLastC).LengthSquare();
      expectedDist = (r + mLastR) * factor;
      if (circlesDistSquare < Square(expectedDist - DIST_MARGIN)) {
        lower = t;
      } else if (circlesDistSquare > Square(expectedDist + DIST_MARGIN)) {
        upper = t;
      } else {
        break;
      }
    }

    circlesDist = sqrt(circlesDistSquare);
  }

  if (mHasZeroBorderWidth) {
    // When calculating circle around r=0, it may result in wrong radius that
    // is bigger than previous circle.  Detect it and stop calculating.
    const Float R_MARGIN = 0.1f;
    if (mLastR < R_MARGIN && r > mLastR) {
      mHasMore = false;
      mLastR = 0.0f;
      return 0.0f;
    }
  }

  mLastT = t;
  mLastC = C;
  mLastR = r;

  if (mHasZeroBorderWidth) {
    const Float T_MARGIN = 0.001f;
    if (mLastT >= 1.0f - T_MARGIN ||
        (mLastC - mCn).LengthSquare() < Square(mLastR)) {
      mHasMore = false;
    }
  }

  if (expectedDist == 0.0f) {
    return 0.0f;
  }

  return 1.0f - circlesDist * factor / expectedDist;
}

void DottedCornerFinder::FindBestOverlap(Float aMinR, Float aMinBorderRadius,
                                         Float aMaxBorderRadius) {
  // If overlap is not calculateable, find it with binary search,
  // such that there exists i that C_i == C_n with the given overlap.

  FourFloats key(aMinR, mMaxR, aMinBorderRadius, aMaxBorderRadius);
  BestOverlap best;
  if (DottedCornerCache.Get(key, &best)) {
    mCount = best.count;
    mBestOverlap = best.overlap;
    return;
  }

  Float lower = 0.0f;
  Float upper = 0.5f;
  // Start from lower bound to find the minimum number of circles.
  Float overlap = 0.0f;
  mBestOverlap = overlap;
  size_t targetCount = 0;

  const Float OVERLAP_MARGIN = 0.1f;
  for (size_t j = 0; j < MAX_LOOP; j++) {
    Reset();

    size_t count;
    Float actualOverlap;
    if (!GetCountAndLastOverlap(overlap, &count, &actualOverlap)) {
      if (j == 0) {
        mCount = mMaxCount;
        break;
      }
    }

    if (j == 0) {
      if (count < 3 || (count == 3 && actualOverlap > 0.5f)) {
        // |count == 3 && actualOverlap > 0.5f| means there could be
        // a circle but it is too near from both ends.
        //
        // if actualOverlap == 0.0
        //               1       2       3
        //   +-------+-------+-------+-------+
        //   | ##### | ***** | ##### | ##### |
        //   |#######|*******|#######|#######|
        //   |###+###|***+***|###+###|###+###|
        //   |# C_0 #|* C_1 *|# C_2 #|# C_n #|
        //   | ##### | ***** | ##### | ##### |
        //   +-------+-------+-------+-------+
        //                   |
        //                   V
        //   +-------+---+-------+---+-------+
        //   | ##### |   | ##### |   | ##### |
        //   |#######|   |#######|   |#######|
        //   |###+###|   |###+###|   |###+###| Find the best overlap to place
        //   |# C_0 #|   |# C_1 #|   |# C_n #| C_1 at the middle of them
        //   | ##### |   | ##### |   | ##### |
        //   +-------+---+-------+---|-------+
        //
        // if actualOverlap == 0.5
        //               1       2     3
        //   +-------+-------+-------+---+
        //   | ##### | ***** | ##### |## |
        //   |#######|*******|##### C_n #|
        //   |###+###|***+***|###+###+###|
        //   |# C_0 #|* C_1 *|# C_2 #|###|
        //   | ##### | ***** | ##### |## |
        //   +-------+-------+-------+---+
        //                 |
        //                 V
        //   +-------+-+-------+-+-------+
        //   | ##### | | ##### | | ##### |
        //   |#######| |#######| |#######|
        //   |###+###| |###+###| |###+###| Even if we place C_1 at the middle
        //   |# C_0 #| |# C_1 #| |# C_n #| of them, it's too near from them
        //   | ##### | | ##### | | ##### |
        //   +-------+-+-------+-|-------+
        //                 |
        //                 V
        //   +-------+-----------+-------+
        //   | ##### |           | ##### |
        //   |#######|           |#######|
        //   |###+###|           |###+###| Do not draw any circle
        //   |# C_0 #|           |# C_n #|
        //   | ##### |           | ##### |
        //   +-------+-----------+-------+
        mCount = 0;
        break;
      }

      // targetCount should be 2n, as we're searching C_1 to C_n.
      //
      //   targetCount = 4
      //   mCount = 1
      //               1       2       3       4
      //   +-------+-------+-------+-------+-------+
      //   | ##### | ***** | ##### | ***** | ##### |
      //   |#######|*******|#######|*******|#######|
      //   |###+###|***+***|###+###|***+***|###+###|
      //   |# C_0 #|* C_1 *|# C_2 #|* C_3 *|# C_n #|
      //   | ##### | ***** | ##### | ***** | ##### |
      //   +-------+-------+-------+-------+-------+
      //                       1
      //
      //   targetCount = 6
      //   mCount = 2
      //               1       2       3       4       5       6
      //   +-------+-------+-------+-------+-------+-------+-------+
      //   | ##### | ***** | ##### | ***** | ##### | ***** | ##### |
      //   |#######|*******|#######|*******|#######|*******|#######|
      //   |###+###|***+***|###+###|***+***|###+###|***+***|###+###|
      //   |# C_0 #|* C_1 *|# C_2 #|* C_3 *|# C_4 #|* C_5 *|# C_n #|
      //   | ##### | ***** | ##### | ***** | ##### | ***** | ##### |
      //   +-------+-------+-------+-------+-------+-------+-------+
      //                       1               2
      if (count % 2) {
        targetCount = count + 1;
      } else {
        targetCount = count;
      }

      mCount = targetCount / 2 - 1;
    }

    if (count == targetCount) {
      mBestOverlap = overlap;

      if (fabs(actualOverlap - overlap) < OVERLAP_MARGIN) {
        break;
      }

      // We started from upper bound, no need to update range when j == 0.
      if (j > 0) {
        if (actualOverlap > overlap) {
          lower = overlap;
        } else {
          upper = overlap;
        }
      }
    } else {
      // |j == 0 && count != targetCount| means that |targetCount = count + 1|,
      // and we started from upper bound, no need to update range when j == 0.
      if (j > 0) {
        if (count > targetCount) {
          upper = overlap;
        } else {
          lower = overlap;
        }
      }
    }

    overlap = (upper + lower) / 2.0f;
  }

  if (DottedCornerCache.Count() > DottedCornerCacheSize) {
    DottedCornerCache.Clear();
  }
  DottedCornerCache.InsertOrUpdate(key, BestOverlap(mBestOverlap, mCount));
}

bool DottedCornerFinder::GetCountAndLastOverlap(Float aOverlap, size_t* aCount,
                                                Float* aActualOverlap) {
  // Return the number of circles and the last circles' overlap for the
  // given overlap.

  Reset();

  const Float T_MARGIN = 0.001f;
  const Float DIST_MARGIN = 0.1f;
  const Float DIST_MARGIN_SQUARE = Square(DIST_MARGIN);
  for (size_t i = 0; i < mMaxCount; i++) {
    Float actualOverlap = FindNext(aOverlap);
    if (mLastT >= 1.0f - T_MARGIN ||
        (mLastC - mCn).LengthSquare() < DIST_MARGIN_SQUARE) {
      *aCount = i + 1;
      *aActualOverlap = actualOverlap;
      return true;
    }
  }

  return false;
}

}  // namespace mozilla
