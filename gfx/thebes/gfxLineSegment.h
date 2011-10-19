/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Corporation code.
 *
 * The Initial Developer of the Original Code is Oracle Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Matt Woodrow <mwoodrow@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef GFX_LINESEGMENT_H
#define GFX_LINESEGMENT_H

#include "gfxTypes.h"
#include "gfxPoint.h"

struct THEBES_API gfxLineSegment {
  gfxLineSegment(const gfxPoint &aStart, const gfxPoint &aEnd) 
    : mStart(aStart)
    , mEnd(aEnd)
  {}

  bool PointsOnSameSide(const gfxPoint& aOne, const gfxPoint& aTwo)
  {
    // Solve the equation y - mStart.y - ((mEnd.y - mStart.y)/(mEnd.x - mStart.x))(x - mStart.x) for both points 
  
    gfxFloat deltaY = (mEnd.y - mStart.y);
    gfxFloat deltaX = (mEnd.x - mStart.x);
  
    gfxFloat one = deltaX * (aOne.y - mStart.y) - deltaY * (aOne.x - mStart.x);
    gfxFloat two = deltaX * (aTwo.y - mStart.y) - deltaY * (aTwo.x - mStart.x);

    // If both results have the same sign, then we're on the correct side of the line.
    // 0 (on the line) is always considered in.

    if ((one >= 0 && two >= 0) || (one <= 0 && two <= 0))
      return true;
    return false;
  }

  /**
   * Determines if two line segments intersect, and returns the intersection
   * point in aIntersection if they do.
   *
   * Coincident lines are considered not intersecting as they don't have an
   * intersection point.
   */
  bool Intersects(const gfxLineSegment& aOther, gfxPoint& aIntersection)
  {
    gfxFloat denominator = (aOther.mEnd.y - aOther.mStart.y) * (mEnd.x - mStart.x ) - 
                           (aOther.mEnd.x - aOther.mStart.x ) * (mEnd.y - mStart.y);

    // Parallel or coincident. We treat coincident as not intersecting since
    // these lines are guaranteed to have corners that intersect instead.
    if (!denominator) {
      return false;
    }

    gfxFloat anumerator = (aOther.mEnd.x - aOther.mStart.x) * (mStart.y - aOther.mStart.y) -
                         (aOther.mEnd.y - aOther.mStart.y) * (mStart.x - aOther.mStart.x);
  
    gfxFloat bnumerator = (mEnd.x - mStart.x) * (mStart.y - aOther.mStart.y) -
                         (mEnd.y - mStart.y) * (mStart.x - aOther.mStart.x);

    gfxFloat ua = anumerator / denominator;
    gfxFloat ub = bnumerator / denominator;

    if (ua <= 0.0 || ua >= 1.0 ||
        ub <= 0.0 || ub >= 1.0) {
      //Intersection is outside of the segment
      return false;
    }

    aIntersection = mStart + (mEnd - mStart) * ua;  
    return true;
  }

  gfxPoint mStart;
  gfxPoint mEnd;
};

#endif /* GFX_LINESEGMENT_H */
