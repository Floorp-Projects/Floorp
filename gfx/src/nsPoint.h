/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
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

#endif /* NSPOINT_H */
