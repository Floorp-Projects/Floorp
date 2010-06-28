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

struct nsIntPoint;

struct nsPoint {
  nscoord x, y;

  // Constructors
  nsPoint() {}
  nsPoint(const nsPoint& aPoint) { x = aPoint.x; y = aPoint.y;}
  nsPoint(nscoord aX, nscoord aY) { VERIFY_COORD(aX); VERIFY_COORD(aY); x = aX; y = aY;}

  void MoveTo(nscoord aX, nscoord aY) {x = aX; y = aY;}
  void MoveBy(nscoord aDx, nscoord aDy) {x += aDx; y += aDy;}

  // Overloaded operators. Note that '=' isn't defined so we'll get the
  // compiler generated default assignment operator
  PRBool   operator==(const nsPoint& aPoint) const {
    return (PRBool) ((x == aPoint.x) && (y == aPoint.y));
  }
  PRBool   operator!=(const nsPoint& aPoint) const {
    return (PRBool) ((x != aPoint.x) || (y != aPoint.y));
  }
  nsPoint operator+(const nsPoint& aPoint) const {
    return nsPoint(x + aPoint.x, y + aPoint.y);
  }
  nsPoint operator-(const nsPoint& aPoint) const {
    return nsPoint(x - aPoint.x, y - aPoint.y);
  }
  nsPoint& operator+=(const nsPoint& aPoint) {
    x += aPoint.x;
    y += aPoint.y;
    return *this;
  }
  nsPoint& operator-=(const nsPoint& aPoint) {
    x -= aPoint.x;
    y -= aPoint.y;
    return *this;
  }

  nsPoint operator-() const {
    return nsPoint(-x, -y);
  }

  inline nsIntPoint ToNearestPixels(nscoord aAppUnitsPerPixel) const;
};

struct nsIntPoint {
  PRInt32 x, y;

  // Constructors
  nsIntPoint() {}
  nsIntPoint(const nsIntPoint& aPoint) { x = aPoint.x; y = aPoint.y;}
  nsIntPoint(PRInt32 aX, PRInt32 aY) { x = aX; y = aY;}

  PRBool   operator==(const nsIntPoint& aPoint) const {
    return (PRBool) ((x == aPoint.x) && (y == aPoint.y));
  }
  PRBool   operator!=(const nsIntPoint& aPoint) const {
    return (PRBool) ((x != aPoint.x) || (y != aPoint.y));
  }
  nsIntPoint operator+(const nsIntPoint& aPoint) const {
    return nsIntPoint(x + aPoint.x, y + aPoint.y);
  }
  nsIntPoint operator-(const nsIntPoint& aPoint) const {
    return nsIntPoint(x - aPoint.x, y - aPoint.y);
  }
  nsIntPoint& operator+=(const nsIntPoint& aPoint) {
    x += aPoint.x;
    y += aPoint.y;
    return *this;
  }
  nsIntPoint& operator-=(const nsIntPoint& aPoint) {
    x -= aPoint.x;
    y -= aPoint.y;
    return *this;
  }
  nsIntPoint operator-() const {
    return nsIntPoint(-x, -y);
  }
  void MoveTo(PRInt32 aX, PRInt32 aY) {x = aX; y = aY;}
};

inline nsIntPoint
nsPoint::ToNearestPixels(nscoord aAppUnitsPerPixel) const {
  return nsIntPoint(
      NSToIntRound(NSAppUnitsToFloatPixels(x, float(aAppUnitsPerPixel))),
      NSToIntRound(NSAppUnitsToFloatPixels(y, float(aAppUnitsPerPixel))));
}

#endif /* NSPOINT_H */
