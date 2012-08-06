/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxMatrix.h"
#include "gfx3DMatrix.h"
#include <math.h>
#include <algorithm>
using namespace std;

/* Force small values to zero.  We do this to avoid having sin(360deg)
 * evaluate to a tiny but nonzero value.
 */
static double FlushToZero(double aVal)
{
  if (-FLT_EPSILON < aVal && aVal < FLT_EPSILON)
    return 0.0f;
  else
    return aVal;
}

/* Computes tan(aTheta).  For values of aTheta such that tan(aTheta) is
 * undefined or very large, SafeTangent returns a manageably large value
 * of the correct sign.
 */
static double SafeTangent(double aTheta)
{
  const double kEpsilon = 0.0001;

  /* tan(theta) = sin(theta)/cos(theta); problems arise when
   * cos(theta) is too close to zero.  Limit cos(theta) to the
   * range [-1, -epsilon] U [epsilon, 1].
   */
  double sinTheta = sin(aTheta);
  double cosTheta = cos(aTheta);

  if (cosTheta >= 0 && cosTheta < kEpsilon)
    cosTheta = kEpsilon;
  else if (cosTheta < 0 && cosTheta >= -kEpsilon)
    cosTheta = -kEpsilon;

  return FlushToZero(sinTheta / cosTheta);
}

gfx3DMatrix::gfx3DMatrix(void)
{
  _11 = _22 = _33 = _44 = 1.0f;
  _12 = _13 = _14 = 0.0f;
  _21 = _23 = _24 = 0.0f;
  _31 = _32 = _34 = 0.0f;
  _41 = _42 = _43 = 0.0f;
}

gfx3DMatrix
gfx3DMatrix::operator*(const gfx3DMatrix &aMatrix) const
{
  if (Is2D() && aMatrix.Is2D()) {
    return Multiply2D(aMatrix);
  }

  gfx3DMatrix matrix;

  matrix._11 = _11 * aMatrix._11 + _12 * aMatrix._21 + _13 * aMatrix._31 + _14 * aMatrix._41;
  matrix._21 = _21 * aMatrix._11 + _22 * aMatrix._21 + _23 * aMatrix._31 + _24 * aMatrix._41;
  matrix._31 = _31 * aMatrix._11 + _32 * aMatrix._21 + _33 * aMatrix._31 + _34 * aMatrix._41;
  matrix._41 = _41 * aMatrix._11 + _42 * aMatrix._21 + _43 * aMatrix._31 + _44 * aMatrix._41;
  matrix._12 = _11 * aMatrix._12 + _12 * aMatrix._22 + _13 * aMatrix._32 + _14 * aMatrix._42;
  matrix._22 = _21 * aMatrix._12 + _22 * aMatrix._22 + _23 * aMatrix._32 + _24 * aMatrix._42;
  matrix._32 = _31 * aMatrix._12 + _32 * aMatrix._22 + _33 * aMatrix._32 + _34 * aMatrix._42;
  matrix._42 = _41 * aMatrix._12 + _42 * aMatrix._22 + _43 * aMatrix._32 + _44 * aMatrix._42;
  matrix._13 = _11 * aMatrix._13 + _12 * aMatrix._23 + _13 * aMatrix._33 + _14 * aMatrix._43;
  matrix._23 = _21 * aMatrix._13 + _22 * aMatrix._23 + _23 * aMatrix._33 + _24 * aMatrix._43;
  matrix._33 = _31 * aMatrix._13 + _32 * aMatrix._23 + _33 * aMatrix._33 + _34 * aMatrix._43;
  matrix._43 = _41 * aMatrix._13 + _42 * aMatrix._23 + _43 * aMatrix._33 + _44 * aMatrix._43;
  matrix._14 = _11 * aMatrix._14 + _12 * aMatrix._24 + _13 * aMatrix._34 + _14 * aMatrix._44;
  matrix._24 = _21 * aMatrix._14 + _22 * aMatrix._24 + _23 * aMatrix._34 + _24 * aMatrix._44;
  matrix._34 = _31 * aMatrix._14 + _32 * aMatrix._24 + _33 * aMatrix._34 + _34 * aMatrix._44;
  matrix._44 = _41 * aMatrix._14 + _42 * aMatrix._24 + _43 * aMatrix._34 + _44 * aMatrix._44;

  return matrix;
}

