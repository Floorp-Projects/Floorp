/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef NSPOINT_H
#define NSPOINT_H

#include "nsCoord.h"

struct nsPoint {
  nscoord x, y;

  // Constructors
  nsPoint() {}
  nsPoint(const nsPoint& aPoint) {x = aPoint.x; y = aPoint.y;}
  nsPoint(nscoord aX, nscoord aY) {x = aX; y = aY;}

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
};


/** ---------------------------------------------------
 *  A special type of point which also add the capability to tell if a point is on
 *  the curve.. or off of the curve for a path
 *	@update 03/29/00 dwc
 */
struct nsPathPoint: public nsPoint{

  PRBool  mIsOnCurve;

  // Constructors
  nsPathPoint() {}
  nsPathPoint(const nsPathPoint& aPoint) {x = aPoint.x; y = aPoint.y;mIsOnCurve=aPoint.mIsOnCurve;}
  nsPathPoint(nscoord aX, nscoord aY) {x = aX; y = aY;mIsOnCurve=PR_TRUE;}
  nsPathPoint(nscoord aX, nscoord aY,PRBool aIsOnCurve) {x = aX; y = aY;mIsOnCurve=aIsOnCurve;}

};


#endif /* NSPOINT_H */
