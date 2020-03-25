/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSRECTABSOLUTE_H
#define NSRECTABSOLUTE_H

#include "mozilla/gfx/RectAbsolute.h"
#include "nsCoord.h"
#include "nsPoint.h"
#include "nsRect.h"

struct nsRectAbsolute
    : public mozilla::gfx::BaseRectAbsolute<nscoord, nsRectAbsolute, nsPoint,
                                            nsRect> {
  typedef mozilla::gfx::BaseRectAbsolute<nscoord, nsRectAbsolute, nsPoint,
                                         nsRect>
      Super;

  nsRectAbsolute() : Super() {}
  nsRectAbsolute(nscoord aX1, nscoord aY1, nscoord aX2, nscoord aY2)
      : Super(aX1, aY1, aX2, aY2) {}

  MOZ_ALWAYS_INLINE nscoord SafeWidth() const {
    int64_t width = right;
    width -= left;
    return nscoord(
        std::min<int64_t>(std::numeric_limits<nscoord>::max(), width));
  }
  MOZ_ALWAYS_INLINE nscoord SafeHeight() const {
    int64_t height = bottom;
    height -= top;
    return nscoord(
        std::min<int64_t>(std::numeric_limits<nscoord>::max(), height));
  }

  nsRect ToNSRect() const {
    return nsRect(left, top, nscoord(SafeWidth()), nscoord(SafeHeight()));
  }

  [[nodiscard]] nsRectAbsolute UnsafeUnion(const nsRectAbsolute& aRect) const {
    return Super::Union(aRect);
  }

  void Inflate(const nsMargin& aMargin) {
    left -= aMargin.left;
    top -= aMargin.top;
    right += aMargin.right;
    bottom += aMargin.bottom;
  }
};

#endif /* NSRECTABSOLUTE_H */