gfx3DMatrix&
gfx3DMatrix::operator*=(const gfx3DMatrix &aMatrix)
{
  return *this = *this * aMatrix;
}

gfx3DMatrix
gfx3DMatrix::Multiply2D(const gfx3DMatrix &aMatrix) const
{
  gfx3DMatrix matrix;

  matrix._11 = _11 * aMatrix._11 + _12 * aMatrix._21;
  matrix._21 = _21 * aMatrix._11 + _22 * aMatrix._21;
  matrix._41 = _41 * aMatrix._11 + _42 * aMatrix._21 + aMatrix._41;
  matrix._12 = _11 * aMatrix._12 + _12 * aMatrix._22;
  matrix._22 = _21 * aMatrix._12 + _22 * aMatrix._22;
  matrix._42 = _41 * aMatrix._12 + _42 * aMatrix._22 + aMatrix._42;

  return matrix;
}

bool
gfx3DMatrix::operator==(const gfx3DMatrix& o) const
{
  // XXX would be nice to memcmp here, but that breaks IEEE 754 semantics
  return _11 == o._11 && _12 == o._12 && _13 == o._13 && _14 == o._14 &&
         _21 == o._21 && _22 == o._22 && _23 == o._23 && _24 == o._24 &&
         _31 == o._31 && _32 == o._32 && _33 == o._33 && _34 == o._34 &&
         _41 == o._41 && _42 == o._42 && _43 == o._43 && _44 == o._44;
}

gfx3DMatrix&
gfx3DMatrix::operator/=(const gfxFloat scalar)
{
  _11 /= scalar;
  _12 /= scalar;
  _13 /= scalar;
  _14 /= scalar;
  _21 /= scalar;
  _22 /= scalar;
  _23 /= scalar;
  _24 /= scalar;
  _31 /= scalar;
  _32 /= scalar;
  _33 /= scalar;
  _34 /= scalar;
  _41 /= scalar;
  _42 /= scalar;
  _43 /= scalar;
  _44 /= scalar;
  return *this;
}

gfx3DMatrix
gfx3DMatrix::From2D(const gfxMatrix &aMatrix)
{
  gfx3DMatrix matrix;
  matrix._11 = (float)aMatrix.xx;
  matrix._12 = (float)aMatrix.yx;
  matrix._21 = (float)aMatrix.xy;
  matrix._22 = (float)aMatrix.yy;
  matrix._41 = (float)aMatrix.x0;
  matrix._42 = (float)aMatrix.y0;
  return matrix;
}

bool
gfx3DMatrix::IsIdentity() const
{
  return _11 == 1.0f && _12 == 0.0f && _13 == 0.0f && _14 == 0.0f &&
         _21 == 0.0f && _22 == 1.0f && _23 == 0.0f && _24 == 0.0f &&
         _31 == 0.0f && _32 == 0.0f && _33 == 1.0f && _34 == 0.0f &&
         _41 == 0.0f && _42 == 0.0f && _43 == 0.0f && _44 == 1.0f;
}

void
gfx3DMatrix::Translate(const gfxPoint3D& aPoint)
{
    _41 += aPoint.x * _11 + aPoint.y * _21 + aPoint.z * _31;
    _42 += aPoint.x * _12 + aPoint.y * _22 + aPoint.z * _32;
    _43 += aPoint.x * _13 + aPoint.y * _23 + aPoint.z * _33;
    _44 += aPoint.x * _14 + aPoint.y * _24 + aPoint.z * _34;
}

void
gfx3DMatrix::TranslatePost(const gfxPoint3D& aPoint)
{
    _11 += _14 * aPoint.x;
    _21 += _24 * aPoint.x;
    _31 += _34 * aPoint.x;
    _41 += _44 * aPoint.x;
    _12 += _14 * aPoint.y;
    _22 += _24 * aPoint.y;
    _32 += _34 * aPoint.y;
    _42 += _44 * aPoint.y;
    _13 += _14 * aPoint.z;
    _23 += _24 * aPoint.z;
    _33 += _34 * aPoint.z;
    _43 += _44 * aPoint.z;
}

void
gfx3DMatrix::ScalePost(float aX, float aY, float aZ)
{
  _11 *= aX;
  _21 *= aX;
  _31 *= aX;
  _41 *= aX;

  _12 *= aY;
  _22 *= aY;
  _32 *= aY;
  _42 *= aY;

  _13 *= aZ;
  _23 *= aZ;
  _33 *= aZ;
  _43 *= aZ;
}

