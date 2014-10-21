/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Matrix.h"
#include "Tools.h"
#include <algorithm>
#include <math.h>

#include "mozilla/FloatingPoint.h" // for UnspecifiedNaN

using namespace std;

namespace mozilla {
namespace gfx {

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

Rect
Matrix4x4::TransformBounds(const Rect& aRect) const
{
  Point quad[4];
  Float min_x, max_x;
  Float min_y, max_y;

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

  return Rect(min_x, min_y, max_x - min_x, max_y - min_y);
}

Point4D ComputePerspectivePlaneIntercept(const Point4D& aFirst,
                                         const Point4D& aSecond)
{
  // FIXME: See bug 1035611
  // Since we can't easily deal with points as w=0 (since we divide by w), we
  // approximate this by finding a point with w just greater than 0. Unfortunately
  // this is a tradeoff between accuracy and floating point precision.

  // We want to interpolate aFirst and aSecond to find a point as close to
  // the positive side of the w=0 plane as possible.

  // Since we know what we want the w component to be, we can rearrange the
  // interpolation equation and solve for t.
  float w = 0.00001f;
  float t = (w - aFirst.w) / (aSecond.w - aFirst.w);

  // Use t to find the remainder of the components
  return aFirst + (aSecond - aFirst) * t;
}

Rect Matrix4x4::ProjectRectBounds(const Rect& aRect) const
{
  Point4D points[4];

  points[0] = ProjectPoint(aRect.TopLeft());
  points[1] = ProjectPoint(aRect.TopRight());
  points[2] = ProjectPoint(aRect.BottomRight());
  points[3] = ProjectPoint(aRect.BottomLeft());

  Float min_x = std::numeric_limits<Float>::max();
  Float min_y = std::numeric_limits<Float>::max();
  Float max_x = -std::numeric_limits<Float>::max();
  Float max_y = -std::numeric_limits<Float>::max();

  bool foundPoint = false;
  for (int i=0; i<4; i++) {
    // Only use points that exist above the w=0 plane
    if (points[i].HasPositiveWCoord()) {
      foundPoint = true;
      Point point2d = points[i].As2DPoint();
      min_x = min<Float>(point2d.x, min_x);
      max_x = max<Float>(point2d.x, max_x);
      min_y = min<Float>(point2d.y, min_y);
      max_y = max<Float>(point2d.y, max_y);
    }

    int next = (i == 3) ? 0 : i + 1;
    if (points[i].HasPositiveWCoord() != points[next].HasPositiveWCoord()) {
      // If the line between two points crosses the w=0 plane, then interpolate a point
      // as close to the w=0 plane as possible and use that instead.
      Point4D intercept = ComputePerspectivePlaneIntercept(points[i], points[next]);

      Point point2d = intercept.As2DPoint();
      min_x = min<Float>(point2d.x, min_x);
      max_x = max<Float>(point2d.x, max_x);
      min_y = min<Float>(point2d.y, min_y);
      max_y = max<Float>(point2d.y, max_y);
    }
  }

  if (!foundPoint) {
    return Rect(0, 0, 0, 0);
  }

  return Rect(min_x, min_y, max_x - min_x, max_y - min_y);
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

}
}
