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


#ifndef NSRECT_H
#define NSRECT_H

#include <stdio.h>
#include "nsCoord.h"
#include "nsPoint.h"
#include "nsSize.h"
#include "nsMargin.h"
#include "nsUnitConversion.h"

struct NS_GFX nsRect {
  nscoord x, y;
  nscoord width, height;

  // Constructors
  nsRect() {}
  nsRect(const nsRect& aRect) {*this = aRect;}
  nsRect(const nsPoint& aOrigin, const nsSize &aSize) {x = aOrigin.x; y = aOrigin.y;
                                                       width = aSize.width; height = aSize.height;}
  nsRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight) {x = aX; y = aY;
                                                                   width = aWidth; height = aHeight;}

  // Emptiness. An empty rect is one that has no area, i.e. its width or height
  // is <= 0
  PRBool IsEmpty() const {
    return (PRBool) ((width <= 0) || (height <= 0));
  }
  void   Empty() {width = height = 0;}

  // Containment
  PRBool Contains(const nsRect& aRect) const;
  PRBool Contains(nscoord aX, nscoord aY) const;
  PRBool Contains(const nsPoint& aPoint) const {return Contains(aPoint.x, aPoint.y);}

  // Intersection. Returns TRUE if the receiver overlaps aRect and
  // FALSE otherwise
  PRBool Intersects(const nsRect& aRect) const;

  // Computes the area in which aRect1 and aRect2 overlap, and fills 'this' with
  // the result. Returns FALSE if the rectangles don't intersect, and sets 'this'
  // rect to be an empty rect.
  //
  // 'this' can be the same object as either aRect1 or aRect2
  PRBool IntersectRect(const nsRect& aRect1, const nsRect& aRect2);

  // Computes the smallest rectangle that contains both aRect1 and aRect2 and
  // fills 'this' with the result. Returns FALSE and sets 'this' rect to be an
  // empty rect if both aRect1 and aRect2 are empty
  //
  // 'this' can be the same object as either aRect1 or aRect2
  PRBool UnionRect(const nsRect& aRect1, const nsRect& aRect2);

  // Accessors
  void SetRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight) {
    x = aX; y = aY; width = aWidth; height = aHeight;
  }
  void MoveTo(nscoord aX, nscoord aY) {x = aX; y = aY;}
  void MoveTo(const nsPoint& aPoint) {x = aPoint.y; y = aPoint.y;}
  void MoveBy(nscoord aDx, nscoord aDy) {x += aDx; y += aDy;}
  void SizeTo(nscoord aWidth, nscoord aHeight) {width = aWidth; height = aHeight;}
  void SizeTo(const nsSize& aSize) {SizeTo(aSize.width, aSize.height);}
  void SizeBy(nscoord aDeltaWidth, nscoord aDeltaHeight) {width += aDeltaWidth;
                                                          height += aDeltaHeight;}

  // Inflate the rect by the specified width/height or margin
  void Inflate(nscoord aDx, nscoord aDy);
  void Inflate(const nsSize& aSize) {Inflate(aSize.width, aSize.height);}
  void Inflate(const nsMargin& aMargin);

  // Deflate the rect by the specified width/height or margin
  void Deflate(nscoord aDx, nscoord aDy);
  void Deflate(const nsSize& aSize) {Deflate(aSize.width, aSize.height);}
  void Deflate(const nsMargin& aMargin);

  // Overloaded operators. Note that '=' isn't defined so we'll get the
  // compiler generated default assignment operator.
  PRBool  operator==(const nsRect& aRect) const {
    return (PRBool) ((IsEmpty() && aRect.IsEmpty()) ||
                     ((x == aRect.x) && (y == aRect.y) &&
                      (width == aRect.width) && (height == aRect.height)));
  }
  PRBool  operator!=(const nsRect& aRect) const {
    return (PRBool) !operator==(aRect);
  }
  nsRect  operator+(const nsRect& aRect) const {
    return nsRect(x + aRect.x, y + aRect.y,
                  width + aRect.width, height + aRect.height);
  }
  nsRect  operator-(const nsRect& aRect) const {
    return nsRect(x - aRect.x, y - aRect.y,
                  width - aRect.width, height - aRect.height);
  }
  nsRect& operator+=(const nsPoint& aPoint) {x += aPoint.x; y += aPoint.y; return *this;}
  nsRect& operator-=(const nsPoint& aPoint) {x -= aPoint.x; y -= aPoint.y; return *this;}

  nsRect& operator*=(const float aScale) {x = NS_TO_INT_ROUND(x * aScale); 
                                          y = NS_TO_INT_ROUND(y * aScale); 
                                          width = NS_TO_INT_ROUND(width * aScale); 
                                          height = NS_TO_INT_ROUND(height * aScale); 
                                          return *this;}

  // Helper methods for computing the extents
  nscoord XMost() const {return x + width;}
  nscoord YMost() const {return y + height;}
};

// Diagnostics
extern NS_GFX FILE* operator<<(FILE* out, const nsRect& rect);

#endif /* NSRECT_H */