void
gfx3DMatrix::SkewXY(double aSkew)
{
    (*this)[1] += (*this)[0] * aSkew;
}

void 
gfx3DMatrix::SkewXZ(double aSkew)
{
    (*this)[2] += (*this)[0] * aSkew;
}

void
gfx3DMatrix::SkewYZ(double aSkew)
{
    (*this)[2] += (*this)[1] * aSkew;
}

void
gfx3DMatrix::Scale(float aX, float aY, float aZ)
{
    (*this)[0] *= aX;
    (*this)[1] *= aY;
    (*this)[2] *= aZ;
}

void
gfx3DMatrix::Perspective(float aDepth)
{
  NS_ASSERTION(aDepth > 0.0f, "Perspective must be positive!");
  _31 += -1.0/aDepth * _41;
  _32 += -1.0/aDepth * _42;
  _33 += -1.0/aDepth * _43;
  _34 += -1.0/aDepth * _44;
}

void gfx3DMatrix::SkewXY(double aXSkew, double aYSkew)
{
  float tanX = SafeTangent(aXSkew);
  float tanY = SafeTangent(aYSkew);
  float temp;

  temp = _11;
  _11 += tanY * _21;
  _21 += tanX * temp;

  temp = _12;
  _12 += tanY * _22;
  _22 += tanX * temp;
  
  temp = _13;
  _13 += tanY * _23;
  _23 += tanX * temp;
  
  temp = _14;
  _14 += tanY * _24;
  _24 += tanX * temp;
}

void
gfx3DMatrix::RotateX(double aTheta)
{
  double cosTheta = FlushToZero(cos(aTheta));
  double sinTheta = FlushToZero(sin(aTheta));

  float temp;

  temp = _21;
  _21 = cosTheta * _21 + sinTheta * _31;
  _31 = -sinTheta * temp + cosTheta * _31;
  
  temp = _22;
  _22 = cosTheta * _22 + sinTheta * _32;
  _32 = -sinTheta * temp + cosTheta * _32;
  
  temp = _23;
  _23 = cosTheta * _23 + sinTheta * _33;
  _33 = -sinTheta * temp + cosTheta * _33;
  
  temp = _24;
  _24 = cosTheta * _24 + sinTheta * _34;
  _34 = -sinTheta * temp + cosTheta * _34;
}

void
gfx3DMatrix::RotateY(double aTheta)
{
  double cosTheta = FlushToZero(cos(aTheta));
  double sinTheta = FlushToZero(sin(aTheta));

  float temp;

  temp = _11;
  _11 = cosTheta * _11 + -sinTheta * _31;
  _31 = sinTheta * temp + cosTheta * _31;
  
  temp = _12;
  _12 = cosTheta * _12 + -sinTheta * _32;
  _32 = sinTheta * temp + cosTheta * _32;
  
  temp = _13;
  _13 = cosTheta * _13 + -sinTheta * _33;
  _33 = sinTheta * temp + cosTheta * _33;
  
  temp = _14;
  _14 = cosTheta * _14 + -sinTheta * _34;
  _34 = sinTheta * temp + cosTheta * _34;
}

void
gfx3DMatrix::RotateZ(double aTheta)
{
  double cosTheta = FlushToZero(cos(aTheta));
  double sinTheta = FlushToZero(sin(aTheta));

  float temp;

  temp = _11;
  _11 = cosTheta * _11 + sinTheta * _21;
  _21 = -sinTheta * temp + cosTheta * _21;
  
  temp = _12;
  _12 = cosTheta * _12 + sinTheta * _22;
  _22 = -sinTheta * temp + cosTheta * _22;
  
  temp = _13;
  _13 = cosTheta * _13 + sinTheta * _23;
  _23 = -sinTheta * temp + cosTheta * _23;
  
  temp = _14;
  _14 = cosTheta * _14 + sinTheta * _24;
  _24 = -sinTheta * temp + cosTheta * _24;
}

void 
gfx3DMatrix::PreMultiply(const gfx3DMatrix& aOther)
{
  *this = aOther * (*this);
}

