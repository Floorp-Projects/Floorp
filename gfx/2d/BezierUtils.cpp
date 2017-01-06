/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:ts=2:et:sw=2:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BezierUtils.h"

#include "PathHelpers.h"

namespace mozilla {
namespace gfx {

Point
GetBezierPoint(const Bezier& aBezier, Float t)
{
  Float s = 1.0f - t;

  return Point(
    aBezier.mPoints[0].x * s * s * s +
    3.0f * aBezier.mPoints[1].x * t * s * s +
    3.0f * aBezier.mPoints[2].x * t * t * s +
    aBezier.mPoints[3].x * t * t * t,
    aBezier.mPoints[0].y * s * s * s +
    3.0f * aBezier.mPoints[1].y * t * s * s +
    3.0f * aBezier.mPoints[2].y * t * t * s +
    aBezier.mPoints[3].y * t * t * t
    );
}

Point
GetBezierDifferential(const Bezier& aBezier, Float t)
{
  // Return P'(t).

  Float s = 1.0f - t;

  return Point(
    -3.0f * ((aBezier.mPoints[0].x - aBezier.mPoints[1].x) * s * s +
             2.0f * (aBezier.mPoints[1].x - aBezier.mPoints[2].x) * t * s +
             (aBezier.mPoints[2].x - aBezier.mPoints[3].x) * t * t),
    -3.0f * ((aBezier.mPoints[0].y - aBezier.mPoints[1].y) * s * s +
             2.0f * (aBezier.mPoints[1].y - aBezier.mPoints[2].y) * t * s+
             (aBezier.mPoints[2].y - aBezier.mPoints[3].y) * t * t)
    );
}

Point
GetBezierDifferential2(const Bezier& aBezier, Float t)
{
  // Return P''(t).

  Float s = 1.0f - t;

  return Point(
    6.0f * ((aBezier.mPoints[0].x - aBezier.mPoints[1].x) * s -
            (aBezier.mPoints[1].x - aBezier.mPoints[2].x) * (s - t) -
            (aBezier.mPoints[2].x - aBezier.mPoints[3].x) * t),
    6.0f * ((aBezier.mPoints[0].y - aBezier.mPoints[1].y) * s -
            (aBezier.mPoints[1].y - aBezier.mPoints[2].y) * (s - t) -
            (aBezier.mPoints[2].y - aBezier.mPoints[3].y) * t)
    );
}

Float
GetBezierLength(const Bezier& aBezier, Float a, Float b)
{
  if (a < 0.5f && b > 0.5f) {
    // To increase the accuracy, split into two parts.
    return GetBezierLength(aBezier, a, 0.5f) +
           GetBezierLength(aBezier, 0.5f, b);
  }

  // Calculate length of simple bezier curve with Simpson's rule.
  //            _
  //           /  b
  // length =  |    |P'(x)| dx
  //          _/  a
  //
  //          b - a                   a + b
  //        = ----- [ |P'(a)| + 4 |P'(-----)| + |P'(b)| ]
  //            6                       2

  Float fa = GetBezierDifferential(aBezier, a).Length();
  Float fab = GetBezierDifferential(aBezier, (a + b) / 2.0f).Length();
  Float fb = GetBezierDifferential(aBezier, b).Length();

  return (b - a) / 6.0f * (fa + 4.0f * fab + fb);
}

static void
SplitBezierA(Bezier* aSubBezier, const Bezier& aBezier, Float t)
{
  // Split bezier curve into [0,t] and [t,1] parts, and return [0,t] part.

  Float s = 1.0f - t;

  Point tmp1;
  Point tmp2;

  aSubBezier->mPoints[0] = aBezier.mPoints[0];

  aSubBezier->mPoints[1] = aBezier.mPoints[0] * s + aBezier.mPoints[1] * t;
  tmp1 = aBezier.mPoints[1] * s + aBezier.mPoints[2] * t;
  tmp2 = aBezier.mPoints[2] * s + aBezier.mPoints[3] * t;

  aSubBezier->mPoints[2] = aSubBezier->mPoints[1] * s + tmp1 * t;
  tmp1 = tmp1 * s + tmp2 * t;

  aSubBezier->mPoints[3] = aSubBezier->mPoints[2] * s + tmp1 * t;
}

static void
SplitBezierB(Bezier* aSubBezier, const Bezier& aBezier, Float t)
{
  // Split bezier curve into [0,t] and [t,1] parts, and return [t,1] part.

  Float s = 1.0f - t;

  Point tmp1;
  Point tmp2;

  aSubBezier->mPoints[3] = aBezier.mPoints[3];

  aSubBezier->mPoints[2] = aBezier.mPoints[2] * s + aBezier.mPoints[3] * t;
  tmp1 = aBezier.mPoints[1] * s + aBezier.mPoints[2] * t;
  tmp2 = aBezier.mPoints[0] * s + aBezier.mPoints[1] * t;

  aSubBezier->mPoints[1] = tmp1 * s + aSubBezier->mPoints[2] * t;
  tmp1 = tmp2 * s + tmp1 * t;

  aSubBezier->mPoints[0] = tmp1 * s + aSubBezier->mPoints[1] * t;
}

void
GetSubBezier(Bezier* aSubBezier, const Bezier& aBezier, Float t1, Float t2)
{
  Bezier tmp;
  SplitBezierB(&tmp, aBezier, t1);

  Float range = 1.0f - t1;
  if (range == 0.0f) {
    *aSubBezier = tmp;
  } else {
    SplitBezierA(aSubBezier, tmp, (t2 - t1) / range);
  }
}

static Point
BisectBezierNearestPoint(const Bezier& aBezier, const Point& aTarget,
                         Float* aT)
{
  // Find a nearest point on bezier curve with Binary search.
  // Called from FindBezierNearestPoint.

  Float lower = 0.0f;
  Float upper = 1.0f;
  Float t;

  Point P, lastP;
  const size_t MAX_LOOP = 32;
  const Float DIST_MARGIN = 0.1f;
  const Float DIST_MARGIN_SQUARE = DIST_MARGIN * DIST_MARGIN;
  const Float DIFF = 0.0001f;
  for (size_t i = 0; i < MAX_LOOP; i++) {
    t = (upper + lower) / 2.0f;
    P = GetBezierPoint(aBezier, t);

    // Check if it converged.
    if (i > 0 && (lastP - P).LengthSquare() < DIST_MARGIN_SQUARE) {
      break;
    }

    Float distSquare = (P - aTarget).LengthSquare();
    if ((GetBezierPoint(aBezier, t + DIFF) - aTarget).LengthSquare() <
        distSquare) {
      lower = t;
    } else if ((GetBezierPoint(aBezier, t - DIFF) - aTarget).LengthSquare() <
               distSquare) {
      upper = t;
    } else {
      break;
    }

    lastP = P;
  }

  if (aT) {
    *aT = t;
  }

  return P;
}

Point
FindBezierNearestPoint(const Bezier& aBezier, const Point& aTarget,
                       Float aInitialT, Float* aT)
{
  // Find a nearest point on bezier curve with Newton's method.
  // It converges within 4 iterations in most cases.
  //
  //                   f(t_n)
  //  t_{n+1} = t_n - ---------
  //                   f'(t_n)
  //
  //             d                     2
  //     f(t) = ---- | P(t) - aTarget |
  //             dt

  Float t = aInitialT;
  Point P;
  Point lastP = GetBezierPoint(aBezier, t);

  const size_t MAX_LOOP = 4;
  const Float DIST_MARGIN = 0.1f;
  const Float DIST_MARGIN_SQUARE = DIST_MARGIN * DIST_MARGIN;
  for (size_t i = 0; i <= MAX_LOOP; i++) {
    Point dP = GetBezierDifferential(aBezier, t);
    Point ddP = GetBezierDifferential2(aBezier, t);
    Float f = 2.0f * (lastP.DotProduct(dP) - aTarget.DotProduct(dP));
    Float df = 2.0f * (dP.DotProduct(dP) + lastP.DotProduct(ddP) -
                       aTarget.DotProduct(ddP));
    t = t - f / df;
    P = GetBezierPoint(aBezier, t);
    if ((P - lastP).LengthSquare() < DIST_MARGIN_SQUARE) {
      break;
    }
    lastP = P;

    if (i == MAX_LOOP) {
      // If aInitialT is too bad, it won't converge in a few iterations,
      // fallback to binary search.
      return BisectBezierNearestPoint(aBezier, aTarget, aT);
    }
  }

  if (aT) {
    *aT = t;
  }

  return P;
}

void
GetBezierPointsForCorner(Bezier* aBezier, Corner aCorner,
                         const Point& aCornerPoint, const Size& aCornerSize)
{
  // Calculate bezier control points for elliptic arc.

  const Float signsList[4][2] = {
    { +1.0f, +1.0f },
    { -1.0f, +1.0f },
    { -1.0f, -1.0f },
    { +1.0f, -1.0f }
  };
  const Float (& signs)[2] = signsList[aCorner];

  aBezier->mPoints[0] = aCornerPoint;
  aBezier->mPoints[0].x += signs[0] * aCornerSize.width;

  aBezier->mPoints[1] = aBezier->mPoints[0];
  aBezier->mPoints[1].x -= signs[0] * aCornerSize.width * kKappaFactor;

  aBezier->mPoints[3] = aCornerPoint;
  aBezier->mPoints[3].y += signs[1] * aCornerSize.height;

  aBezier->mPoints[2] = aBezier->mPoints[3];
  aBezier->mPoints[2].y -= signs[1] * aCornerSize.height * kKappaFactor;
}

Float
GetQuarterEllipticArcLength(Float a, Float b)
{
  // Calculate the approximate length of a quarter elliptic arc formed by radii
  // (a, b), by Ramanujan's approximation of the perimeter p of an ellipse.
  //           _                                                     _
  //          |                                      2                |
  //          |                           3 * (a - b)                 |
  //  p =  PI | (a + b) + ------------------------------------------- |
  //          |                                 2                 2   |
  //          |_           10 * (a + b) + sqrt(a  + 14 * a * b + b ) _|
  //
  //           _                                                            _
  //          |                                         2                    |
  //          |                              3 * (a - b)                     |
  //    =  PI | (a + b) + -------------------------------------------------- |
  //          |                                           2              2   |
  //          |_           10 * (a + b) + sqrt(4 * (a + b)  - 3 * (a - b) ) _|
  //
  //           _                                          _
  //          |                          2                 |
  //          |                     3 * S                  |
  //    =  PI | A + -------------------------------------- |
  //          |                               2        2   |
  //          |_           10 * A + sqrt(4 * A  - 3 * S ) _|
  //
  // where A = a + b, S = a - b

  Float A = a + b, S = a - b;
  Float A2 = A * A, S2 = S * S;
  Float p = M_PI * (A + 3.0f * S2 / (10.0f * A + sqrt(4.0f * A2 - 3.0f * S2)));
  return p / 4.0f;
}

Float
CalculateDistanceToEllipticArc(const Point& P, const Point& normal,
                               const Point& origin, Float width, Float height)
{
  // Solve following equations with n and return smaller n.
  //
  // /  (x, y) = P + n * normal
  // |
  // <   _            _ 2   _            _ 2
  // |  | x - origin.x |   | y - origin.y |
  // |  | ------------ | + | ------------ | = 1
  // \  |_   width    _|   |_   height   _|

  Float a = (P.x - origin.x) / width;
  Float b = normal.x / width;
  Float c = (P.y - origin.y) / height;
  Float d = normal.y / height;

  Float A = b * b + d * d;
  Float B = a * b + c * d;
  Float C = a * a + c * c - 1;

  Float S = sqrt(B * B - A * C);

  Float n1 = (- B + S) / A;
  Float n2 = (- B - S) / A;

  MOZ_ASSERT(n1 >= 0);
  MOZ_ASSERT(n2 >= 0);

  return n1 < n2 ? n1 : n2;
}

} // namespace gfx
} // namespace mozilla
