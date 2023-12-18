/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSSIZE_H
#define NSSIZE_H

#include "nsCoord.h"
#include "mozilla/gfx/BaseSize.h"
#include "mozilla/gfx/Point.h"

// Maximum allowable size
inline constexpr nscoord NS_MAXSIZE = nscoord_MAX;

typedef mozilla::gfx::IntSize nsIntSize;

struct nsSize : public mozilla::gfx::BaseSize<nscoord, nsSize> {
  typedef mozilla::gfx::BaseSize<nscoord, nsSize> Super;

  constexpr nsSize() {}
  constexpr nsSize(nscoord aWidth, nscoord aHeight) : Super(aWidth, aHeight) {}

  inline mozilla::gfx::IntSize ScaleToNearestPixels(
      float aXScale, float aYScale, nscoord aAppUnitsPerPixel) const;
  inline mozilla::gfx::IntSize ToNearestPixels(nscoord aAppUnitsPerPixel) const;

  /**
   * Return this size scaled to a different appunits per pixel (APP) ratio.
   * @param aFromAPP the APP to scale from
   * @param aToAPP the APP to scale to
   */
  [[nodiscard]] inline nsSize ScaleToOtherAppUnits(int32_t aFromAPP,
                                                   int32_t aToAPP) const;
};

inline mozilla::gfx::IntSize nsSize::ScaleToNearestPixels(
    float aXScale, float aYScale, nscoord aAppUnitsPerPixel) const {
  return mozilla::gfx::IntSize(
      NSToIntRoundUp(NSAppUnitsToDoublePixels(width, aAppUnitsPerPixel) *
                     aXScale),
      NSToIntRoundUp(NSAppUnitsToDoublePixels(height, aAppUnitsPerPixel) *
                     aYScale));
}

inline mozilla::gfx::IntSize nsSize::ToNearestPixels(
    nscoord aAppUnitsPerPixel) const {
  return ScaleToNearestPixels(1.0f, 1.0f, aAppUnitsPerPixel);
}

inline nsSize nsSize::ScaleToOtherAppUnits(int32_t aFromAPP,
                                           int32_t aToAPP) const {
  if (aFromAPP != aToAPP) {
    nsSize size;
    size.width = NSToCoordRound(NSCoordScale(width, aFromAPP, aToAPP));
    size.height = NSToCoordRound(NSCoordScale(height, aFromAPP, aToAPP));
    return size;
  }
  return *this;
}

inline nsSize IntSizeToAppUnits(mozilla::gfx::IntSize aSize,
                                nscoord aAppUnitsPerPixel) {
  return nsSize(NSIntPixelsToAppUnits(aSize.width, aAppUnitsPerPixel),
                NSIntPixelsToAppUnits(aSize.height, aAppUnitsPerPixel));
}

#endif /* NSSIZE_H */
