/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxRect.h"

#include "nsMathUtils.h"

#include "mozilla/gfx/Matrix.h"

#include "gfxQuad.h"

gfxQuad
gfxRect::TransformToQuad(const mozilla::gfx::Matrix4x4 &aMatrix) const
{
  gfxPoint points[4];

  points[0] = aMatrix.TransformPoint(TopLeft());
  points[1] = aMatrix.TransformPoint(TopRight());
  points[2] = aMatrix.TransformPoint(BottomRight());
  points[3] = aMatrix.TransformPoint(BottomLeft());

  // Could this ever result in lines that intersect? I don't think so.
  return gfxQuad(points[0], points[1], points[2], points[3]);
}

void
gfxRect::TransformBy(const mozilla::gfx::MatrixDouble& aMatrix)
{
  *this = gfxRect(aMatrix.TransformPoint(TopLeft()),
                  aMatrix.TransformSize(Size()));
}

void
gfxRect::TransformBoundsBy(const mozilla::gfx::MatrixDouble& aMatrix)
{
  RectDouble tmp(x, y, width, height);
  tmp = aMatrix.TransformBounds(tmp);
  x = tmp.x;
  y = tmp.y;
  width = tmp.width;
  height = tmp.height;
}

static bool
WithinEpsilonOfInteger(gfxFloat aX, gfxFloat aEpsilon)
{
    return fabs(NS_round(aX) - aX) <= fabs(aEpsilon);
}

bool
gfxRect::WithinEpsilonOfIntegerPixels(gfxFloat aEpsilon) const
{
    NS_ASSERTION(-0.5 < aEpsilon && aEpsilon < 0.5, "Nonsense epsilon value");
    return (WithinEpsilonOfInteger(x, aEpsilon) &&
            WithinEpsilonOfInteger(y, aEpsilon) &&
            WithinEpsilonOfInteger(width, aEpsilon) &&
            WithinEpsilonOfInteger(height, aEpsilon));
}

/* Clamp r to CAIRO_COORD_MIN .. CAIRO_COORD_MAX
 * these are to be device coordinates.
 *
 * Cairo is currently using 24.8 fixed point,
 * so -2^24 .. 2^24-1 is our valid
 */

#define CAIRO_COORD_MAX (16777215.0)
#define CAIRO_COORD_MIN (-16777216.0)

void
gfxRect::Condition()
{
    // if either x or y is way out of bounds;
    // note that we don't handle negative w/h here
    if (x > CAIRO_COORD_MAX) {
        x = CAIRO_COORD_MAX;
        width = 0.0;
    } 

    if (y > CAIRO_COORD_MAX) {
        y = CAIRO_COORD_MAX;
        height = 0.0;
    }

    if (x < CAIRO_COORD_MIN) {
        width += x - CAIRO_COORD_MIN;
        if (width < 0.0)
            width = 0.0;
        x = CAIRO_COORD_MIN;
    }

    if (y < CAIRO_COORD_MIN) {
        height += y - CAIRO_COORD_MIN;
        if (height < 0.0)
            height = 0.0;
        y = CAIRO_COORD_MIN;
    }

    if (x + width > CAIRO_COORD_MAX) {
        width = CAIRO_COORD_MAX - x;
    }

    if (y + height > CAIRO_COORD_MAX) {
        height = CAIRO_COORD_MAX - y;
    }
}

