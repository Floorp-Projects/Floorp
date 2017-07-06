/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_RECT_H
#define GFX_RECT_H

#include "gfxTypes.h"
#include "gfxPoint.h"
#include "nsDebug.h"
#include "nsRect.h"
#include "mozilla/gfx/BaseMargin.h"
#include "mozilla/gfx/BaseRect.h"
#include "mozilla/gfx/MatrixFwd.h"
#include "mozilla/gfx/Rect.h"
#include "mozilla/Assertions.h"

struct gfxQuad;

typedef mozilla::gfx::MarginDouble gfxMargin;

struct gfxRect :
    public mozilla::gfx::BaseRect<gfxFloat, gfxRect, gfxPoint, gfxSize, gfxMargin> {
    typedef mozilla::gfx::BaseRect<gfxFloat, gfxRect, gfxPoint, gfxSize, gfxMargin> Super;

    gfxRect() : Super() {}
    gfxRect(const gfxPoint& aPos, const gfxSize& aSize) :
        Super(aPos, aSize) {}
    gfxRect(gfxFloat aX, gfxFloat aY, gfxFloat aWidth, gfxFloat aHeight) :
        Super(aX, aY, aWidth, aHeight) {}

    /**
     * Return true if all components of this rect are within
     * aEpsilon of integer coordinates, defined as
     *   |round(coord) - coord| <= |aEpsilon|
     * for x,y,width,height.
     */
    bool WithinEpsilonOfIntegerPixels(gfxFloat aEpsilon) const;

    gfxPoint AtCorner(mozilla::Corner corner) const {
        switch (corner) {
            case mozilla::eCornerTopLeft: return TopLeft();
            case mozilla::eCornerTopRight: return TopRight();
            case mozilla::eCornerBottomRight: return BottomRight();
            case mozilla::eCornerBottomLeft: return BottomLeft();
        }
        return gfxPoint(0.0, 0.0);
    }

    gfxPoint CCWCorner(mozilla::Side side) const {
        switch (side) {
            case mozilla::eSideTop: return TopLeft();
            case mozilla::eSideRight: return TopRight();
            case mozilla::eSideBottom: return BottomRight();
            case mozilla::eSideLeft: return BottomLeft();
        }
        MOZ_CRASH("Incomplete switch");
    }

    gfxPoint CWCorner(mozilla::Side side) const {
        switch (side) {
            case mozilla::eSideTop: return TopRight();
            case mozilla::eSideRight: return BottomRight();
            case mozilla::eSideBottom: return BottomLeft();
            case mozilla::eSideLeft: return TopLeft();
        }
        MOZ_CRASH("Incomplete switch");
    }

    /* Conditions this border to Cairo's max coordinate space.
     * The caller can check IsEmpty() after Condition() -- if it's TRUE,
     * the caller can possibly avoid doing any extra rendering.
     */
    void Condition();

    void Scale(gfxFloat k) {
        NS_ASSERTION(k >= 0.0, "Invalid (negative) scale factor");
        x *= k;
        y *= k;
        width *= k;
        height *= k;
    }

    void Scale(gfxFloat sx, gfxFloat sy) {
        NS_ASSERTION(sx >= 0.0, "Invalid (negative) scale factor");
        NS_ASSERTION(sy >= 0.0, "Invalid (negative) scale factor");
        x *= sx;
        y *= sy;
        width *= sx;
        height *= sy;
    }

    void ScaleInverse(gfxFloat k) {
        NS_ASSERTION(k > 0.0, "Invalid (negative) scale factor");
        x /= k;
        y /= k;
        width /= k;
        height /= k;
    }

    /*
     * Transform this rectangle with aMatrix, resulting in a gfxQuad.
     */
    gfxQuad TransformToQuad(const mozilla::gfx::Matrix4x4 &aMatrix) const;

    // Some temporary functions that we need until gfxRect gets turned into a
    // typedef for RectDouble. It would be simpler to put these in Matrix.h
    // but that code shouldn't #include gfxRect.h so we put it here instead.
    void TransformBy(const mozilla::gfx::MatrixDouble& aMatrix);
    void TransformBoundsBy(const mozilla::gfx::MatrixDouble& aMatrix);
};

#endif /* GFX_RECT_H */
