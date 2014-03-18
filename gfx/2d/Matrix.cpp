/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Matrix.h"
#include "Tools.h"
#include <math.h>

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

void
Matrix::NudgeToIntegers()
{
  NudgeToInteger(&_11);
  NudgeToInteger(&_12);
  NudgeToInteger(&_21);
  NudgeToInteger(&_22);
  NudgeToInteger(&_31);
  NudgeToInteger(&_32);
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

}
}
