/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxRect.h"
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

    RoundedRect(gfxRect &aRect, RectCornerRadii &aCorners) : rect(aRect), corners(aCorners) { }
    void Deflate(gfxFloat aTopWidth, gfxFloat aBottomWidth, gfxFloat aLeftWidth, gfxFloat aRightWidth) {
        // deflate the internal rect
        rect.x += aLeftWidth;
        rect.y += aTopWidth;
        rect.width = std::max(0., rect.width - aLeftWidth - aRightWidth);
        rect.height = std::max(0., rect.height - aTopWidth - aBottomWidth);

        corners.radii[NS_CORNER_TOP_LEFT].width  = std::max(0., corners.radii[NS_CORNER_TOP_LEFT].width - aLeftWidth);
        corners.radii[NS_CORNER_TOP_LEFT].height = std::max(0., corners.radii[NS_CORNER_TOP_LEFT].height - aTopWidth);

        corners.radii[NS_CORNER_TOP_RIGHT].width  = std::max(0., corners.radii[NS_CORNER_TOP_RIGHT].width - aRightWidth);
        corners.radii[NS_CORNER_TOP_RIGHT].height = std::max(0., corners.radii[NS_CORNER_TOP_RIGHT].height - aTopWidth);

        corners.radii[NS_CORNER_BOTTOM_LEFT].width  = std::max(0., corners.radii[NS_CORNER_BOTTOM_LEFT].width - aLeftWidth);
        corners.radii[NS_CORNER_BOTTOM_LEFT].height = std::max(0., corners.radii[NS_CORNER_BOTTOM_LEFT].height - aBottomWidth);

        corners.radii[NS_CORNER_BOTTOM_RIGHT].width  = std::max(0., corners.radii[NS_CORNER_BOTTOM_RIGHT].width - aRightWidth);
        corners.radii[NS_CORNER_BOTTOM_RIGHT].height = std::max(0., corners.radii[NS_CORNER_BOTTOM_RIGHT].height - aBottomWidth);
    }
    gfxRect rect;
    RectCornerRadii corners;
};

} // namespace mozilla
