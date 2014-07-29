/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_POINT_H
#define GFX_POINT_H

#include "nsMathUtils.h"
#include "mozilla/gfx/BaseSize.h"
#include "mozilla/gfx/BasePoint.h"
#include "nsSize.h"
#include "nsPoint.h"

#include "gfxTypes.h"

struct gfxSize : public mozilla::gfx::BaseSize<gfxFloat, gfxSize> {
    typedef mozilla::gfx::BaseSize<gfxFloat, gfxSize> Super;

    gfxSize() : Super() {}
    gfxSize(gfxFloat aWidth, gfxFloat aHeight) : Super(aWidth, aHeight) {}
    MOZ_IMPLICIT gfxSize(const nsIntSize& aSize) : Super(aSize.width, aSize.height) {}
};

struct gfxPoint : public mozilla::gfx::BasePoint<gfxFloat, gfxPoint> {
    typedef mozilla::gfx::BasePoint<gfxFloat, gfxPoint> Super;

    gfxPoint() : Super() {}
    gfxPoint(gfxFloat aX, gfxFloat aY) : Super(aX, aY) {}
    MOZ_IMPLICIT gfxPoint(const nsIntPoint& aPoint) : Super(aPoint.x, aPoint.y) {}

    bool WithinEpsilonOf(const gfxPoint& aPoint, gfxFloat aEpsilon) {
        return fabs(aPoint.x - x) < aEpsilon && fabs(aPoint.y - y) < aEpsilon;
    }
};

inline gfxPoint
operator*(const gfxPoint& aPoint, const gfxSize& aSize)
{
  return gfxPoint(aPoint.x * aSize.width, aPoint.y * aSize.height);
}

inline gfxPoint
operator/(const gfxPoint& aPoint, const gfxSize& aSize)
{
  return gfxPoint(aPoint.x / aSize.width, aPoint.y / aSize.height);
}

inline gfxSize
operator/(gfxFloat aValue, const gfxSize& aSize)
{
  return gfxSize(aValue / aSize.width, aValue / aSize.height);
}

#endif /* GFX_POINT_H */
