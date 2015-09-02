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

namespace {

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

} // namespace

namespace mozilla {
namespace gfx {

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

std::ostream&
operator<<(std::ostream& aStream, const Matrix4x4& aMatrix)
{
  const Float *f = &aMatrix._11;
  aStream << "[ " << f[0] << " "  << f[1] << " " << f[2] << " " << f[3] << " ;" << std::endl; f += 4;
  aStream << "  " << f[0] << " "  << f[1] << " " << f[2] << " " << f[3] << " ;" << std::endl; f += 4;
  aStream << "  " << f[0] << " "  << f[1] << " " << f[2] << " " << f[3] << " ;" << std::endl; f += 4;
  aStream << "  " << f[0] << " "  << f[1] << " " << f[2] << " " << f[3] << " ]" << std::endl;
  return aStream;
}

Matrix
Matrix::Rotation(Float aAngle)
{
  Matrix newMatrix;

  Float s = sin(aAngle);
  Float c = cos(aAngle);

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

template<class F>
RectTyped<UnknownUnits, F>
Matrix4x4::TransformBounds(const RectTyped<UnknownUnits, F>& aRect) const
{
  Point4DTyped<UnknownUnits, F> verts[4];
  verts[0] = *this * Point4DTyped<UnknownUnits, F>(aRect.x, aRect.y, 0.0, 1.0);
  verts[1] = *this * Point4DTyped<UnknownUnits, F>(aRect.XMost(), aRect.y, 0.0, 1.0);
  verts[2] = *this * Point4DTyped<UnknownUnits, F>(aRect.XMost(), aRect.YMost(), 0.0, 1.0);
  verts[3] = *this * Point4DTyped<UnknownUnits, F>(aRect.x, aRect.YMost(), 0.0, 1.0);

  PointTyped<UnknownUnits, F> quad[4];
  F min_x, max_x;
  F min_y, max_y;

  quad[0] = *this * aRect.TopLeft();
  quad[1] = *this * aRect.TopRight();
  quad[2] = *this * aRect.BottomLeft();
  quad[3] = *this * aRect.BottomRight();

  min_x = max_x = quad[0].x;
  min_y = max_y = quad[0].y;

  for (int i = 1; i < 4; i++) {
    if (quad[i].x < min_x) {
      min_x = quad[i].x;
    }
    if (quad[i].x > max_x) {
      max_x = quad[i].x;
    }

    if (quad[i].y < min_y) {
      min_y = quad[i].y;
    }
    if (quad[i].y > max_y) {
      max_y = quad[i].y;
    }
  }

  return RectTyped<UnknownUnits, F>(min_x, min_y, max_x - min_x, max_y - min_y);
}

Point4D ComputePerspectivePlaneIntercept(const Point4D& aFirst,
                                         const Point4D& aSecond)
{
  // This function will always return a point with a w value of 0.
  // The X, Y, and Z components will point towards an infinite vanishing
  // point.

  // We want to interpolate aFirst and aSecond to find the point intersecting
  // with the w=0 plane.

  // Since we know what we want the w component to be, we can rearrange the
  // interpolation equation and solve for t.
  float t = -aFirst.w / (aSecond.w - aFirst.w);

  // Use t to find the remainder of the components
  return aFirst + (aSecond - aFirst) * t;
}

Rect Matrix4x4::ProjectRectBounds(const Rect& aRect, const Rect &aClip) const
{
  // This function must never return std::numeric_limits<Float>::max() or any
  // other arbitrary large value in place of inifinity.  This often occurs when
  // aRect is an inversed projection matrix or when aRect is transformed to be
  // partly behind and in front of the camera (w=0 plane in homogenous
  // coordinates) - See Bug 1035611

  // Some call-sites will call RoundGfxRectToAppRect which clips both the
  // extents and dimensions of the rect to be bounded by nscoord_MAX.
  // If we return a Rect that, when converted to nscoords, has a width or height
  // greater than nscoord_MAX, RoundGfxRectToAppRect will clip the overflow
  // off both the min and max end of the rect after clipping the extents of the
  // rect, resulting in a translation of the rect towards the infinite end.

  // The bounds returned by ProjectRectBounds are expected to be clipped only on
  // the edges beyond the bounds of the coordinate system; otherwise, the
  // clipped bounding box would be smaller than the correct one and result
  // bugs such as incorrect culling (eg. Bug 1073056)

  // To address this without requiring all code to work in homogenous
  // coordinates or interpret infinite values correctly, a specialized
  // clipping function is integrated into ProjectRectBounds.

  // Callers should pass an aClip value that represents the extents to clip
  // the result to, in the same coordinate system as aRect.
  Point4D points[4];

  points[0] = ProjectPoint(aRect.TopLeft());
  points[1] = ProjectPoint(aRect.TopRight());
  points[2] = ProjectPoint(aRect.BottomRight());
  points[3] = ProjectPoint(aRect.BottomLeft());

  Float min_x = std::numeric_limits<Float>::max();
  Float min_y = std::numeric_limits<Float>::max();
  Float max_x = -std::numeric_limits<Float>::max();
  Float max_y = -std::numeric_limits<Float>::max();

  for (int i=0; i<4; i++) {
    // Only use points that exist above the w=0 plane
    if (points[i].HasPositiveWCoord()) {
      Point point2d = aClip.ClampPoint(points[i].As2DPoint());
      min_x = std::min<Float>(point2d.x, min_x);
      max_x = std::max<Float>(point2d.x, max_x);
      min_y = std::min<Float>(point2d.y, min_y);
      max_y = std::max<Float>(point2d.y, max_y);
    }

    int next = (i == 3) ? 0 : i + 1;
    if (points[i].HasPositiveWCoord() != points[next].HasPositiveWCoord()) {
      // If the line between two points crosses the w=0 plane, then interpolate
      // to find the point of intersection with the w=0 plane and use that
      // instead.
      Point4D intercept = ComputePerspectivePlaneIntercept(points[i], points[next]);
      // Since intercept.w will always be 0 here, we interpret x,y,z as a
      // direction towards an infinite vanishing point.
      if (intercept.x < 0.0f) {
        min_x = aClip.x;
      } else if (intercept.x > 0.0f) {
        max_x = aClip.XMost();
      }
      if (intercept.y < 0.0f) {
        min_y = aClip.y;
      } else if (intercept.y > 0.0f) {
        max_y = aClip.YMost();
      }
    }
  }

  if (max_x < min_x || max_y < min_y) {
    return Rect(0, 0, 0, 0);
  }

  return Rect(min_x, min_y, max_x - min_x, max_y - min_y);
}

template<class F>
size_t
Matrix4x4::TransformAndClipRect(const RectTyped<UnknownUnits, F>& aRect,
                                const RectTyped<UnknownUnits, F>& aClip,
                                PointTyped<UnknownUnits, F>* aVerts) const
{
  // Initialize a double-buffered array of points in homogenous space with
  // the input rectangle, aRect.
  Point4DTyped<UnknownUnits, F> points[2][kTransformAndClipRectMaxVerts];
  Point4DTyped<UnknownUnits, F>* dstPoint = points[0];
  *dstPoint++ = *this * Point4DTyped<UnknownUnits, F>(aRect.x, aRect.y, 0, 1);
  *dstPoint++ = *this * Point4DTyped<UnknownUnits, F>(aRect.XMost(), aRect.y, 0, 1);
  *dstPoint++ = *this * Point4DTyped<UnknownUnits, F>(aRect.XMost(), aRect.YMost(), 0, 1);
  *dstPoint++ = *this * Point4DTyped<UnknownUnits, F>(aRect.x, aRect.YMost(), 0, 1);

  // View frustum clipping planes are described as normals originating from
  // the 0,0,0,0 origin.
  Point4DTyped<UnknownUnits, F> planeNormals[4];
  planeNormals[0] = Point4DTyped<UnknownUnits, F>(1.0, 0.0, 0.0, -aClip.x);
  planeNormals[1] = Point4DTyped<UnknownUnits, F>(-1.0, 0.0, 0.0, aClip.XMost());
  planeNormals[2] = Point4DTyped<UnknownUnits, F>(0.0, 1.0, 0.0, -aClip.y);
  planeNormals[3] = Point4DTyped<UnknownUnits, F>(0.0, -1.0, 0.0, aClip.YMost());

  // Iterate through each clipping plane and clip the polygon.
  // In each pass, we double buffer, alternating between points[0] and
  // points[1].
  for (int plane=0; plane < 4; plane++) {
    planeNormals[plane].Normalize();

    Point4DTyped<UnknownUnits, F>* srcPoint = points[plane & 1];
    Point4DTyped<UnknownUnits, F>* srcPointEnd = dstPoint;
    dstPoint = points[~plane & 1];

    Point4DTyped<UnknownUnits, F>* prevPoint = srcPointEnd - 1;
    F prevDot = planeNormals[plane].DotProduct(*prevPoint);
    while (srcPoint < srcPointEnd) {
      F nextDot = planeNormals[plane].DotProduct(*srcPoint);

      if ((nextDot >= 0.0) != (prevDot >= 0.0)) {
        // An intersection with the clipping plane has been detected.
        // Interpolate to find the intersecting point and emit it.
        F t = -prevDot / (nextDot - prevDot);
        *dstPoint++ = *srcPoint * t + *prevPoint * (1.0 - t);
      }

      if (nextDot >= 0.0) {
        // Emit any source points that are on the positive side of the
        // clipping plane.
        *dstPoint++ = *srcPoint;
      }

      prevPoint = srcPoint++;
      prevDot = nextDot;
    }
  }

  size_t dstPointCount = 0;
  size_t srcPointCount = dstPoint - points[0];
  for (Point4DTyped<UnknownUnits, F>* srcPoint = points[0]; srcPoint < points[0] + srcPointCount; srcPoint++) {

    PointTyped<UnknownUnits, F> p;
    if (srcPoint->w == 0.0) {
      // If a point lies on the intersection of the clipping planes at
      // (0,0,0,0), we must avoid a division by zero w component.
      p = PointTyped<UnknownUnits, F>(0.0, 0.0);
    } else {
      p = srcPoint->As2DPoint();
    }
    // Emit only unique points
    if (dstPointCount == 0 || p != aVerts[dstPointCount - 1]) {
      aVerts[dstPointCount++] = p;
    }
  }

  return dstPointCount;
}

bool
Matrix4x4::Invert()
{
  Float det = Determinant();
  if (!det) {
    return false;
  }

  Matrix4x4 result;
  result._11 = _23 * _34 * _42 - _24 * _33 * _42 + _24 * _32 * _43 - _22 * _34 * _43 - _23 * _32 * _44 + _22 * _33 * _44;
  result._12 = _14 * _33 * _42 - _13 * _34 * _42 - _14 * _32 * _43 + _12 * _34 * _43 + _13 * _32 * _44 - _12 * _33 * _44;
  result._13 = _13 * _24 * _42 - _14 * _23 * _42 + _14 * _22 * _43 - _12 * _24 * _43 - _13 * _22 * _44 + _12 * _23 * _44;
  result._14 = _14 * _23 * _32 - _13 * _24 * _32 - _14 * _22 * _33 + _12 * _24 * _33 + _13 * _22 * _34 - _12 * _23 * _34;
  result._21 = _24 * _33 * _41 - _23 * _34 * _41 - _24 * _31 * _43 + _21 * _34 * _43 + _23 * _31 * _44 - _21 * _33 * _44;
  result._22 = _13 * _34 * _41 - _14 * _33 * _41 + _14 * _31 * _43 - _11 * _34 * _43 - _13 * _31 * _44 + _11 * _33 * _44;
  result._23 = _14 * _23 * _41 - _13 * _24 * _41 - _14 * _21 * _43 + _11 * _24 * _43 + _13 * _21 * _44 - _11 * _23 * _44;
  result._24 = _13 * _24 * _31 - _14 * _23 * _31 + _14 * _21 * _33 - _11 * _24 * _33 - _13 * _21 * _34 + _11 * _23 * _34;
  result._31 = _22 * _34 * _41 - _24 * _32 * _41 + _24 * _31 * _42 - _21 * _34 * _42 - _22 * _31 * _44 + _21 * _32 * _44;
  result._32 = _14 * _32 * _41 - _12 * _34 * _41 - _14 * _31 * _42 + _11 * _34 * _42 + _12 * _31 * _44 - _11 * _32 * _44;
  result._33 = _12 * _24 * _41 - _14 * _22 * _41 + _14 * _21 * _42 - _11 * _24 * _42 - _12 * _21 * _44 + _11 * _22 * _44;
  result._34 = _14 * _22 * _31 - _12 * _24 * _31 - _14 * _21 * _32 + _11 * _24 * _32 + _12 * _21 * _34 - _11 * _22 * _34;
  result._41 = _23 * _32 * _41 - _22 * _33 * _41 - _23 * _31 * _42 + _21 * _33 * _42 + _22 * _31 * _43 - _21 * _32 * _43;
  result._42 = _12 * _33 * _41 - _13 * _32 * _41 + _13 * _31 * _42 - _11 * _33 * _42 - _12 * _31 * _43 + _11 * _32 * _43;
  result._43 = _13 * _22 * _41 - _12 * _23 * _41 - _13 * _21 * _42 + _11 * _23 * _42 + _12 * _21 * _43 - _11 * _22 * _43;
  result._44 = _12 * _23 * _31 - _13 * _22 * _31 + _13 * _21 * _32 - _11 * _23 * _32 - _12 * _21 * _33 + _11 * _22 * _33;

  result._11 /= det;
  result._12 /= det;
  result._13 /= det;
  result._14 /= det;
  result._21 /= det;
  result._22 /= det;
  result._23 /= det;
  result._24 /= det;
  result._31 /= det;
  result._32 /= det;
  result._33 /= det;
  result._34 /= det;
  result._41 /= det;
  result._42 /= det;
  result._43 /= det;
  result._44 /= det;
  *this = result;

  return true;
}

void
Matrix4x4::SetNAN()
{
  _11 = UnspecifiedNaN<Float>();
  _21 = UnspecifiedNaN<Float>();
  _31 = UnspecifiedNaN<Float>();
  _41 = UnspecifiedNaN<Float>();
  _12 = UnspecifiedNaN<Float>();
  _22 = UnspecifiedNaN<Float>();
  _32 = UnspecifiedNaN<Float>();
  _42 = UnspecifiedNaN<Float>();
  _13 = UnspecifiedNaN<Float>();
  _23 = UnspecifiedNaN<Float>();
  _33 = UnspecifiedNaN<Float>();
  _43 = UnspecifiedNaN<Float>();
  _14 = UnspecifiedNaN<Float>();
  _24 = UnspecifiedNaN<Float>();
  _34 = UnspecifiedNaN<Float>();
  _44 = UnspecifiedNaN<Float>();
}

void
Matrix4x4::SetRotationFromQuaternion(const Quaternion& q)
{
  const Float x2 = q.x + q.x, y2 = q.y + q.y, z2 = q.z + q.z;
  const Float xx = q.x * x2, xy = q.x * y2, xz = q.x * z2;
  const Float yy = q.y * y2, yz = q.y * z2, zz = q.z * z2;
  const Float wx = q.w * x2, wy = q.w * y2, wz = q.w * z2;

  _11 = 1.0f - (yy + zz);
  _21 = xy + wz;
  _31 = xz - wy;
  _41 = 0.0f;

  _12 = xy - wz;
  _22 = 1.0f - (xx + zz);
  _32 = yz + wx;
  _42 = 0.0f;

  _13 = xz + wy;
  _23 = yz - wx;
  _33 = 1.0f - (xx + yy);
  _43 = 0.0f;

  _14 = _42 = _43 = 0.0f;
  _44 = 1.0f;
}

void
Matrix4x4::SkewXY(double aXSkew, double aYSkew)
{
  // XXX Is double precision really necessary here
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
Matrix4x4::RotateX(double aTheta)
{
  // XXX Is double precision really necessary here
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
Matrix4x4::RotateY(double aTheta)
{
  // XXX Is double precision really necessary here
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
Matrix4x4::RotateZ(double aTheta)
{
  // XXX Is double precision really necessary here
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
Matrix4x4::Perspective(float aDepth)
{
  MOZ_ASSERT(aDepth > 0.0f, "Perspective must be positive!");
  _31 += -1.0/aDepth * _41;
  _32 += -1.0/aDepth * _42;
  _33 += -1.0/aDepth * _43;
  _34 += -1.0/aDepth * _44;
}

Point3D
Matrix4x4::GetNormalVector() const
{
  // Define a plane in transformed space as the transformations
  // of 3 points on the z=0 screen plane.
  Point3D a = *this * Point3D(0, 0, 0);
  Point3D b = *this * Point3D(0, 1, 0);
  Point3D c = *this * Point3D(1, 0, 0);

  // Convert to two vectors on the surface of the plane.
  Point3D ab = b - a;
  Point3D ac = c - a;

  return ac.CrossProduct(ab);
}

template<class F>
RectTyped<UnknownUnits, F>
Matrix4x4::TransformAndClipBounds(const RectTyped<UnknownUnits, F>& aRect,
                                  const RectTyped<UnknownUnits, F>& aClip) const
{
  PointTyped<UnknownUnits, F> verts[kTransformAndClipRectMaxVerts];
  size_t vertCount = TransformAndClipRect(aRect, aClip, verts);

  F min_x = std::numeric_limits<F>::max();
  F min_y = std::numeric_limits<F>::max();
  F max_x = -std::numeric_limits<F>::max();
  F max_y = -std::numeric_limits<F>::max();
  for (size_t i=0; i < vertCount; i++) {
    min_x = std::min(min_x, verts[i].x);
    max_x = std::max(max_x, verts[i].x);
    min_y = std::min(min_y, verts[i].y);
    max_y = std::max(max_y, verts[i].y);
  }

  if (max_x < min_x || max_y < min_y) {
    return RectTyped<UnknownUnits, F>(0, 0, 0, 0);
  }

  return RectTyped<UnknownUnits, F>(min_x, min_y, max_x - min_x, max_y - min_y);

}

// Explicit template instantiation for float and double precision
template
size_t
Matrix4x4::TransformAndClipRect(const Rect& aRect, const Rect& aClip,
                                Point* aVerts) const;

template
size_t
Matrix4x4::TransformAndClipRect(const RectDouble& aRect,
                                const RectDouble& aClip,
                                PointDouble* aVerts) const;

template
Rect
Matrix4x4::TransformAndClipBounds(const Rect& aRect,
                                  const Rect& aClip) const;

template
RectDouble
Matrix4x4::TransformAndClipBounds(const RectDouble& aRect,
                                  const RectDouble& aClip) const;

template
Rect
Matrix4x4::TransformBounds(const Rect& aRect) const;

template
RectDouble
Matrix4x4::TransformBounds(const RectDouble& aRect) const;

} // namespace gfx
} // namespace mozilla
