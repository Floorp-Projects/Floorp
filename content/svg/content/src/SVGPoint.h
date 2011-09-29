/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla SVG Project code.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef MOZILLA_SVGPOINT_H__
#define MOZILLA_SVGPOINT_H__

#include "nsDebug.h"
#include "gfxPoint.h"
#include "nsMathUtils.h"

namespace mozilla {

/**
 * This class is currently used for point list attributes.
 *
 * The DOM wrapper class for this class is DOMSVGPoint.
 */
class SVGPoint
{
public:

  SVGPoint()
#ifdef DEBUG
    : mX(0.0f)
    , mY(0.0f)
#endif
  {}

  SVGPoint(float aX, float aY)
    : mX(aX)
    , mY(aY)
  {
    NS_ASSERTION(IsValid(), "Constructed an invalid SVGPoint");
  }

  SVGPoint(const SVGPoint &aOther)
    : mX(aOther.mX)
    , mY(aOther.mY)
  {}

  SVGPoint& operator=(const SVGPoint &rhs) {
    mX = rhs.mX;
    mY = rhs.mY;
    return *this;
  }

  bool operator==(const SVGPoint &rhs) const {
    return mX == rhs.mX && mY == rhs.mY;
  }

  SVGPoint& operator+=(const SVGPoint &rhs) {
    mX += rhs.mX;
    mY += rhs.mY;
    return *this;
  }

  operator gfxPoint() const {
    return gfxPoint(mX, mY);
  }

#ifdef DEBUG
  bool IsValid() const {
    return NS_finite(mX) && NS_finite(mY);
  }
#endif

  float mX;
  float mY;
};

inline SVGPoint operator+(const SVGPoint& aP1,
                          const SVGPoint& aP2)
{
  return SVGPoint(aP1.mX + aP2.mX, aP1.mY + aP2.mY);
}

inline SVGPoint operator-(const SVGPoint& aP1,
                          const SVGPoint& aP2)
{
  return SVGPoint(aP1.mX - aP2.mX, aP1.mY - aP2.mY);
}

inline SVGPoint operator*(float aFactor,
                          const SVGPoint& aPoint)
{
  return SVGPoint(aFactor * aPoint.mX, aFactor * aPoint.mY);
}

inline SVGPoint operator*(const SVGPoint& aPoint,
                          float aFactor)
{
  return SVGPoint(aFactor * aPoint.mX, aFactor * aPoint.mY);
}

} // namespace mozilla

#endif // MOZILLA_SVGPOINT_H__
