/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_MATRIX_H_
#define MOZILLA_GFX_MATRIX_H_

#include "Types.h"
#include "Rect.h"
#include "Point.h"
#include <math.h>

namespace mozilla {
namespace gfx {

class Matrix
{
public:
  Matrix()
    : _11(1.0f), _12(0)
    , _21(0), _22(1.0f)
    , _31(0), _32(0)
  {}
  Matrix(Float a11, Float a12, Float a21, Float a22, Float a31, Float a32)
    : _11(a11), _12(a12)
    , _21(a21), _22(a22)
    , _31(a31), _32(a32)
  {}
  Float _11, _12;
  Float _21, _22;
  Float _31, _32;

  Point operator *(const Point &aPoint) const
  {
    Point retPoint;

    retPoint.x = aPoint.x * _11 + aPoint.y * _21 + _31;
    retPoint.y = aPoint.x * _12 + aPoint.y * _22 + _32;

    return retPoint;
  }

  Size operator *(const Size &aSize) const
  {
    Size retSize;

    retSize.width = aSize.width * _11 + aSize.height * _21;
    retSize.height = aSize.width * _12 + aSize.height * _22;

    return retSize;
  }

  GFX2D_API Rect TransformBounds(const Rect& rect) const;

  // Apply a scale to this matrix. This scale will be applied -before- the
  // existing transformation of the matrix.
  Matrix &Scale(Float aX, Float aY)
  {
    _11 *= aX;
    _12 *= aX;
    _21 *= aY;
    _22 *= aY;

    return *this;
  }

  Matrix &Translate(Float aX, Float aY)
  {
    _31 += _11 * aX + _21 * aY;
    _32 += _12 * aX + _22 * aY;

    return *this;
  }

  bool Invert()
  {
    // Compute co-factors.
    Float A = _22;
    Float B = -_21;
    Float C = _21 * _32 - _22 * _31;
    Float D = -_12;
    Float E = _11;
    Float F = _31 * _12 - _11 * _32;

    Float det = Determinant();

    if (!det) {
      return false;
    }

    Float inv_det = 1 / det;

    _11 = inv_det * A;
    _12 = inv_det * D;
    _21 = inv_det * B;
    _22 = inv_det * E;
    _31 = inv_det * C;
    _32 = inv_det * F;

    return true;
  }

  Float Determinant() const
  {
    return _11 * _22 - _12 * _21;
  }
  
  GFX2D_API static Matrix Rotation(Float aAngle);

  Matrix operator*(const Matrix &aMatrix) const
  {
    Matrix resultMatrix;

    resultMatrix._11 = this->_11 * aMatrix._11 + this->_12 * aMatrix._21;
    resultMatrix._12 = this->_11 * aMatrix._12 + this->_12 * aMatrix._22;
    resultMatrix._21 = this->_21 * aMatrix._11 + this->_22 * aMatrix._21;
    resultMatrix._22 = this->_21 * aMatrix._12 + this->_22 * aMatrix._22;
    resultMatrix._31 = this->_31 * aMatrix._11 + this->_32 * aMatrix._21 + aMatrix._31;
    resultMatrix._32 = this->_31 * aMatrix._12 + this->_32 * aMatrix._22 + aMatrix._32;

    return resultMatrix;
  }

  Matrix& operator*=(const Matrix &aMatrix)
  {
    Matrix resultMatrix = *this * aMatrix;
    return *this = resultMatrix;
  }

  /* Returns true if the other matrix is fuzzy-equal to this matrix.
   * Note that this isn't a cheap comparison!
   */
  bool operator==(const Matrix& other) const
  {
    return FuzzyEqual(_11, other._11) && FuzzyEqual(_12, other._12) &&
           FuzzyEqual(_21, other._21) && FuzzyEqual(_22, other._22) &&
           FuzzyEqual(_31, other._31) && FuzzyEqual(_32, other._32);
  }

  bool operator!=(const Matrix& other) const
  {
    return !(*this == other);
  }

  /* Returns true if the matrix is a rectilinear transformation (i.e.
   * grid-aligned rectangles are transformed to grid-aligned rectangles)
   */
  bool IsRectilinear() const {
    if (FuzzyEqual(_12, 0) && FuzzyEqual(_21, 0)) {
      return true;
    } else if (FuzzyEqual(_22, 0) && FuzzyEqual(_11, 0)) {
      return true;
    }

    return false;
  }

  /* Returns true if the matrix is an identity matrix.
   */
  bool IsIdentity() const
  {
    return _11 == 1.0f && _12 == 0.0f &&
           _21 == 0.0f && _22 == 1.0f &&
           _31 == 0.0f && _32 == 0.0f;
  }

  /* Returns true if the matrix is singular.
   */
  bool IsSingular() const
  {
    return Determinant() == 0;
  }

  GFX2D_API void NudgeToIntegers();

  bool IsTranslation() const
  {
    return FuzzyEqual(_11, 1.0f) && FuzzyEqual(_12, 0.0f) &&
           FuzzyEqual(_21, 0.0f) && FuzzyEqual(_22, 1.0f);
  }

  bool IsIntegerTranslation() const
  {
    return IsTranslation() &&
           FuzzyEqual(_31, floorf(_31 + 0.5f)) &&
           FuzzyEqual(_32, floorf(_32 + 0.5f));
  }

private:
  static bool FuzzyEqual(Float aV1, Float aV2) {
    // XXX - Check if fabs does the smart thing and just negates the sign bit.
    return fabs(aV2 - aV1) < 1e-6;
  }
};

class Matrix4x4
{
public:
  Matrix4x4()
    : _11(1.0f), _12(0.0f), _13(0.0f), _14(0.0f)
    , _21(0.0f), _22(1.0f), _23(0.0f), _24(0.0f)
    , _31(0.0f), _32(0.0f), _33(1.0f), _34(0.0f)
    , _41(0.0f), _42(0.0f), _43(0.0f), _44(1.0f)
  {}

  Float _11, _12, _13, _14;
  Float _21, _22, _23, _24;
  Float _31, _32, _33, _34;
  Float _41, _42, _43, _44;

  /**
   * Returns true if the matrix is isomorphic to a 2D affine transformation.
   */
  bool Is2D() const
  {
    if (_13 != 0.0f || _14 != 0.0f ||
        _23 != 0.0f || _24 != 0.0f ||
        _31 != 0.0f || _32 != 0.0f || _33 != 1.0f || _34 != 0.0f ||
        _43 != 0.0f || _44 != 1.0f) {
      return false;
    }
    return true;
  }

  Matrix As2D() const
  {
    MOZ_ASSERT(Is2D(), "Matrix is not a 2D affine transform");

    return Matrix(_11, _12, _21, _22, _41, _42);
  }

  bool Is2DIntegerTranslation() const
  {
    return Is2D() && As2D().IsIntegerTranslation();
  }

  // Apply a scale to this matrix. This scale will be applied -before- the
  // existing transformation of the matrix.
  Matrix4x4 &Scale(Float aX, Float aY, Float aZ)
  {
    _11 *= aX;
    _12 *= aX;
    _13 *= aX;
    _21 *= aY;
    _22 *= aY;
    _23 *= aY;
    _31 *= aZ;
    _32 *= aZ;
    _33 *= aZ;

    return *this;
  }
};

}
}

#endif /* MOZILLA_GFX_MATRIX_H_ */