void
gfx3DMatrix::PreMultiply(const gfxMatrix& aOther)
{
  gfx3DMatrix temp;
  temp._11 = aOther.xx * _11 + aOther.yx * _21;
  temp._21 = aOther.xy * _11 + aOther.yy * _21;
  temp._31 = _31;
  temp._41 = aOther.x0 * _11 + aOther.y0 * _21 + _41;
  temp._12 = aOther.xx * _12 + aOther.yx * _22;
  temp._22 = aOther.xy * _12 + aOther.yy * _22;
  temp._32 = _32;
  temp._42 = aOther.x0 * _12 + aOther.y0 * _22 + _42;
  temp._13 = aOther.xx * _13 + aOther.yx * _23;
  temp._23 = aOther.xy * _13 + aOther.yy * _23;
  temp._33 = _33;
  temp._43 = aOther.x0 * _13 + aOther.y0 * _23 + _43;
  temp._14 = aOther.xx * _14 + aOther.yx * _24;
  temp._24 = aOther.xy * _14 + aOther.yy * _24;
  temp._34 = _34;
  temp._44 = aOther.x0 * _14 + aOther.y0 * _24 + _44;

  *this = temp;
}

gfx3DMatrix
gfx3DMatrix::Translation(float aX, float aY, float aZ)
{
  gfx3DMatrix matrix;

  matrix._41 = aX;
  matrix._42 = aY;
  matrix._43 = aZ;
  return matrix;
}

gfx3DMatrix
gfx3DMatrix::Translation(const gfxPoint3D& aPoint)
{
  gfx3DMatrix matrix;

  matrix._41 = aPoint.x;
  matrix._42 = aPoint.y;
  matrix._43 = aPoint.z;
  return matrix;
}

gfx3DMatrix
gfx3DMatrix::ScalingMatrix(float aFactor)
{
  gfx3DMatrix matrix;

  matrix._11 = matrix._22 = matrix._33 = aFactor;
  return matrix;
}

gfx3DMatrix
gfx3DMatrix::ScalingMatrix(float aX, float aY, float aZ)
{
  gfx3DMatrix matrix;

  matrix._11 = aX;
  matrix._22 = aY;
  matrix._33 = aZ;

  return matrix;
}

gfxFloat
gfx3DMatrix::Determinant() const
{
  return _14 * _23 * _32 * _41
       - _13 * _24 * _32 * _41
       - _14 * _22 * _33 * _41
       + _12 * _24 * _33 * _41
       + _13 * _22 * _34 * _41
       - _12 * _23 * _34 * _41
       - _14 * _23 * _31 * _42
       + _13 * _24 * _31 * _42
       + _14 * _21 * _33 * _42
       - _11 * _24 * _33 * _42
       - _13 * _21 * _34 * _42
       + _11 * _23 * _34 * _42
       + _14 * _22 * _31 * _43
       - _12 * _24 * _31 * _43
       - _14 * _21 * _32 * _43
       + _11 * _24 * _32 * _43
       + _12 * _21 * _34 * _43
       - _11 * _22 * _34 * _43
       - _13 * _22 * _31 * _44
       + _12 * _23 * _31 * _44
       + _13 * _21 * _32 * _44
       - _11 * _23 * _32 * _44
       - _12 * _21 * _33 * _44
       + _11 * _22 * _33 * _44;
}

gfxFloat
gfx3DMatrix::Determinant3x3() const
{
    return _11 * (_22 * _33 - _23 * _32) +
           _12 * (_23 * _31 - _33 * _21) +
           _13 * (_21 * _32 - _22 * _31);
}

gfx3DMatrix
gfx3DMatrix::Inverse3x3() const
{
    gfxFloat det = Determinant3x3();
    if (det == 0.0) {
        return *this;
    }

    gfxFloat detInv = 1/det;
    gfx3DMatrix temp;

    temp._11 = (_22 * _33 - _23 * _32) * detInv;
    temp._12 = (_13 * _32 - _12 * _33) * detInv;
    temp._13 = (_12 * _23 - _13 * _22) * detInv;
    temp._21 = (_23 * _31 - _33 * _21) * detInv;
    temp._22 = (_11 * _33 - _13 * _31) * detInv;
    temp._23 = (_13 * _21 - _11 * _23) * detInv;
    temp._31 = (_21 * _32 - _22 * _31) * detInv;
    temp._32 = (_31 * _12 - _11 * _32) * detInv;
    temp._33 = (_11 * _22 - _12 * _21) * detInv;
    return temp;
}

