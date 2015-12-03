/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Matrix.h"
#include "Quaternion.h"
#include "Tools.h"
#include <algorithm>
#include <ostream>
#include <math.h>
#include <float.h>  // for FLT_EPSILON

#include "mozilla/FloatingPoint.h" // for UnspecifiedNaN

using namespace std;


namespace mozilla {
namespace gfx {

/* Force small values to zero.  We do this to avoid having sin(360deg)
 * evaluate to a tiny but nonzero value.
 */
double
FlushToZero(double aVal)
{
  // XXX Is double precision really necessary here
  if (-FLT_EPSILON < aVal && aVal < FLT_EPSILON) {
    return 0.0f;
  } else {
    return aVal;
  }
}

/* Computes tan(aTheta).  For values of aTheta such that tan(aTheta) is
 * undefined or very large, SafeTangent returns a manageably large value
 * of the correct sign.
 */
double
SafeTangent(double aTheta)
{
  // XXX Is double precision really necessary here
  const double kEpsilon = 0.0001;

  /* tan(theta) = sin(theta)/cos(theta); problems arise when
   * cos(theta) is too close to zero.  Limit cos(theta) to the
   * range [-1, -epsilon] U [epsilon, 1].
   */

  double sinTheta = sin(aTheta);
  double cosTheta = cos(aTheta);

  if (cosTheta >= 0 && cosTheta < kEpsilon) {
    cosTheta = kEpsilon;
  } else if (cosTheta < 0 && cosTheta >= -kEpsilon) {
    cosTheta = -kEpsilon;
  }
  return FlushToZero(sinTheta / cosTheta);
}

std::ostream&
operator<<(std::ostream& aStream, const Matrix& aMatrix)
{
  return aStream << "[ " << aMatrix._11
                 << " "  << aMatrix._12
                 << "; " << aMatrix._21
                 << " "  << aMatrix._22
                 << "; " << aMatrix._31
                 << " "  << aMatrix._32
                 << "; ]";
}

Matrix
Matrix::Rotation(Float aAngle)
{
  Matrix newMatrix;

  Float s = sinf(aAngle);
  Float c = cosf(aAngle);

  newMatrix._11 = c;
  newMatrix._12 = s;
  newMatrix._21 = -s;
  newMatrix._22 = c;

  return newMatrix;
}

Rect
Matrix::TransformBounds(const Rect &aRect) const
{
  int i;
  Point quad[4];
  Float min_x, max_x;
  Float min_y, max_y;

  quad[0] = *this * aRect.TopLeft();
  quad[1] = *this * aRect.TopRight();
  quad[2] = *this * aRect.BottomLeft();
  quad[3] = *this * aRect.BottomRight();

  min_x = max_x = quad[0].x;
  min_y = max_y = quad[0].y;

  for (i = 1; i < 4; i++) {
    if (quad[i].x < min_x)
      min_x = quad[i].x;
    if (quad[i].x > max_x)
      max_x = quad[i].x;

    if (quad[i].y < min_y)
      min_y = quad[i].y;
    if (quad[i].y > max_y)
      max_y = quad[i].y;
  }

  return Rect(min_x, min_y, max_x - min_x, max_y - min_y);
}

Matrix&
Matrix::NudgeToIntegers()
{
  NudgeToInteger(&_11);
  NudgeToInteger(&_12);
  NudgeToInteger(&_21);
  NudgeToInteger(&_22);
  NudgeToInteger(&_31);
  NudgeToInteger(&_32);
  return *this;
}

} // namespace gfx
} // namespace mozilla
