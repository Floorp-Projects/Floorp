/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSSIZE_H
#define NSSIZE_H

#include "nsCoord.h"
#include "mozilla/gfx/BaseSize.h"
#include "mozilla/gfx/Point.h"

// Maximum allowable size
#define NS_MAXSIZE nscoord_MAX

struct nsIntSize;
typedef nsIntSize gfxIntSize;

struct nsSize : public mozilla::gfx::BaseSize<nscoord, nsSize> {
  typedef mozilla::gfx::BaseSize<nscoord, nsSize> Super;

  nsSize() : Super() {}
  nsSize(nscoord aWidth, nscoord aHeight) : Super(aWidth, aHeight) {}

  inline nsIntSize ScaleToNearestPixels(float aXScale, float aYScale,
                                        nscoord aAppUnitsPerPixel) const;
  inline nsIntSize ToNearestPixels(nscoord aAppUnitsPerPixel) const;

  // Converts this size from aFromAPP, an appunits per pixel ratio, to aToAPP.
  inline nsSize ConvertAppUnits(int32_t aFromAPP, int32_t aToAPP) const;
};

struct nsIntSize : public mozilla::gfx::BaseSize<int32_t, nsIntSize> {
  typedef mozilla::gfx::BaseSize<int32_t, nsIntSize> Super;

  nsIntSize() : Super() {}
  nsIntSize(int32_t aWidth, int32_t aHeight) : Super(aWidth, aHeight) {}

  inline nsSize ToAppUnits(nscoord aAppUnitsPerPixel) const;
  mozilla::gfx::IntSize ToIntSize() const
  {
    return mozilla::gfx::IntSize(width, height);
  };
};

inline nsIntSize
nsSize::ScaleToNearestPixels(float aXScale, float aYScale,
                             nscoord aAppUnitsPerPixel) const
{
  return nsIntSize(
      NSToIntRoundUp(NSAppUnitsToDoublePixels(width, aAppUnitsPerPixel) * aXScale),
      NSToIntRoundUp(NSAppUnitsToDoublePixels(height, aAppUnitsPerPixel) * aYScale));
}

inline nsIntSize
nsSize::ToNearestPixels(nscoord aAppUnitsPerPixel) const
{
  return ScaleToNearestPixels(1.0f, 1.0f, aAppUnitsPerPixel);
}

inline nsSize
nsSize::ConvertAppUnits(int32_t aFromAPP, int32_t aToAPP) const {
  if (aFromAPP != aToAPP) {
    nsSize size;
    size.width = NSToCoordRound(NSCoordScale(width, aFromAPP, aToAPP));
    size.height = NSToCoordRound(NSCoordScale(height, aFromAPP, aToAPP));
    return size;
  }
  return *this;
}

inline nsSize
nsIntSize::ToAppUnits(nscoord aAppUnitsPerPixel) const
{
  return nsSize(NSIntPixelsToAppUnits(width, aAppUnitsPerPixel),
                NSIntPixelsToAppUnits(height, aAppUnitsPerPixel));
}

#endif /* NSSIZE_H */
