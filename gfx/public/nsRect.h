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


#ifndef NSRECT_H
#define NSRECT_H

#include <stdio.h>
#include "nsCoord.h"
#include "nsPoint.h"
#include "nsSize.h"
#include "nsMargin.h"
#include "nsUnitConversion.h"
#include "gfxCore.h"

struct NS_GFX nsRect {
  nscoord x, y;
  nscoord width, height;

  // Constructors
  nsRect() : x(0), y(0), width(0), height(0) {}
  nsRect(const nsRect& aRect) {*this = aRect;}
  nsRect(const nsPoint& aOrigin, const nsSize &aSize) {x = aOrigin.x; y = aOrigin.y;
                                                       width = aSize.width; height = aSize.height;}
  nsRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight) {x = aX; y = aY;
                                                                   width = aWidth; height = aHeight;}

  // Emptiness. An empty rect is one that has no area, i.e. its height or width
  // is <= 0
  PRBool IsEmpty() const {
    return (PRBool) ((height <= 0) || (width <= 0));
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
  void MoveTo(const nsPoint& aPoint) {x = aPoint.x; y = aPoint.y;}
  void MoveBy(nscoord aDx, nscoord aDy) {x += aDx; y += aDy;}
  void MoveBy(const nsPoint& aPoint) {x += aPoint.x; y += aPoint.y;}
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

  nsRect  operator+(const nsPoint& aPoint) const {
    return nsRect(x + aPoint.x, y + aPoint.y, width, height);
  }
  nsRect  operator-(const nsPoint& aPoint) const {
    return nsRect(x - aPoint.x, y - aPoint.y, width, height);
  }
  nsRect& operator+=(const nsPoint& aPoint) {x += aPoint.x; y += aPoint.y; return *this;}
  nsRect& operator-=(const nsPoint& aPoint) {x -= aPoint.x; y -= aPoint.y; return *this;}

  nsRect& operator*=(const float aScale) {x = NSToCoordRound(x * aScale); 
                                          y = NSToCoordRound(y * aScale); 
                                          width = NSToCoordRound(width * aScale); 
                                          height = NSToCoordRound(height * aScale); 
                                          return *this;}

  nsRect& ScaleRoundOut(const float aScale);
  nsRect& ScaleRoundIn(const float aScale);

  // Helpers for accessing the vertices
  nsPoint TopLeft() const { return nsPoint(x, y); }
  nsPoint TopRight() const { return nsPoint(XMost(), y); }
  nsPoint BottomLeft() const { return nsPoint(x, YMost()); }
  nsPoint BottomRight() const { return nsPoint(XMost(), YMost()); }

  // Helper methods for computing the extents
  nscoord XMost() const {return x + width;}
  nscoord YMost() const {return y + height;}
};

#ifdef DEBUG
// Diagnostics
extern NS_GFX FILE* operator<<(FILE* out, const nsRect& rect);
#endif // DEBUG

#endif /* NSRECT_H */
