/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ROUNDED_RECT_H
#define ROUNDED_RECT_H

#include "gfxRect.h"
#include "gfxTypes.h"
#include "mozilla/gfx/PathHelpers.h"

namespace mozilla {
/* A rounded rectangle abstraction.
 *
 * This can represent a rectangle with a different pair of radii on each corner.
 *
 * Note: CoreGraphics and Direct2D only support rounded rectangle with the same
 * radii on all corners. However, supporting CSS's border-radius requires the extra flexibility. */
struct RoundedRect {
    typedef mozilla::gfx::RectCornerRadii RectCornerRadii;

    RoundedRect(const gfxRect& aRect, const RectCornerRadii& aCorners)
      : rect(aRect)
      , corners(aCorners)
    {
    }

    void Deflate(gfxFloat aTopWidth, gfxFloat aBottomWidth, gfxFloat aLeftWidth, gfxFloat aRightWidth) {
        // deflate the internal rect
        rect.SetRect(rect.X() + aLeftWidth,
                     rect.Y() + aTopWidth,
                     std::max(0., rect.Width() - aLeftWidth - aRightWidth),
                     std::max(0., rect.Height() - aTopWidth - aBottomWidth));

        corners.radii[mozilla::eCornerTopLeft].width =
            std::max(0., corners.radii[mozilla::eCornerTopLeft].width - aLeftWidth);
        corners.radii[mozilla::eCornerTopLeft].height =
            std::max(0., corners.radii[mozilla::eCornerTopLeft].height - aTopWidth);

        corners.radii[mozilla::eCornerTopRight].width =
            std::max(0., corners.radii[mozilla::eCornerTopRight].width - aRightWidth);
        corners.radii[mozilla::eCornerTopRight].height =
            std::max(0., corners.radii[mozilla::eCornerTopRight].height - aTopWidth);

        corners.radii[mozilla::eCornerBottomLeft].width =
            std::max(0., corners.radii[mozilla::eCornerBottomLeft].width - aLeftWidth);
        corners.radii[mozilla::eCornerBottomLeft].height =
            std::max(0., corners.radii[mozilla::eCornerBottomLeft].height - aBottomWidth);

        corners.radii[mozilla::eCornerBottomRight].width =
            std::max(0., corners.radii[mozilla::eCornerBottomRight].width - aRightWidth);
        corners.radii[mozilla::eCornerBottomRight].height =
            std::max(0., corners.radii[mozilla::eCornerBottomRight].height - aBottomWidth);
    }
    gfxRect rect;
    RectCornerRadii corners;
};

} // namespace mozilla

#endif
