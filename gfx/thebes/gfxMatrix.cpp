/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxMatrix.h"
#include "cairo.h"
#include "mozilla/gfx/Tools.h"
#include "mozilla/gfx/Matrix.h" // for Matrix4x4

#define CAIRO_MATRIX(x) reinterpret_cast<cairo_matrix_t*>((x))
#define CONST_CAIRO_MATRIX(x) reinterpret_cast<const cairo_matrix_t*>((x))

const gfxMatrix&
gfxMatrix::Reset()
{
    cairo_matrix_init_identity(CAIRO_MATRIX(this));
    return *this;
}

bool
gfxMatrix::Invert()
{
    return cairo_matrix_invert(CAIRO_MATRIX(this)) == CAIRO_STATUS_SUCCESS;
}

gfxMatrix&
gfxMatrix::Scale(gfxFloat x, gfxFloat y)
{
    cairo_matrix_scale(CAIRO_MATRIX(this), x, y);
    return *this;
}

gfxMatrix&
gfxMatrix::Translate(const gfxPoint& pt)
{
    cairo_matrix_translate(CAIRO_MATRIX(this), pt.x, pt.y);
    return *this;
}

gfxMatrix&
gfxMatrix::Rotate(gfxFloat radians)
{
    cairo_matrix_rotate(CAIRO_MATRIX(this), radians);
    return *this;
}

const gfxMatrix&
gfxMatrix::operator *= (const gfxMatrix& m)
{
    cairo_matrix_multiply(CAIRO_MATRIX(this), CAIRO_MATRIX(this), CONST_CAIRO_MATRIX(&m));
    return *this;
}

gfxMatrix&
gfxMatrix::PreMultiply(const gfxMatrix& m)
{
    cairo_matrix_multiply(CAIRO_MATRIX(this), CONST_CAIRO_MATRIX(&m), CAIRO_MATRIX(this));
    return *this;
}

/* static */ gfxMatrix
gfxMatrix::Rotation(gfxFloat aAngle)
{
    gfxMatrix newMatrix;

    gfxFloat s = sin(aAngle);
    gfxFloat c = cos(aAngle);

    newMatrix._11 = c;
    newMatrix._12 = s;
    newMatrix._21 = -s;
    newMatrix._22 = c;

    return newMatrix;
}

gfxPoint
gfxMatrix::Transform(const gfxPoint& point) const
{
    gfxPoint ret = point;
    cairo_matrix_transform_point(CONST_CAIRO_MATRIX(this), &ret.x, &ret.y);
    return ret;
}

gfxSize
gfxMatrix::Transform(const gfxSize& size) const
{
    gfxSize ret = size;
    cairo_matrix_transform_distance(CONST_CAIRO_MATRIX(this), &ret.width, &ret.height);
    return ret;
}

gfxRect
gfxMatrix::Transform(const gfxRect& rect) const
{
    return gfxRect(Transform(rect.TopLeft()), Transform(rect.Size()));
}

gfxRect
gfxMatrix::TransformBounds(const gfxRect& rect) const
{
    /* Code taken from cairo-matrix.c, _cairo_matrix_transform_bounding_box isn't public */
    int i;
    double quad_x[4], quad_y[4];
    double min_x, max_x;
    double min_y, max_y;

    quad_x[0] = rect.X();
    quad_y[0] = rect.Y();
    cairo_matrix_transform_point (CONST_CAIRO_MATRIX(this), &quad_x[0], &quad_y[0]);

    quad_x[1] = rect.XMost();
    quad_y[1] = rect.Y();
    cairo_matrix_transform_point (CONST_CAIRO_MATRIX(this), &quad_x[1], &quad_y[1]);

    quad_x[2] = rect.X();
    quad_y[2] = rect.YMost();
    cairo_matrix_transform_point (CONST_CAIRO_MATRIX(this), &quad_x[2], &quad_y[2]);

    quad_x[3] = rect.XMost();
    quad_y[3] = rect.YMost();
    cairo_matrix_transform_point (CONST_CAIRO_MATRIX(this), &quad_x[3], &quad_y[3]);

    min_x = max_x = quad_x[0];
    min_y = max_y = quad_y[0];

    for (i = 1; i < 4; i++) {
        if (quad_x[i] < min_x)
            min_x = quad_x[i];
        if (quad_x[i] > max_x)
            max_x = quad_x[i];

        if (quad_y[i] < min_y)
            min_y = quad_y[i];
        if (quad_y[i] > max_y)
            max_y = quad_y[i];
    }

    return gfxRect(min_x, min_y, max_x - min_x, max_y - min_y);
}


static void NudgeToInteger(double *aVal)
{
    float f = float(*aVal);
    mozilla::gfx::NudgeToInteger(&f);
    *aVal = f;
}

gfxMatrix&
gfxMatrix::NudgeToIntegers(void)
{
    NudgeToInteger(&_11);
    NudgeToInteger(&_21);
    NudgeToInteger(&_12);
    NudgeToInteger(&_22);
    NudgeToInteger(&_31);
    NudgeToInteger(&_32);
    return *this;
}

mozilla::gfx::Matrix4x4
gfxMatrix::operator *(const mozilla::gfx::Matrix4x4& aMatrix) const
{
  Matrix4x4 resultMatrix;

  resultMatrix._11 = _11 * aMatrix._11 + _12 * aMatrix._21;
  resultMatrix._12 = _11 * aMatrix._12 + _12 * aMatrix._22;
  resultMatrix._13 = _11 * aMatrix._13 + _12 * aMatrix._23;
  resultMatrix._14 = _11 * aMatrix._14 + _12 * aMatrix._24;

  resultMatrix._21 = _21 * aMatrix._11 + _22 * aMatrix._21;
  resultMatrix._22 = _21 * aMatrix._12 + _22 * aMatrix._22;
  resultMatrix._23 = _21 * aMatrix._13 + _22 * aMatrix._23;
  resultMatrix._24 = _21 * aMatrix._14 + _22 * aMatrix._24;

  resultMatrix._31 = aMatrix._31;
  resultMatrix._32 = aMatrix._32;
  resultMatrix._33 = aMatrix._33;
  resultMatrix._34 = aMatrix._34;

  resultMatrix._41 = _31 * aMatrix._11 + _32 * aMatrix._21 + aMatrix._41;
  resultMatrix._42 = _31 * aMatrix._12 + _32 * aMatrix._22 + aMatrix._42;
  resultMatrix._43 = _31 * aMatrix._13 + _32 * aMatrix._23 + aMatrix._43;
  resultMatrix._44 = _31 * aMatrix._14 + _32 * aMatrix._24 + aMatrix._44;

  return resultMatrix;
}
