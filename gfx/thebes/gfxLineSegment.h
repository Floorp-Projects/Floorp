/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_LINESEGMENT_H
#define GFX_LINESEGMENT_H

#include "gfxTypes.h"
#include "gfxPoint.h"

struct gfxLineSegment {
  gfxLineSegment() {}
  gfxLineSegment(const gfxPoint& aStart, const gfxPoint& aEnd)
      : mStart(aStart), mEnd(aEnd) {}

  bool PointsOnSameSide(const gfxPoint& aOne, const gfxPoint& aTwo) {
    // Solve the equation
    // y - mStart.y - ((mEnd.y - mStart.y)/(mEnd.x - mStart.x))(x - mStart.x)
    // for both points

    gfxFloat deltaY = (mEnd.y - mStart.y);
    gfxFloat deltaX = (mEnd.x - mStart.x);

    gfxFloat one = deltaX * (aOne.y - mStart.y) - deltaY * (aOne.x - mStart.x);
    gfxFloat two = deltaX * (aTwo.y - mStart.y) - deltaY * (aTwo.x - mStart.x);

    // If both results have the same sign, then we're on the correct side of the
    // line. 0 (on the line) is always considered in.

    if ((one >= 0 && two >= 0) || (one <= 0 && two <= 0)) return true;
    return false;
  }

  /**
   * Determines if two line segments intersect, and returns the intersection
   * point in aIntersection if they do.
   *
   * Coincident lines are considered not intersecting as they don't have an
   * intersection point.
   */
  bool Intersects(const gfxLineSegment& aOther, gfxPoint& aIntersection) {
    gfxFloat denominator =
        (aOther.mEnd.y - aOther.mStart.y).value * (mEnd.x - mStart.x).value -
        (aOther.mEnd.x - aOther.mStart.x).value * (mEnd.y - mStart.y).value;

    // Parallel or coincident. We treat coincident as not intersecting since
    // these lines are guaranteed to have corners that intersect instead.
    if (!denominator) {
      return false;
    }

    gfxFloat anumerator = (aOther.mEnd.x - aOther.mStart.x).value *
                              (mStart.y - aOther.mStart.y).value -
                          (aOther.mEnd.y - aOther.mStart.y).value *
                              (mStart.x - aOther.mStart.x).value;

    gfxFloat bnumerator =
        (mEnd.x - mStart.x).value * (mStart.y - aOther.mStart.y).value -
        (mEnd.y - mStart.y).value * (mStart.x - aOther.mStart.x).value;

    gfxFloat ua = anumerator / denominator;
    gfxFloat ub = bnumerator / denominator;

    if (ua <= 0.0 || ua >= 1.0 || ub <= 0.0 || ub >= 1.0) {
      // Intersection is outside of the segment
      return false;
    }

    aIntersection = mStart + (mEnd - mStart) * ua;
    return true;
  }

  gfxPoint mStart;
  gfxPoint mEnd;
};

#endif /* GFX_LINESEGMENT_H */