bool
gfx3DMatrix::IsSingular() const
{
  return Determinant() == 0.0;
}

gfx3DMatrix
gfx3DMatrix::Inverse() const
{
  if (TransposedVector(3) == gfxPointH3D(0, 0, 0, 1)) {
    /** 
     * When the matrix contains no perspective, the inverse
     * is the same as the 3x3 inverse of the rotation components
     * multiplied by the inverse of the translation components.
     * Doing these steps separately is faster and more numerically
     * stable.
     *
     * Inverse of the translation matrix is just negating
     * the values.
     */
    gfx3DMatrix matrix3 = Inverse3x3();
    matrix3.Translate(gfxPoint3D(-_41, -_42, -_43));
    return matrix3;
 }

  gfxFloat det = Determinant();
  if (det == 0.0) {
    return *this;
  }

  gfx3DMatrix temp;

  temp._11 = _23*_34*_42 - _24*_33*_42 + 
             _24*_32*_43 - _22*_34*_43 - 
             _23*_32*_44 + _22*_33*_44;
  temp._12 = _14*_33*_42 - _13*_34*_42 -
             _14*_32*_43 + _12*_34*_43 +
             _13*_32*_44 - _12*_33*_44;
  temp._13 = _13*_24*_42 - _14*_23*_42 +
             _14*_22*_43 - _12*_24*_43 -
             _13*_22*_44 + _12*_23*_44;
  temp._14 = _14*_23*_32 - _13*_24*_32 -
             _14*_22*_33 + _12*_24*_33 +
             _13*_22*_34 - _12*_23*_34;
  temp._21 = _24*_33*_41 - _23*_34*_41 -
             _24*_31*_43 + _21*_34*_43 +
             _23*_31*_44 - _21*_33*_44;
  temp._22 = _13*_34*_41 - _14*_33*_41 +
             _14*_31*_43 - _11*_34*_43 -
             _13*_31*_44 + _11*_33*_44;
  temp._23 = _14*_23*_41 - _13*_24*_41 -
             _14*_21*_43 + _11*_24*_43 +
             _13*_21*_44 - _11*_23*_44;
  temp._24 = _13*_24*_31 - _14*_23*_31 +
             _14*_21*_33 - _11*_24*_33 -
             _13*_21*_34 + _11*_23*_34;
  temp._31 = _22*_34*_41 - _24*_32*_41 +
             _24*_31*_42 - _21*_34*_42 -
             _22*_31*_44 + _21*_32*_44;
  temp._32 = _14*_32*_41 - _12*_34*_41 -
             _14*_31*_42 + _11*_34*_42 +
             _12*_31*_44 - _11*_32*_44;
  temp._33 = _12*_24*_41 - _14*_22*_41 +
             _14*_21*_42 - _11*_24*_42 -
             _12*_21*_44 + _11*_22*_44;
  temp._34 = _14*_22*_31 - _12*_24*_31 -
             _14*_21*_32 + _11*_24*_32 +
             _12*_21*_34 - _11*_22*_34;
  temp._41 = _23*_32*_41 - _22*_33*_41 -
             _23*_31*_42 + _21*_33*_42 +
             _22*_31*_43 - _21*_32*_43;
  temp._42 = _12*_33*_41 - _13*_32*_41 +
             _13*_31*_42 - _11*_33*_42 -
             _12*_31*_43 + _11*_32*_43;
  temp._43 = _13*_22*_41 - _12*_23*_41 -
             _13*_21*_42 + _11*_23*_42 +
             _12*_21*_43 - _11*_22*_43;
  temp._44 = _12*_23*_31 - _13*_22*_31 +
             _13*_21*_32 - _11*_23*_32 -
             _12*_21*_33 + _11*_22*_33;

  temp /= det;
  return temp;
}

gfx3DMatrix&
gfx3DMatrix::Normalize()
{
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            (*this)[i][j] /= (*this)[3][3];
       }
    }
    return *this;
}

gfx3DMatrix&
gfx3DMatrix::Transpose()
{
    *this = Transposed();
    return *this;
}

gfx3DMatrix
gfx3DMatrix::Transposed() const
{
    gfx3DMatrix temp;
    for (int i = 0; i < 4; i++) {
        temp[i] = TransposedVector(i);
    }
    return temp;
}

