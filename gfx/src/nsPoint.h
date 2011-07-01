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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#endif /* NSPOINT_H */
