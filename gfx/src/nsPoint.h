/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSPOINT_H
#define NSPOINT_H

#include "nsCoord.h"
#include "mozilla/gfx/BaseSize.h"
#include "mozilla/gfx/BasePoint.h"
#include "nsSize.h"

struct nsIntPoint;

struct nsPoint : public mozilla::gfx::BasePoint<nscoord, nsPoint> {
  typedef mozilla::gfx::BasePoint<nscoord, nsPoint> Super;

  nsPoint() : Super() {}
  nsPoint(const nsPoint& aPoint) : Super(aPoint) {}
  nsPoint(nscoord aX, nscoord aY) : Super(aX, aY) {}

  inline nsIntPoint ScaleToNearestPixels(float aXScale, float aYScale,
                                         nscoord aAppUnitsPerPixel) const;
  inline nsIntPoint ToNearestPixels(nscoord aAppUnitsPerPixel) const;

  // Converts this point from aFromAPP, an appunits per pixel ratio, to aToAPP.
  inline nsPoint ConvertAppUnits(PRInt32 aFromAPP, PRInt32 aToAPP) const;
};

struct nsIntPoint : public mozilla::gfx::BasePoint<PRInt32, nsIntPoint> {
  typedef mozilla::gfx::BasePoint<PRInt32, nsIntPoint> Super;

  nsIntPoint() : Super() {}
  nsIntPoint(const nsIntPoint& aPoint) : Super(aPoint) {}
  nsIntPoint(PRInt32 aX, PRInt32 aY) : Super(aX, aY) {}

  inline nsPoint ToAppUnits(nscoord aAppUnitsPerPixel) const;
};

inline nsIntPoint
nsPoint::ScaleToNearestPixels(float aXScale, float aYScale,
                              nscoord aAppUnitsPerPixel) const
{
  return nsIntPoint(
      NSToIntRoundUp(NSAppUnitsToDoublePixels(x, aAppUnitsPerPixel) * aXScale),
      NSToIntRoundUp(NSAppUnitsToDoublePixels(y, aAppUnitsPerPixel) * aYScale));
}

inline nsIntPoint
nsPoint::ToNearestPixels(nscoord aAppUnitsPerPixel) const
{
  return ScaleToNearestPixels(1.0f, 1.0f, aAppUnitsPerPixel);
}

inline nsPoint
nsPoint::ConvertAppUnits(PRInt32 aFromAPP, PRInt32 aToAPP) const
{
  if (aFromAPP != aToAPP) {
    nsPoint point;
    point.x = NSToCoordRound(NSCoordScale(x, aFromAPP, aToAPP));
    point.y = NSToCoordRound(NSCoordScale(y, aFromAPP, aToAPP));
    return point;
  }
  return *this;
}

// app units are integer multiples of pixels, so no rounding needed
inline nsPoint
nsIntPoint::ToAppUnits(nscoord aAppUnitsPerPixel) const
{
  return nsPoint(NSIntPixelsToAppUnits(x, aAppUnitsPerPixel),
                 NSIntPixelsToAppUnits(y, aAppUnitsPerPixel));
}

#endif /* NSPOINT_H */
