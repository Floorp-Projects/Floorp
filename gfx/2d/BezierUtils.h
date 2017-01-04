/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_BezierUtils_h_
#define mozilla_BezierUtils_h_

#include "mozilla/gfx/2D.h"
#include "gfxRect.h"

namespace mozilla {
namespace gfx {

// Control points for bezier curve
//
//                   mPoints[2]
//                    +-----___---+ mPoints[3]
//                      __--
//                   _--
//                  /
//                /
// mPoints[1] +  |
//            | |
//            ||
//            ||
//            |
//            |
//            |
//            |
// mPoints[0] +
struct Bezier {
  Point mPoints[4];
};

// Calculate a point or it's differential of a bezier curve formed by
// aBezier and parameter t.
//
//   GetBezierPoint = P(t)
//   GetBezierDifferential = P'(t)
//   GetBezierDifferential2 = P''(t)
//
//                   mPoints[2]
//                    +-----___---+ mPoints[3]
//                      __--     P(1)
//                   _--
//                  +
//                /  P(t)
// mPoints[1] +  |
//            | |
//            ||
//            ||
//            |
//            |
//            |
//            |
// mPoints[0] + P(0)
Point GetBezierPoint(const Bezier& aBezier, Float t);
Point GetBezierDifferential(const Bezier& aBezier, Float t);
Point GetBezierDifferential2(const Bezier& aBezier, Float t);

// Calculate length of a simple bezier curve formed by aBezier and range [a, b].
Float GetBezierLength(const Bezier& aBezier, Float a, Float b);

// Split bezier curve formed by aBezier into [0,t1], [t1,t2], [t2,1] parts, and
// stores control points for [t1,t2] to aSubBezier.
//
//                 ___---+
//             __+-     P(1)
//          _--   P(t2)
//         -
//       /  <-- aSubBezier
//      |
//     |
//    +
//    | P(t1)
//   |
//   |
//   |
//   |
//   + P(0)
void GetSubBezier(Bezier* aSubBezier, const Bezier& aBezier,
                  Float t1, Float t2);

// Find a nearest point on bezier curve formed by aBezier to a point aTarget.
// aInitialT is a hint to find the parameter t for the nearest point.
// If aT is non-null, parameter for the nearest point is stored to *aT.
// This function expects a bezier curve to be an approximation of elliptic arc.
// Otherwise it will return wrong point.
//
//  aTarget
//    +            ___---+
//             __--
//          _--
//         +
//       /  nearest point = P(t = *aT)
//      |
//     |
//    |
//    + P(aInitialT)
//   |
//   |
//   |
//   |
//   +
Point FindBezierNearestPoint(const Bezier& aBezier, const Point& aTarget,
                             Float aInitialT, Float* aT=nullptr);

// Calculate control points for a bezier curve that is an approximation of
// an elliptic arc.
//
//                                   aCornerSize.width
//                                 |<----------------->|
//                                 |                   |
//                     aCornerPoint|      mPoints[2]   |
//                    -------------+-------+-----___---+ mPoints[3]
//                    ^            |         __--
//                    |            |      _--
//                    |            |     -
//                    |            |   /
// aCornerSize.height | mPoints[1] +  |
//                    |            | |
//                    |            ||
//                    |            ||
//                    |            |
//                    |            |
//                    |            |
//                    v mPoints[0] |
//                    -------------+
void GetBezierPointsForCorner(Bezier* aBezier, mozilla::Corner aCorner,
                              const Point& aCornerPoint,
                              const Size& aCornerSize);

// Calculate the approximate length of a quarter elliptic arc formed by radii
// (a, b).
//
//                a
//      |<----------------->|
//      |                   |
//   ---+-------------___---+
//   ^  |         __--
//   |  |      _--
//   |  |     -
//   |  |   /
// b |  |  |
//   |  | |
//   |  ||
//   |  ||
//   |  |
//   |  |
//   |  |
//   v  |
//   ---+
Float GetQuarterEllipticArcLength(Float a, Float b);

// Calculate the distance between an elliptic arc formed by (origin, width,
// height), and a point P, along a line formed by |P + n * normal|.
// P should be outside of the ellipse, and the line should cross with the
// ellipse twice at n > 0 points.
//
//                            width
//                     |<----------------->|
//              origin |                   |
//          -----------+-------------___---+
//          ^  normal  |         __--
//          | P +->__  |      _--
//          |        --__    -
//          |          | --+
//   height |          |  |
//          |          | |
//          |          ||
//          |          ||
//          |          |
//          |          |
//          |          |
//          v          |
//          -----------+
Float CalculateDistanceToEllipticArc(const Point& P, const Point& normal,
                                     const Point& origin,
                                     Float width, Float height);

} // namespace gfx
} // namespace mozilla

#endif /* mozilla_BezierUtils_h_ */
