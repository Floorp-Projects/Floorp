/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "2D.h"
#include "PathAnalysis.h"
#include "PathHelpers.h"

namespace mozilla {
namespace gfx {

static double CubicRoot(double aValue) {
  if (aValue < 0.0) {
    return -CubicRoot(-aValue);
  } else {
    return pow(aValue, 1.0 / 3.0);
  }
}

struct PointD : public BasePoint<double, PointD> {
  typedef BasePoint<double, PointD> Super;

  PointD() = default;
  PointD(double aX, double aY) : Super(aX, aY) {}
  MOZ_IMPLICIT PointD(const Point& aPoint) : Super(aPoint.x, aPoint.y) {}

  Point ToPoint() const {
    return Point(static_cast<Float>(x), static_cast<Float>(y));
  }
};

struct BezierControlPoints {
  BezierControlPoints() = default;
  BezierControlPoints(const PointD& aCP1, const PointD& aCP2,
                      const PointD& aCP3, const PointD& aCP4)
      : mCP1(aCP1), mCP2(aCP2), mCP3(aCP3), mCP4(aCP4) {}

  PointD mCP1, mCP2, mCP3, mCP4;
};

void FlattenBezier(const BezierControlPoints& aPoints, PathSink* aSink,
                   double aTolerance);

Path::Path() = default;

Path::~Path() = default;

Float Path::ComputeLength() {
  EnsureFlattenedPath();
  return mFlattenedPath->ComputeLength();
}

Point Path::ComputePointAtLength(Float aLength, Point* aTangent) {
  EnsureFlattenedPath();
  return mFlattenedPath->ComputePointAtLength(aLength, aTangent);
}

void Path::EnsureFlattenedPath() {
  if (!mFlattenedPath) {
    mFlattenedPath = new FlattenedPath();
    StreamToSink(mFlattenedPath);
  }
}

// This is the maximum deviation we allow (with an additional ~20% margin of
// error) of the approximation from the actual Bezier curve.
const Float kFlatteningTolerance = 0.0001f;

void FlattenedPath::MoveTo(const Point& aPoint) {
  MOZ_ASSERT(!mCalculatedLength);
  FlatPathOp op;
  op.mType = FlatPathOp::OP_MOVETO;
  op.mPoint = aPoint;
  mPathOps.push_back(op);

  mBeginPoint = aPoint;
}

void FlattenedPath::LineTo(const Point& aPoint) {
  MOZ_ASSERT(!mCalculatedLength);
  FlatPathOp op;
  op.mType = FlatPathOp::OP_LINETO;
  op.mPoint = aPoint;
  mPathOps.push_back(op);
}

void FlattenedPath::BezierTo(const Point& aCP1, const Point& aCP2,
                             const Point& aCP3) {
  MOZ_ASSERT(!mCalculatedLength);
  FlattenBezier(BezierControlPoints(CurrentPoint(), aCP1, aCP2, aCP3), this,
                kFlatteningTolerance);
}

void FlattenedPath::QuadraticBezierTo(const Point& aCP1, const Point& aCP2) {
  MOZ_ASSERT(!mCalculatedLength);
  // We need to elevate the degree of this quadratic B�zier to cubic, so we're
  // going to add an intermediate control point, and recompute control point 1.
  // The first and last control points remain the same.
  // This formula can be found on http://fontforge.sourceforge.net/bezier.html
  Point CP0 = CurrentPoint();
  Point CP1 = (CP0 + aCP1 * 2.0) / 3.0;
  Point CP2 = (aCP2 + aCP1 * 2.0) / 3.0;
  Point CP3 = aCP2;

  BezierTo(CP1, CP2, CP3);
}

void FlattenedPath::Close() {
  MOZ_ASSERT(!mCalculatedLength);
  LineTo(mBeginPoint);
}

void FlattenedPath::Arc(const Point& aOrigin, float aRadius, float aStartAngle,
                        float aEndAngle, bool aAntiClockwise) {
  ArcToBezier(this, aOrigin, Size(aRadius, aRadius), aStartAngle, aEndAngle,
              aAntiClockwise);
}

Float FlattenedPath::ComputeLength() {
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

    mCalculatedLength = true;
  }

