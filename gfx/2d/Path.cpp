/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "2D.h"
#include "PathAnalysis.h"
#include "PathHelpers.h"

namespace mozilla {
namespace gfx {

struct BezierControlPoints
{
  BezierControlPoints() {}
  BezierControlPoints(const Point &aCP1, const Point &aCP2,
                      const Point &aCP3, const Point &aCP4)
    : mCP1(aCP1), mCP2(aCP2), mCP3(aCP3), mCP4(aCP4)
  {
  }

  Point mCP1, mCP2, mCP3, mCP4;
};

void
FlattenBezier(const BezierControlPoints &aPoints,
              PathSink *aSink, Float aTolerance);


Path::Path()
{
}

Path::~Path()
{
}

Float
Path::ComputeLength()
{
  EnsureFlattenedPath();
  return mFlattenedPath->ComputeLength();
}

Point
Path::ComputePointAtLength(Float aLength, Point* aTangent)
{
  EnsureFlattenedPath();
  return mFlattenedPath->ComputePointAtLength(aLength, aTangent);
}

void
Path::EnsureFlattenedPath()
{
  if (!mFlattenedPath) {
    mFlattenedPath = new FlattenedPath();
    StreamToSink(mFlattenedPath);
  }
}

// This is the maximum deviation we allow (with an additional ~20% margin of
// error) of the approximation from the actual Bezier curve.
const Float kFlatteningTolerance = 0.0001f;

void
FlattenedPath::MoveTo(const Point &aPoint)
{
  MOZ_ASSERT(!mCalculatedLength);
  FlatPathOp op;
  op.mType = FlatPathOp::OP_MOVETO;
  op.mPoint = aPoint;
  mPathOps.push_back(op);

  mLastMove = aPoint;
}

void
FlattenedPath::LineTo(const Point &aPoint)
{
  MOZ_ASSERT(!mCalculatedLength);
  FlatPathOp op;
  op.mType = FlatPathOp::OP_LINETO;
  op.mPoint = aPoint;
  mPathOps.push_back(op);
}

void
FlattenedPath::BezierTo(const Point &aCP1,
                        const Point &aCP2,
                        const Point &aCP3)
{
  MOZ_ASSERT(!mCalculatedLength);
  FlattenBezier(BezierControlPoints(CurrentPoint(), aCP1, aCP2, aCP3), this, kFlatteningTolerance);
}

void
FlattenedPath::QuadraticBezierTo(const Point &aCP1,
                                 const Point &aCP2)
{
  MOZ_ASSERT(!mCalculatedLength);
  // We need to elevate the degree of this quadratic Bézier to cubic, so we're
  // going to add an intermediate control point, and recompute control point 1.
  // The first and last control points remain the same.
  // This formula can be found on http://fontforge.sourceforge.net/bezier.html
  Point CP0 = CurrentPoint();
  Point CP1 = (CP0 + aCP1 * 2.0) / 3.0;
  Point CP2 = (aCP2 + aCP1 * 2.0) / 3.0;
  Point CP3 = aCP2;

  BezierTo(CP1, CP2, CP3);
}

void
FlattenedPath::Close()
{
  MOZ_ASSERT(!mCalculatedLength);
  LineTo(mLastMove);
}

void
FlattenedPath::Arc(const Point &aOrigin, float aRadius, float aStartAngle,
                   float aEndAngle, bool aAntiClockwise)
{
  ArcToBezier(this, aOrigin, Size(aRadius, aRadius), aStartAngle, aEndAngle, aAntiClockwise);
}

Float
FlattenedPath::ComputeLength()
{
  if (!mCalculatedLength) {
    Point currentPoint;

    for (uint32_t i = 0; i < mPathOps.size(); i++) {
      if (mPathOps[i].mType == FlatPathOp::OP_MOVETO) {
        currentPoint = mPathOps[i].mPoint;
      } else {
        mCachedLength += Distance(currentPoint, mPathOps[i].mPoint);
        currentPoint = mPathOps[i].mPoint;
      }
    }

    mCalculatedLength =  true;
  }

  return mCachedLength;
}

Point
FlattenedPath::ComputePointAtLength(Float aLength, Point *aTangent)
{
  // We track the last point that -wasn't- in the same place as the current
  // point so if we pass the edge of the path with a bunch of zero length
  // paths we still get the correct tangent vector.
  Point lastPointSinceMove;
  Point currentPoint;
  for (uint32_t i = 0; i < mPathOps.size(); i++) {
    if (mPathOps[i].mType == FlatPathOp::OP_MOVETO) {
      if (Distance(currentPoint, mPathOps[i].mPoint)) {
        lastPointSinceMove = currentPoint;
      }
      currentPoint = mPathOps[i].mPoint;
    } else {
      Float segmentLength = Distance(currentPoint, mPathOps[i].mPoint);

      if (segmentLength) {
        lastPointSinceMove = currentPoint;
        if (segmentLength > aLength) {
          Point currentVector = mPathOps[i].mPoint - currentPoint;
          Point tangent = currentVector / segmentLength;
          if (aTangent) {
            *aTangent = tangent;
          }
          return currentPoint + tangent * aLength;
        }
      }

      aLength -= segmentLength;
      currentPoint = mPathOps[i].mPoint;
    }
  }

  Point currentVector = currentPoint - lastPointSinceMove;
  if (aTangent) {
    if (hypotf(currentVector.x, currentVector.y)) {
      *aTangent = currentVector / hypotf(currentVector.x, currentVector.y);
    } else {
      *aTangent = Point();
    }
  }
  return currentPoint;
}

// This function explicitly permits aControlPoints to refer to the same object
// as either of the other arguments.
static void 
SplitBezier(const BezierControlPoints &aControlPoints,
            BezierControlPoints *aFirstSegmentControlPoints,
            BezierControlPoints *aSecondSegmentControlPoints,
            Float t)
{
  MOZ_ASSERT(aSecondSegmentControlPoints);
  
  *aSecondSegmentControlPoints = aControlPoints;

  Point cp1a = aControlPoints.mCP1 + (aControlPoints.mCP2 - aControlPoints.mCP1) * t;
  Point cp2a = aControlPoints.mCP2 + (aControlPoints.mCP3 - aControlPoints.mCP2) * t;
  Point cp1aa = cp1a + (cp2a - cp1a) * t;
  Point cp3a = aControlPoints.mCP3 + (aControlPoints.mCP4 - aControlPoints.mCP3) * t;
  Point cp2aa = cp2a + (cp3a - cp2a) * t;
  Point cp1aaa = cp1aa + (cp2aa - cp1aa) * t;
  aSecondSegmentControlPoints->mCP4 = aControlPoints.mCP4;

  if(aFirstSegmentControlPoints) {
    aFirstSegmentControlPoints->mCP1 = aControlPoints.mCP1;
    aFirstSegmentControlPoints->mCP2 = cp1a;
    aFirstSegmentControlPoints->mCP3 = cp1aa;
    aFirstSegmentControlPoints->mCP4 = cp1aaa;
  }
  aSecondSegmentControlPoints->mCP1 = cp1aaa;
  aSecondSegmentControlPoints->mCP2 = cp2aa;
  aSecondSegmentControlPoints->mCP3 = cp3a;
}

static void
FlattenBezierCurveSegment(const BezierControlPoints &aControlPoints,
                          PathSink *aSink,
                          Float aTolerance)
{
  /* The algorithm implemented here is based on:
   * http://cis.usouthal.edu/~hain/general/Publications/Bezier/Bezier%20Offset%20Curves.pdf
   *
   * The basic premise is that for a small t the third order term in the
   * equation of a cubic bezier curve is insignificantly small. This can
   * then be approximated by a quadratic equation for which the maximum
   * difference from a linear approximation can be much more easily determined.
   */
  BezierControlPoints currentCP = aControlPoints;

  Float t = 0;
  while (t < 1.0f) {
    Point cp21 = currentCP.mCP2 - currentCP.mCP3;
    Point cp31 = currentCP.mCP3 - currentCP.mCP1;

    Float s3 = (cp31.x * cp21.y - cp31.y * cp21.x) / hypotf(cp21.x, cp21.y);

    t = 2 * Float(sqrt(aTolerance / (3. * abs(s3))));

    if (t >= 1.0f) {
      aSink->LineTo(aControlPoints.mCP4);
      break;
    }

    Point prevCP2, prevCP3, nextCP1, nextCP2, nextCP3;
    SplitBezier(currentCP, nullptr, &currentCP, t);

    aSink->LineTo(currentCP.mCP1);
  }
}

static inline void
FindInflectionApproximationRange(BezierControlPoints aControlPoints,
                                 Float *aMin, Float *aMax, Float aT,
                                 Float aTolerance)
{
    SplitBezier(aControlPoints, nullptr, &aControlPoints, aT);

    Point cp21 = aControlPoints.mCP2 - aControlPoints.mCP1;
    Point cp41 = aControlPoints.mCP4 - aControlPoints.mCP1;

    if (!cp21.x && !cp21.y) {
      // In this case s3 becomes lim[n->0] (cp41.x * n) / n - (cp41.y * n) / n = cp41.x - cp41.y.
      *aMin = aT - pow(aTolerance / (cp41.x - cp41.y), Float(1. / 3.));
      *aMax = aT + pow(aTolerance / (cp41.x - cp41.y), Float(1. / 3.));;
      return;
    }

    Float s3 = (cp41.x * cp21.y - cp41.y * cp21.x) / hypotf(cp21.x, cp21.y);

    Float tf = pow(abs(aTolerance / s3), Float(1. / 3.));

    *aMin = aT - tf * (1 - aT);
    *aMax = aT + tf * (1 - aT);
}

/* Find the inflection points of a bezier curve. Will return false if the
 * curve is degenerate in such a way that it is best approximated by a straight
 * line.
 *
 * The below algorithm was written by Jeff Muizelaar <jmuizelaar@mozilla.com>, explanation follows:
 *
 * The lower inflection point is returned in aT1, the higher one in aT2. In the
 * case of a single inflection point this will be in aT1.
 *
 * The method is inspired by the algorithm in "analysis of in?ection points for planar cubic bezier curve"
 *
 * Here are some differences between this algorithm and versions discussed elsewhere in the literature:
 *
 * zhang et. al compute a0, d0 and e0 incrementally using the follow formula:
 *
 * Point a0 = CP2 - CP1
 * Point a1 = CP3 - CP2
 * Point a2 = CP4 - CP1
 *
 * Point d0 = a1 - a0
 * Point d1 = a2 - a1
 
 * Point e0 = d1 - d0
 *
 * this avoids any multiplications and may or may not be faster than the approach take below.
 *
 * "fast, precise flattening of cubic bezier path and ofset curves" by hain et. al
 * Point a = CP1 + 3 * CP2 - 3 * CP3 + CP4
 * Point b = 3 * CP1 - 6 * CP2 + 3 * CP3
 * Point c = -3 * CP1 + 3 * CP2
 * Point d = CP1
 * the a, b, c, d can be expressed in terms of a0, d0 and e0 defined above as:
 * c = 3 * a0
 * b = 3 * d0
 * a = e0
 *
 *
 * a = 3a = a.y * b.x - a.x * b.y
 * b = 3b = a.y * c.x - a.x * c.y
 * c = 9c = b.y * c.x - b.x * c.y
 *
 * The additional multiples of 3 cancel each other out as show below:
 *
 * x = (-b + sqrt(b * b - 4 * a * c)) / (2 * a)
 * x = (-3 * b + sqrt(3 * b * 3 * b - 4 * a * 3 * 9 * c / 3)) / (2 * 3 * a)
 * x = 3 * (-b + sqrt(b * b - 4 * a * c)) / (2 * 3 * a)
 * x = (-b + sqrt(b * b - 4 * a * c)) / (2 * a)
 *
 * I haven't looked into whether the formulation of the quadratic formula in
 * hain has any numerical advantages over the one used below.
 */
static inline bool
FindInflectionPoints(const BezierControlPoints &aControlPoints,
                     Float *aT1, Float *aT2, uint32_t *aCount)
{
  // Find inflection points.
  // See www.faculty.idc.ac.il/arik/quality/appendixa.html for an explanation
  // of this approach.
  Point A = aControlPoints.mCP2 - aControlPoints.mCP1;
  Point B = aControlPoints.mCP3 - (aControlPoints.mCP2 * 2) + aControlPoints.mCP1;
  Point C = aControlPoints.mCP4 - (aControlPoints.mCP3 * 3) + (aControlPoints.mCP2 * 3) - aControlPoints.mCP1;

  Float a = Float(B.x) * C.y - Float(B.y) * C.x;
  Float b = Float(A.x) * C.y - Float(A.y) * C.x;
  Float c = Float(A.x) * B.y - Float(A.y) * B.x;

  if (a == 0) {
    // Not a quadratic equation.
    if (b == 0) {
      // Instead of a linear equation we have a constant.
      return false;
    }
    *aT1 = -c / b;
    *aCount = 1;
    return true;
  } else {
    Float discriminant = b * b - 4 * a * c;

    if (discriminant < 0) {
      // No inflection points.
      *aCount = 0;
    } else if (discriminant == 0) {
      *aCount = 1;
      *aT1 = -b / (2 * a);
    } else {
      /* Use the following formula for computing the roots:
       *
       * q = -1/2 * (b + sign(b) * sqrt(b^2 - 4ac))
       * t1 = q / a
       * t2 = c / q
       */
      Float q = sqrtf(discriminant);
      if (b < 0) {
        q = b - q;
      } else {
        q = b + q;
      }
      q *= Float(-1./2);

      *aT1 = q / a;
      *aT2 = c / q;
      if (*aT1 > *aT2) {
        std::swap(*aT1, *aT2);
      }
      *aCount = 2;
    }
  }

  return true;
}

void
FlattenBezier(const BezierControlPoints &aControlPoints,
              PathSink *aSink, Float aTolerance)
{
  Float t1;
  Float t2;
  uint32_t count;

  if (!FindInflectionPoints(aControlPoints, &t1, &t2, &count)) {
    aSink->LineTo(aControlPoints.mCP4);
    return;
  }

  // Check that at least one of the inflection points is inside [0..1]
  if (count == 0 || ((t1 < 0 || t1 > 1.0) && ((t2 < 0 || t2 > 1.0) || count == 1)) ) {
    FlattenBezierCurveSegment(aControlPoints, aSink, aTolerance);
    return;
  }

  Float t1min = t1, t1max = t1, t2min = t2, t2max = t2;

  BezierControlPoints remainingCP = aControlPoints;

  // For both inflection points, calulate the range where they can be linearly
  // approximated if they are positioned within [0,1]
  if (count > 0 && t1 >= 0 && t1 < 1.0) {
    FindInflectionApproximationRange(aControlPoints, &t1min, &t1max, t1, aTolerance);
  }
  if (count > 1 && t2 >= 0 && t2 < 1.0) {
    FindInflectionApproximationRange(aControlPoints, &t2min, &t2max, t2, aTolerance);
  }
  BezierControlPoints nextCPs = aControlPoints;
  BezierControlPoints prevCPs;

  // Process ranges. [t1min, t1max] and [t2min, t2max] are approximated by line
  // segments.
  if (t1min > 0) {
    // Flatten the Bezier up until the first inflection point's approximation
    // point.
    SplitBezier(aControlPoints, &prevCPs,
                &remainingCP, t1min);
    FlattenBezierCurveSegment(prevCPs, aSink, aTolerance);
  }
  if (t1max < 1.0 && (count == 1 || t2min > t1max)) {
    // The second inflection point's approximation range begins after the end
    // of the first, approximate the first inflection point by a line and
    // subsequently flatten up until the end or the next inflection point.
    SplitBezier(aControlPoints, nullptr, &nextCPs, t1max);

    aSink->LineTo(nextCPs.mCP1);

    if (count == 1 || (count > 1 && t2min >= 1.0)) {
      // No more inflection points to deal with, flatten the rest of the curve.
      FlattenBezierCurveSegment(nextCPs, aSink, aTolerance);
    }
  } else if (count > 1 && t2min > 1.0) {
    // We've already concluded t2min <= t1max, so if this is true the
    // approximation range for the first inflection point runs past the
    // end of the curve, draw a line to the end and we're done.
    aSink->LineTo(aControlPoints.mCP4);
    return;
  }

  if (count > 1 && t2min < 1.0 && t2max > 0) {
    if (t2min > 0 && t2min < t1max) {
      // In this case the t2 approximation range starts inside the t1
      // approximation range.
      SplitBezier(aControlPoints, nullptr, &nextCPs, t1max);
      aSink->LineTo(nextCPs.mCP1);
    } else if (t2min > 0 && t1max > 0) {
      SplitBezier(aControlPoints, nullptr, &nextCPs, t1max);

      // Find a control points describing the portion of the curve between t1max and t2min.
      Float t2mina = (t2min - t1max) / (1 - t1max);
      SplitBezier(nextCPs, &prevCPs, &nextCPs, t2mina);
      FlattenBezierCurveSegment(prevCPs, aSink, aTolerance);
    } else {
      // We have nothing interesting before t2min, find that bit and flatten it.
      SplitBezier(aControlPoints, &prevCPs, &nextCPs, t2min);
      FlattenBezierCurveSegment(prevCPs, aSink, aTolerance);
    }
    if (t2max < 1.0) {
      // Flatten the portion of the curve after t2max
      SplitBezier(aControlPoints, nullptr, &nextCPs, t2max);

      // Draw a line to the start, this is the approximation between t2min and
      // t2max.
      aSink->LineTo(nextCPs.mCP1);
      FlattenBezierCurveSegment(nextCPs, aSink, aTolerance);
    } else {
      // Our approximation range extends beyond the end of the curve.
      aSink->LineTo(aControlPoints.mCP4);
      return;
    }
  }
}

}
}
