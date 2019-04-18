/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_SVGPOINT_H__
#define MOZILLA_SVGPOINT_H__

#include "nsDebug.h"
#include "gfxPoint.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/FloatingPoint.h"

namespace mozilla {

/**
 * This class is currently used for point list attributes.
 *
 * The DOM wrapper class for this class is DOMSVGPoint.
 */
class SVGPoint {
  typedef mozilla::gfx::Point Point;

 public:
  SVGPoint() : mX(0.0f), mY(0.0f) {}

  SVGPoint(float aX, float aY) : mX(aX), mY(aY) {
    NS_ASSERTION(IsValid(), "Constructed an invalid SVGPoint");
  }

  bool operator==(const SVGPoint& rhs) const {
    return mX == rhs.mX && mY == rhs.mY;
  }

  SVGPoint& operator+=(const SVGPoint& rhs) {
    mX += rhs.mX;
    mY += rhs.mY;
    return *this;
  }

  operator gfxPoint() const { return gfxPoint(mX, mY); }

  operator Point() const { return Point(mX, mY); }

#ifdef DEBUG
  bool IsValid() const { return IsFinite(mX) && IsFinite(mY); }
#endif

  void SetX(float aX) { mX = aX; }
  void SetY(float aY) { mY = aY; }
  float GetX() const { return mX; }
  float GetY() const { return mY; }

  bool operator!=(const SVGPoint& rhs) const {
    return mX != rhs.mX || mY != rhs.mY;
  }

  float mX;
  float mY;
};

inline SVGPoint operator+(const SVGPoint& aP1, const SVGPoint& aP2) {
  return SVGPoint(aP1.mX + aP2.mX, aP1.mY + aP2.mY);
}

inline SVGPoint operator-(const SVGPoint& aP1, const SVGPoint& aP2) {
  return SVGPoint(aP1.mX - aP2.mX, aP1.mY - aP2.mY);
}

inline SVGPoint operator*(float aFactor, const SVGPoint& aPoint) {
  return SVGPoint(aFactor * aPoint.mX, aFactor * aPoint.mY);
}

inline SVGPoint operator*(const SVGPoint& aPoint, float aFactor) {
  return SVGPoint(aFactor * aPoint.mX, aFactor * aPoint.mY);
}

}  // namespace mozilla

#endif  // MOZILLA_SVGPOINT_H__
