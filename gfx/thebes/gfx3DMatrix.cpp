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
 * The Original Code is Oracle Corporation code.
 *
 * The Initial Developer of the Original Code is Oracle Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bas Schouten <bschouten@mozilla.com>
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

#include "gfxMatrix.h"
#include "gfx3DMatrix.h"
#include <math.h>
#include <algorithm>
using namespace std;

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

PRBool
gfx3DMatrix::IsIdentity() const
{
  return _11 == 1.0f && _12 == 0.0f && _13 == 0.0f && _14 == 0.0f &&
         _21 == 0.0f && _22 == 1.0f && _23 == 0.0f && _24 == 0.0f &&
         _31 == 0.0f && _32 == 0.0f && _33 == 1.0f && _34 == 0.0f &&
         _41 == 0.0f && _42 == 0.0f && _43 == 0.0f && _44 == 1.0f;
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
gfx3DMatrix::Scale(float aFactor)
{
  gfx3DMatrix matrix;

  matrix._11 = matrix._22 = matrix._33 = aFactor;
  return matrix;
}

gfx3DMatrix
gfx3DMatrix::Scale(float aX, float aY, float aZ)
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

PRBool
gfx3DMatrix::IsSingular() const
{
  return Determinant() == 0.0;
}

gfx3DMatrix&
gfx3DMatrix::Invert()
{
  gfxFloat det = Determinant();
  if (det == 0.0) {
    return *this;
  }

  gfx3DMatrix temp = *this;

  _11 = temp._23*temp._34*temp._42 - temp._24*temp._33*temp._42 
      + temp._24*temp._32*temp._43 - temp._22*temp._34*temp._43 
      - temp._23*temp._32*temp._44 + temp._22*temp._33*temp._44;
  _12 = temp._14*temp._33*temp._42 - temp._13*temp._34*temp._42 
      - temp._14*temp._32*temp._43 + temp._12*temp._34*temp._43 
      + temp._13*temp._32*temp._44 - temp._12*temp._33*temp._44;
  _13 = temp._13*temp._24*temp._42 - temp._14*temp._23*temp._42 
      + temp._14*temp._22*temp._43 - temp._12*temp._24*temp._43 
      - temp._13*temp._22*temp._44 + temp._12*temp._23*temp._44;
  _14 = temp._14*temp._23*temp._32 - temp._13*temp._24*temp._32 
      - temp._14*temp._22*temp._33 + temp._12*temp._24*temp._33 
      + temp._13*temp._22*temp._34 - temp._12*temp._23*temp._34;
  _21 = temp._24*temp._33*temp._41 - temp._23*temp._34*temp._41 
      - temp._24*temp._31*temp._43 + temp._21*temp._34*temp._43 
      + temp._23*temp._31*temp._44 - temp._21*temp._33*temp._44;
  _22 = temp._13*temp._34*temp._41 - temp._14*temp._33*temp._41 
      + temp._14*temp._31*temp._43 - temp._11*temp._34*temp._43 
      - temp._13*temp._31*temp._44 + temp._11*temp._33*temp._44;
  _23 = temp._14*temp._23*temp._41 - temp._13*temp._24*temp._41 
      - temp._14*temp._21*temp._43 + temp._11*temp._24*temp._43 
      + temp._13*temp._21*temp._44 - temp._11*temp._23*temp._44;
  _24 = temp._13*temp._24*temp._31 - temp._14*temp._23*temp._31 
      + temp._14*temp._21*temp._33 - temp._11*temp._24*temp._33 
      - temp._13*temp._21*temp._34 + temp._11*temp._23*temp._34;
  _31 = temp._22*temp._34*temp._41 - temp._24*temp._32*temp._41 
      + temp._24*temp._31*temp._42 - temp._21*temp._34*temp._42 
      - temp._22*temp._31*temp._44 + temp._21*temp._32*temp._44;
  _32 = temp._14*temp._32*temp._41 - temp._12*temp._34*temp._41 
      - temp._14*temp._31*temp._42 + temp._11*temp._34*temp._42 
      + temp._12*temp._31*temp._44 - temp._11*temp._32*temp._44;
  _33 = temp._12*temp._24*temp._41 - temp._14*temp._22*temp._41 
      + temp._14*temp._21*temp._42 - temp._11*temp._24*temp._42 
      - temp._12*temp._21*temp._44 + temp._11*temp._22*temp._44;
  _34 = temp._14*temp._22*temp._31 - temp._12*temp._24*temp._31 
      - temp._14*temp._21*temp._32 + temp._11*temp._24*temp._32 
      + temp._12*temp._21*temp._34 - temp._11*temp._22*temp._34;
  _41 = temp._23*temp._32*temp._41 - temp._22*temp._33*temp._41 
      - temp._23*temp._31*temp._42 + temp._21*temp._33*temp._42 
      + temp._22*temp._31*temp._43 - temp._21*temp._32*temp._43;
  _42 = temp._12*temp._33*temp._41 - temp._13*temp._32*temp._41 
      + temp._13*temp._31*temp._42 - temp._11*temp._33*temp._42 
      - temp._12*temp._31*temp._43 + temp._11*temp._32*temp._43;
  _43 = temp._13*temp._22*temp._41 - temp._12*temp._23*temp._41 
      - temp._13*temp._21*temp._42 + temp._11*temp._23*temp._42 
      + temp._12*temp._21*temp._43 - temp._11*temp._22*temp._43;
  _44 = temp._12*temp._23*temp._31 - temp._13*temp._22*temp._31 
      + temp._13*temp._21*temp._32 - temp._11*temp._23*temp._32 
      - temp._12*temp._21*temp._33 + temp._11*temp._22*temp._33;

  *this /= det;
  return *this;
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

PRBool
gfx3DMatrix::Is2D(gfxMatrix* aMatrix) const
{
  if (_13 != 0.0f || _14 != 0.0f ||
      _23 != 0.0f || _24 != 0.0f ||
      _31 != 0.0f || _32 != 0.0f || _33 != 1.0f || _34 != 0.0f ||
      _43 != 0.0f || _44 != 1.0f) {
    return PR_FALSE;
  }
  if (aMatrix) {
    aMatrix->xx = _11;
    aMatrix->yx = _12;
    aMatrix->xy = _21;
    aMatrix->yy = _22;
    aMatrix->x0 = _41;
    aMatrix->y0 = _42;
  }
  return PR_TRUE;
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