gfxPoint
gfx3DMatrix::Transform(const gfxPoint& point) const
{
  gfxPoint3D vec3d(point.x, point.y, 0);
  vec3d = Transform3D(vec3d);
  return gfxPoint(vec3d.x, vec3d.y);
}

gfxPoint3D
gfx3DMatrix::Transform3D(const gfxPoint3D& point) const
{
  gfxFloat x = point.x * _11 + point.y * _21 + point.z * _31 + _41;
  gfxFloat y = point.x * _12 + point.y * _22 + point.z * _32 + _42;
  gfxFloat z = point.x * _13 + point.y * _23 + point.z * _33 + _43;
  gfxFloat w = point.x * _14 + point.y * _24 + point.z * _34 + _44;

  x /= w;
  y /= w;
  z /= w;

  return gfxPoint3D(x, y, z);
}

gfxPointH3D
gfx3DMatrix::Transform4D(const gfxPointH3D& aPoint) const
{
    gfxFloat x = aPoint.x * _11 + aPoint.y * _21 + aPoint.z * _31 + aPoint.w * _41;
    gfxFloat y = aPoint.x * _12 + aPoint.y * _22 + aPoint.z * _32 + aPoint.w * _42;
    gfxFloat z = aPoint.x * _13 + aPoint.y * _23 + aPoint.z * _33 + aPoint.w * _43;
    gfxFloat w = aPoint.x * _14 + aPoint.y * _24 + aPoint.z * _34 + aPoint.w * _44;

    return gfxPointH3D(x, y, z, w);
}

gfxPointH3D
gfx3DMatrix::TransposeTransform4D(const gfxPointH3D& aPoint) const
{
    gfxFloat x = aPoint.x * _11 + aPoint.y * _12 + aPoint.z * _13 + aPoint.w * _14;
    gfxFloat y = aPoint.x * _21 + aPoint.y * _22 + aPoint.z * _23 + aPoint.w * _24;
    gfxFloat z = aPoint.x * _31 + aPoint.y * _32 + aPoint.z * _33 + aPoint.w * _34;
    gfxFloat w = aPoint.x * _41 + aPoint.y * _42 + aPoint.z * _43 + aPoint.w * _44;

    return gfxPointH3D(x, y, z, w);
}

gfxRect
gfx3DMatrix::TransformBounds(const gfxRect& rect) const
{
  gfxPoint points[4];

  points[0] = Transform(rect.TopLeft());
  points[1] = Transform(gfxPoint(rect.X() + rect.Width(), rect.Y()));
  points[2] = Transform(gfxPoint(rect.X(), rect.Y() + rect.Height()));
  points[3] = Transform(gfxPoint(rect.X() + rect.Width(),
                                 rect.Y() + rect.Height()));

  gfxFloat min_x, max_x;
  gfxFloat min_y, max_y;

  min_x = max_x = points[0].x;
  min_y = max_y = points[0].y;

  for (int i=1; i<4; i++) {
    min_x = min(points[i].x, min_x);
    max_x = max(points[i].x, max_x);
    min_y = min(points[i].y, min_y);
    max_y = max(points[i].y, max_y);
  }

  return gfxRect(min_x, min_y, max_x - min_x, max_y - min_y);
}

gfxQuad 
gfx3DMatrix::TransformRect(const gfxRect& aRect) const
{
  gfxPoint points[4];

  points[0] = Transform(aRect.TopLeft());
  points[1] = Transform(gfxPoint(aRect.X() + aRect.Width(), aRect.Y()));
  points[2] = Transform(gfxPoint(aRect.X() + aRect.Width(),
                                 aRect.Y() + aRect.Height()));
  points[3] = Transform(gfxPoint(aRect.X(), aRect.Y() + aRect.Height()));
  
  // Could this ever result in lines that intersect? I don't think so.
  return gfxQuad(points[0], points[1], points[2], points[3]);
}

bool
gfx3DMatrix::Is2D() const
{
  if (_13 != 0.0f || _14 != 0.0f ||
      _23 != 0.0f || _24 != 0.0f ||
      _31 != 0.0f || _32 != 0.0f || _33 != 1.0f || _34 != 0.0f ||
      _43 != 0.0f || _44 != 1.0f) {
    return false;
  }
  return true;
}

