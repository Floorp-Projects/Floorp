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
#include "mozilla/gfx/Point.h"

// nsIntPoint represents a point in one of the types of pixels.
// Uses of nsIntPoint should eventually be converted to CSSIntPoint,
// LayoutDeviceIntPoint, etc. (see layout/base/Units.h).
typedef mozilla::gfx::IntPoint nsIntPoint;

// nsPoint represents a point in app units.

struct nsPoint : public mozilla::gfx::BasePoint<nscoord, nsPoint> {
  typedef mozilla::gfx::BasePoint<nscoord, nsPoint> Super;

  nsPoint() : Super() {}
  nsPoint(const nsPoint& aPoint) : Super(aPoint) {}
  nsPoint(nscoord aX, nscoord aY) : Super(aX, aY) {}

  inline nsIntPoint ScaleToNearestPixels(float aXScale, float aYScale,
                                         nscoord aAppUnitsPerPixel) const;
  inline nsIntPoint ToNearestPixels(nscoord aAppUnitsPerPixel) const;

  /**
   * Return this point scaled to a different appunits per pixel (APP) ratio.
   * @param aFromAPP the APP to scale from
   * @param aToAPP the APP to scale to
   */
  MOZ_WARN_UNUSED_RESULT inline nsPoint
    ScaleToOtherAppUnits(int32_t aFromAPP, int32_t aToAPP) const;

  MOZ_WARN_UNUSED_RESULT inline nsPoint
    RemoveResolution(const float resolution) const;
  MOZ_WARN_UNUSED_RESULT inline nsPoint
    ApplyResolution(const float resolution) const;
};

inline nsPoint ToAppUnits(const nsIntPoint& aPoint, nscoord aAppUnitsPerPixel);

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
nsPoint::ScaleToOtherAppUnits(int32_t aFromAPP, int32_t aToAPP) const
{
  if (aFromAPP != aToAPP) {
    nsPoint point;
    point.x = NSToCoordRound(NSCoordScale(x, aFromAPP, aToAPP));
    point.y = NSToCoordRound(NSCoordScale(y, aFromAPP, aToAPP));
    return point;
  }
  return *this;
}

inline nsPoint
nsPoint::RemoveResolution(const float resolution) const {
  if (resolution != 1.0f) {
    nsPoint point;
    point.x = NSToCoordRound(NSCoordToFloat(x) / resolution);
    point.y = NSToCoordRound(NSCoordToFloat(y) / resolution);
    return point;
  }
  return *this;
}

inline nsPoint
nsPoint::ApplyResolution(const float resolution) const {
  if (resolution != 1.0f) {
    nsPoint point;
    point.x = NSToCoordRound(NSCoordToFloat(x) * resolution);
    point.y = NSToCoordRound(NSCoordToFloat(y) * resolution);
    return point;
  }
  return *this;
}

// app units are integer multiples of pixels, so no rounding needed
inline nsPoint
ToAppUnits(const nsIntPoint& aPoint, nscoord aAppUnitsPerPixel)
{
  return nsPoint(NSIntPixelsToAppUnits(aPoint.x, aAppUnitsPerPixel),
                 NSIntPixelsToAppUnits(aPoint.y, aAppUnitsPerPixel));
}

#endif /* NSPOINT_H */
