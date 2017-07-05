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

template<> Matrix
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

template<> MatrixDouble
MatrixDouble::Rotation(Double aAngle)
{
  MatrixDouble newMatrix;

  Double s = sin(aAngle);
  Double c = cos(aAngle);

  newMatrix._11 = c;
  newMatrix._12 = s;
  newMatrix._21 = -s;
  newMatrix._22 = c;

  return newMatrix;
}

template<> Matrix4x4
MatrixDouble::operator*(const Matrix4x4& aMatrix) const
{
  Matrix4x4 resultMatrix;

  resultMatrix._11 = this->_11 * aMatrix._11 + this->_12 * aMatrix._21;
  resultMatrix._12 = this->_11 * aMatrix._12 + this->_12 * aMatrix._22;
  resultMatrix._13 = this->_11 * aMatrix._13 + this->_12 * aMatrix._23;
  resultMatrix._14 = this->_11 * aMatrix._14 + this->_12 * aMatrix._24;

  resultMatrix._21 = this->_21 * aMatrix._11 + this->_22 * aMatrix._21;
  resultMatrix._22 = this->_21 * aMatrix._12 + this->_22 * aMatrix._22;
  resultMatrix._23 = this->_21 * aMatrix._13 + this->_22 * aMatrix._23;
  resultMatrix._24 = this->_21 * aMatrix._14 + this->_22 * aMatrix._24;

  resultMatrix._31 = aMatrix._31;
  resultMatrix._32 = aMatrix._32;
  resultMatrix._33 = aMatrix._33;
  resultMatrix._34 = aMatrix._34;

  resultMatrix._41 = this->_31 * aMatrix._11 + this->_32 * aMatrix._21 + aMatrix._41;
  resultMatrix._42 = this->_31 * aMatrix._12 + this->_32 * aMatrix._22 + aMatrix._42;
  resultMatrix._43 = this->_31 * aMatrix._13 + this->_32 * aMatrix._23 + aMatrix._43;
  resultMatrix._44 = this->_31 * aMatrix._14 + this->_32 * aMatrix._24 + aMatrix._44;

  return resultMatrix;
}

} // namespace gfx
} // namespace mozilla