bool
gfx3DMatrix::Is2D(gfxMatrix* aMatrix) const
{
  if (!Is2D()) {
    return false;
  }
  if (aMatrix) {
    aMatrix->xx = _11;
    aMatrix->yx = _12;
    aMatrix->xy = _21;
    aMatrix->yy = _22;
    aMatrix->x0 = _41;
    aMatrix->y0 = _42;
  }
  return true;
}

bool
gfx3DMatrix::CanDraw2D(gfxMatrix* aMatrix) const
{
  if (_14 != 0.0f ||
      _24 != 0.0f ||
      _44 != 1.0f) {
    return false;
  }
  if (aMatrix) {
    aMatrix->xx = _11;
    aMatrix->yx = _12;
    aMatrix->xy = _21;
    aMatrix->yy = _22;
    aMatrix->x0 = _41;
    aMatrix->y0 = _42;
  }
  return true;
}

gfx3DMatrix&
gfx3DMatrix::ProjectTo2D()
{
  _31 = 0.0f;
  _32 = 0.0f;
  _13 = 0.0f; 
  _23 = 0.0f; 
  _33 = 1.0f; 
  _43 = 0.0f; 
  _34 = 0.0f;
  return *this;
}

gfxPoint gfx3DMatrix::ProjectPoint(const gfxPoint& aPoint) const
{
  // Define a ray of the form P + Ut where t is a real number
  // w is assumed to always be 1 when transforming 3d points with our
  // 4x4 matrix.
  // p is our click point, q is another point on the same ray.
  // 
  // Note: since the transformation is a general projective transformation and is not
  // necessarily affine, we can't just take a unit vector u, back-transform it, and use
  // it as unit vector on the back-transformed ray. Instead, we really must take two points
  // on the ray and back-transform them.
  gfxPoint3D p(aPoint.x, aPoint.y, 0);
  gfxPoint3D q(aPoint.x, aPoint.y, 1);

  // Back transform the vectors (using w = 1) and normalize
  // back into 3d vectors by dividing by the w component.
  gfxPoint3D pback = Transform3D(p);
  gfxPoint3D qback = Transform3D(q);
  gfxPoint3D uback = qback - pback;

  // Find the point where the back transformed line intersects z=0
  // and find t.
  
  float t = -pback.z / uback.z;

  gfxPoint result(pback.x + t*uback.x, pback.y + t*uback.y);

  return result;
}

gfxRect gfx3DMatrix::ProjectRectBounds(const gfxRect& aRect) const
{
  gfxPoint points[4];

  points[0] = ProjectPoint(aRect.TopLeft());
  points[1] = ProjectPoint(gfxPoint(aRect.X() + aRect.Width(), aRect.Y()));
  points[2] = ProjectPoint(gfxPoint(aRect.X(), aRect.Y() + aRect.Height()));
  points[3] = ProjectPoint(gfxPoint(aRect.X() + aRect.Width(),
                                    aRect.Y() + aRect.Height()));

  gfxFloat min_x, max_x;
  gfxFloat min_y, max_y;

  min_x = max_x = points[0].x;
  min_y = max_y = points[0].y;

  for (int i=1; i<4; i++) {
    min_x = min(points[i].x, min_x);
    max_x = max(points[i].x, max_x);
    min_y = min(points[i].y, min_y);
    max_y = max(points[i].y, max_y);
  }

  return gfxRect(min_x, min_y, max_x - min_x, max_y - min_y);
}

gfxPoint3D gfx3DMatrix::GetNormalVector() const
{
  // Define a plane in transformed space as the transformations
  // of 3 points on the z=0 screen plane.
  gfxPoint3D a = Transform3D(gfxPoint3D(0, 0, 0));
  gfxPoint3D b = Transform3D(gfxPoint3D(0, 1, 0));
  gfxPoint3D c = Transform3D(gfxPoint3D(1, 0, 0));

  // Convert to two vectors on the surface of the plane.
  gfxPoint3D ab = b - a;
  gfxPoint3D ac = c - a;

  return ac.CrossProduct(ab);
}

bool gfx3DMatrix::IsBackfaceVisible() const
{
  // Inverse()._33 < 0;
  gfxFloat det = Determinant();
  float _33 = _12*_24*_41 - _14*_22*_41 +
              _14*_21*_42 - _11*_24*_42 -
              _12*_21*_44 + _11*_22*_44;
  return (_33 * det) < 0;
}