  return mCachedLength;
}

Point FlattenedPath::ComputePointAtLength(Float aLength, Point* aTangent) {
  if (aLength < mCursor.mLength) {
    // If cursor is beyond the target length, reset to the beginning.
    mCursor.Reset();
  } else {
    // Adjust aLength to account for the position where we'll start searching.
    aLength -= mCursor.mLength;
  }

  while (mCursor.mIndex < mPathOps.size()) {
    const auto& op = mPathOps[mCursor.mIndex];
    if (op.mType == FlatPathOp::OP_MOVETO) {
      if (Distance(mCursor.mCurrentPoint, op.mPoint) > 0.0f) {
        mCursor.mLastPointSinceMove = mCursor.mCurrentPoint;
      }
      mCursor.mCurrentPoint = op.mPoint;
    } else {
      Float segmentLength = Distance(mCursor.mCurrentPoint, op.mPoint);

      if (segmentLength) {
        mCursor.mLastPointSinceMove = mCursor.mCurrentPoint;
        if (segmentLength > aLength) {
          Point currentVector = op.mPoint - mCursor.mCurrentPoint;
          Point tangent = currentVector / segmentLength;
          if (aTangent) {
            *aTangent = tangent;
          }
          return mCursor.mCurrentPoint + tangent * aLength;
        }
      }

      aLength -= segmentLength;
      mCursor.mLength += segmentLength;
      mCursor.mCurrentPoint = op.mPoint;
    }
    mCursor.mIndex++;
  }

  if (aTangent) {
    Point currentVector = mCursor.mCurrentPoint - mCursor.mLastPointSinceMove;
    if (auto h = hypotf(currentVector.x, currentVector.y)) {
      *aTangent = currentVector / h;
    } else {
      *aTangent = Point();
    }
  }
  return mCursor.mCurrentPoint;
}

// This function explicitly permits aControlPoints to refer to the same object
// as either of the other arguments.
static void SplitBezier(const BezierControlPoints& aControlPoints,
                        BezierControlPoints* aFirstSegmentControlPoints,
                        BezierControlPoints* aSecondSegmentControlPoints,
                        double t) {
  MOZ_ASSERT(aSecondSegmentControlPoints);

  *aSecondSegmentControlPoints = aControlPoints;

  PointD cp1a =
      aControlPoints.mCP1 + (aControlPoints.mCP2 - aControlPoints.mCP1) * t;
  PointD cp2a =
      aControlPoints.mCP2 + (aControlPoints.mCP3 - aControlPoints.mCP2) * t;
  PointD cp1aa = cp1a + (cp2a - cp1a) * t;
  PointD cp3a =
      aControlPoints.mCP3 + (aControlPoints.mCP4 - aControlPoints.mCP3) * t;
  PointD cp2aa = cp2a + (cp3a - cp2a) * t;
  PointD cp1aaa = cp1aa + (cp2aa - cp1aa) * t;
  aSecondSegmentControlPoints->mCP4 = aControlPoints.mCP4;

  if (aFirstSegmentControlPoints) {
    aFirstSegmentControlPoints->mCP1 = aControlPoints.mCP1;
    aFirstSegmentControlPoints->mCP2 = cp1a;
    aFirstSegmentControlPoints->mCP3 = cp1aa;
    aFirstSegmentControlPoints->mCP4 = cp1aaa;
  }
  aSecondSegmentControlPoints->mCP1 = cp1aaa;
  aSecondSegmentControlPoints->mCP2 = cp2aa;
  aSecondSegmentControlPoints->mCP3 = cp3a;
}

static void FlattenBezierCurveSegment(const BezierControlPoints& aControlPoints,
                                      PathSink* aSink, double aTolerance) {
  /* The algorithm implemented here is based on:
   * http://cis.usouthal.edu/~hain/general/Publications/Bezier/Bezier%20Offset%20Curves.pdf
   *
   * The basic premise is that for a small t the third order term in the
   * equation of a cubic bezier curve is insignificantly small. This can
   * then be approximated by a quadratic equation for which the maximum
   * difference from a linear approximation can be much more easily determined.
   */
  BezierControlPoints currentCP = aControlPoints;

  double t = 0;
  double currentTolerance = aTolerance;
  while (t < 1.0) {
    PointD cp21 = currentCP.mCP2 - currentCP.mCP1;
    PointD cp31 = currentCP.mCP3 - currentCP.mCP1;

    /* To remove divisions and check for divide-by-zero, this is optimized from:
     * Float s3 = (cp31.x * cp21.y - cp31.y * cp21.x) / hypotf(cp21.x, cp21.y);
     * t = 2 * Float(sqrt(aTolerance / (3. * std::abs(s3))));
     */
    double cp21x31 = cp31.x * cp21.y - cp31.y * cp21.x;
    double h = hypot(cp21.x, cp21.y);
    if (cp21x31 * h == 0) {
      break;
    }

    double s3inv = h / cp21x31;
    t = 2 * sqrt(currentTolerance * std::abs(s3inv) / 3.);
    currentTolerance *= 1 + aTolerance;
    // Increase tolerance every iteration to prevent this loop from executing
    // too many times. This approximates the length of large curves more
    // roughly. In practice, aTolerance is the constant kFlatteningTolerance
    // which has value 0.0001. With this value, it takes 6,932 splits to double
    // currentTolerance (to 0.0002) and 23,028 splits to increase
    // currentTolerance by an order of magnitude (to 0.001).
    if (t >= 1.0) {
      break;
    }

    SplitBezier(currentCP, nullptr, &currentCP, t);

    aSink->LineTo(currentCP.mCP1.ToPoint());
  }

  aSink->LineTo(currentCP.mCP4.ToPoint());
}

static inline void FindInflectionApproximationRange(
    BezierControlPoints aControlPoints, double* aMin, double* aMax, double aT,
    double aTolerance) {
  SplitBezier(aControlPoints, nullptr, &aControlPoints, aT);

  PointD cp21 = aControlPoints.mCP2 - aControlPoints.mCP1;
  PointD cp41 = aControlPoints.mCP4 - aControlPoints.mCP1;

  if (cp21.x == 0. && cp21.y == 0.) {
    cp21 = aControlPoints.mCP3 - aControlPoints.mCP1;
  }

  if (cp21.x == 0. && cp21.y == 0.) {
    // In this case s3 becomes lim[n->0] (cp41.x * n) / n - (cp41.y * n) / n =
    // cp41.x - cp41.y.
    double s3 = cp41.x - cp41.y;

    // Use the absolute value so that Min and Max will correspond with the
    // minimum and maximum of the range.
    if (s3 == 0) {
      *aMin = -1.0;
      *aMax = 2.0;
    } else {
      double r = CubicRoot(std::abs(aTolerance / s3));
      *aMin = aT - r;
      *aMax = aT + r;
    }
    return;
  }

  double s3 = (cp41.x * cp21.y - cp41.y * cp21.x) / hypot(cp21.x, cp21.y);

  if (s3 == 0) {
    // This means within the precision we have it can be approximated
    // infinitely by a linear segment. Deal with this by specifying the
    // approximation range as extending beyond the entire curve.
    *aMin = -1.0;
    *aMax = 2.0;
    return;
  }

  double tf = CubicRoot(std::abs(aTolerance / s3));

  *aMin = aT - tf * (1 - aT);
  *aMax = aT + tf * (1 - aT);
}

/* Find the inflection points of a bezier curve. Will return false if the
 * curve is degenerate in such a way that it is best approximated by a straight
 * line.
 *
 * The below algorithm was written by Jeff Muizelaar <jmuizelaar@mozilla.com>,
 * explanation follows:
 *
 * The lower inflection point is returned in aT1, the higher one in aT2. In the
 * case of a single inflection point this will be in aT1.
 *
 * The method is inspired by the algorithm in "analysis of in?ection points for
 * planar cubic bezier curve"
 *
 * Here are some differences between this algorithm and versions discussed
 * elsewhere in the literature:
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
 * this avoids any multiplications and may or may not be faster than the
 * approach take below.
 *
 * "fast, precise flattening of cubic bezier path and ofset curves" by hain et.
 * al
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
static inline void FindInflectionPoints(
    const BezierControlPoints& aControlPoints, double* aT1, double* aT2,
    uint32_t* aCount) {
  // Find inflection points.
  // See www.faculty.idc.ac.il/arik/quality/appendixa.html for an explanation
  // of this approach.
  PointD A = aControlPoints.mCP2 - aControlPoints.mCP1;
  PointD B =
      aControlPoints.mCP3 - (aControlPoints.mCP2 * 2) + aControlPoints.mCP1;
  PointD C = aControlPoints.mCP4 - (aControlPoints.mCP3 * 3) +
             (aControlPoints.mCP2 * 3) - aControlPoints.mCP1;

  double a = B.x * C.y - B.y * C.x;
  double b = A.x * C.y - A.y * C.x;
  double c = A.x * B.y - A.y * B.x;

  if (a == 0) {
    // Not a quadratic equation.
    if (b == 0) {
      // Instead of a linear acceleration change we have a constant
      // acceleration change. This means the equation has no solution
      // and there are no inflection points, unless the constant is 0.
      // In that case the curve is a straight line, essentially that means
      // the easiest way to deal with is is by saying there's an inflection
      // point at t == 0. The inflection point approximation range found will
      // automatically extend into infinity.
      if (c == 0) {
        *aCount = 1;
        *aT1 = 0;
        return;
      }
      *aCount = 0;
      return;
    }
    *aT1 = -c / b;
    *aCount = 1;
    return;
  }

  double discriminant = b * b - 4 * a * c;

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
    double q = sqrt(discriminant);
    if (b < 0) {
      q = b - q;
    } else {
      q = b + q;
    }
    q *= -1. / 2;

    *aT1 = q / a;
    *aT2 = c / q;
    if (*aT1 > *aT2) {
      std::swap(*aT1, *aT2);
    }
    *aCount = 2;
  }
}

void FlattenBezier(const BezierControlPoints& aControlPoints, PathSink* aSink,
                   double aTolerance) {
  double t1;
  double t2;
  uint32_t count;

  FindInflectionPoints(aControlPoints, &t1, &t2, &count);

  // Check that at least one of the inflection points is inside [0..1]
  if (count == 0 ||
      ((t1 < 0.0 || t1 >= 1.0) && (count == 1 || (t2 < 0.0 || t2 >= 1.0)))) {
    FlattenBezierCurveSegment(aControlPoints, aSink, aTolerance);
    return;
  }

  double t1min = t1, t1max = t1, t2min = t2, t2max = t2;

  BezierControlPoints remainingCP = aControlPoints;

  // For both inflection points, calulate the range where they can be linearly
  // approximated if they are positioned within [0,1]
  if (count > 0 && t1 >= 0 && t1 < 1.0) {
    FindInflectionApproximationRange(aControlPoints, &t1min, &t1max, t1,
                                     aTolerance);
  }
  if (count > 1 && t2 >= 0 && t2 < 1.0) {
    FindInflectionApproximationRange(aControlPoints, &t2min, &t2max, t2,
                                     aTolerance);
  }
  BezierControlPoints nextCPs = aControlPoints;
  BezierControlPoints prevCPs;

  // Process ranges. [t1min, t1max] and [t2min, t2max] are approximated by line
  // segments.
  if (count == 1 && t1min <= 0 && t1max >= 1.0) {
    // The whole range can be approximated by a line segment.
    aSink->LineTo(aControlPoints.mCP4.ToPoint());
    return;
  }

  if (t1min > 0) {
    // Flatten the Bezier up until the first inflection point's approximation
    // point.
    SplitBezier(aControlPoints, &prevCPs, &remainingCP, t1min);
    FlattenBezierCurveSegment(prevCPs, aSink, aTolerance);
  }
  if (t1max >= 0 && t1max < 1.0 && (count == 1 || t2min > t1max)) {
    // The second inflection point's approximation range begins after the end
    // of the first, approximate the first inflection point by a line and
    // subsequently flatten up until the end or the next inflection point.
    SplitBezier(aControlPoints, nullptr, &nextCPs, t1max);

    aSink->LineTo(nextCPs.mCP1.ToPoint());

    if (count == 1 || (count > 1 && t2min >= 1.0)) {
      // No more inflection points to deal with, flatten the rest of the curve.
      FlattenBezierCurveSegment(nextCPs, aSink, aTolerance);
    }
  } else if (count > 1 && t2min > 1.0) {
    // We've already concluded t2min <= t1max, so if this is true the
    // approximation range for the first inflection point runs past the
    // end of the curve, draw a line to the end and we're done.
    aSink->LineTo(aControlPoints.mCP4.ToPoint());
    return;
  }

  if (count > 1 && t2min < 1.0 && t2max > 0) {
    if (t2min > 0 && t2min < t1max) {
      // In this case the t2 approximation range starts inside the t1
      // approximation range.
      SplitBezier(aControlPoints, nullptr, &nextCPs, t1max);
      aSink->LineTo(nextCPs.mCP1.ToPoint());
    } else if (t2min > 0 && t1max > 0) {
      SplitBezier(aControlPoints, nullptr, &nextCPs, t1max);

      // Find a control points describing the portion of the curve between t1max
      // and t2min.
      double t2mina = (t2min - t1max) / (1 - t1max);
      SplitBezier(nextCPs, &prevCPs, &nextCPs, t2mina);
      FlattenBezierCurveSegment(prevCPs, aSink, aTolerance);
    } else if (t2min > 0) {
      // We have nothing interesting before t2min, find that bit and flatten it.
      SplitBezier(aControlPoints, &prevCPs, &nextCPs, t2min);
      FlattenBezierCurveSegment(prevCPs, aSink, aTolerance);
    }
    if (t2max < 1.0) {
      // Flatten the portion of the curve after t2max
      SplitBezier(aControlPoints, nullptr, &nextCPs, t2max);

      // Draw a line to the start, this is the approximation between t2min and
      // t2max.
      aSink->LineTo(nextCPs.mCP1.ToPoint());
      FlattenBezierCurveSegment(nextCPs, aSink, aTolerance);
    } else {
      // Our approximation range extends beyond the end of the curve.
      aSink->LineTo(aControlPoints.mCP4.ToPoint());
      return;
    }
  }
}

Rect Path::GetFastBounds(const Matrix& aTransform,
                         const StrokeOptions* aStrokeOptions) const {
  return aStrokeOptions ? GetStrokedBounds(*aStrokeOptions, aTransform)
                        : GetBounds(aTransform);
}

}  // namespace gfx
}  // namespace mozilla
