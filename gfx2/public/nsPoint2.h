/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-2000 Netscape Communications Corporation.
 * All Rights Reserved.
 * 
 * Contributor(s): 
 *   Stuart Parmenter <pavlov@netscape.com>
 */

#ifndef NSPOINT2_H
#define NSPOINT2_H

#include "gfxtypes.h"

/**
 * nsPoint2
 *
 * @version 1.1
 **/
struct nsPoint2 {
  gfx_coord x, y;

  // Constructors
  nsPoint2() {}
  nsPoint2(const nsPoint2& aPoint) {x = aPoint.x; y = aPoint.y;}
  nsPoint2(gfx_coord aX, gfx_coord aY) {x = aX; y = aY;}

  void MoveTo(gfx_coord aX, gfx_coord aY) {x = aX; y = aY;}
  void MoveBy(gfx_coord aDx, gfx_coord aDy) {x += aDx; y += aDy;}

  // Overloaded operators. Note that '=' isn't defined so we'll get the
  // compiler generated default assignment operator
  PRBool   operator==(const nsPoint2& aPoint) const {
    return (PRBool) ((x == aPoint.x) && (y == aPoint.y));
  }
  PRBool   operator!=(const nsPoint2& aPoint) const {
    return (PRBool) ((x != aPoint.x) || (y != aPoint.y));
  }
  nsPoint2 operator+(const nsPoint2& aPoint) const {
    return nsPoint2(x + aPoint.x, y + aPoint.y);
  }
  nsPoint2 operator-(const nsPoint2& aPoint) const {
    return nsPoint2(x - aPoint.x, y - aPoint.y);
  }
  nsPoint2& operator+=(const nsPoint2& aPoint) {
    x += aPoint.x;
    y += aPoint.y;
    return *this;
  }
  nsPoint2& operator-=(const nsPoint2& aPoint) {
    x -= aPoint.x;
    y -= aPoint.y;
    return *this;
  }
};

/**
 * A special type of point which also add the capability to tell if a point is on
 * the curve.. or off of the curve for a path
 *
 * @author Don Cone <dcone@netscape.com> (3/29/00)
 * @version 0.0
 */
struct nsPathPoint: public nsPoint2{

  PRBool  mIsOnCurve;

  // Constructors
  nsPathPoint() {}
  nsPathPoint(const nsPathPoint& aPoint) {
    x = aPoint.x;
    y = aPoint.y;
    mIsOnCurve = aPoint.mIsOnCurve;
  }
  nsPathPoint(gfx_coord aX, gfx_coord aY) {
    x = aX;
    y = aY;
    mIsOnCurve = PR_TRUE;
  }
  nsPathPoint(gfx_coord aX, gfx_coord aY, PRBool aIsOnCurve) {
    x = aX;
    y = aY;
    mIsOnCurve = aIsOnCurve;
  }

};


#endif /* NSPOINT2_H */
