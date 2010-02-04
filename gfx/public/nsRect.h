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
#include "gfxCore.h"
#include "nsTraceRefcnt.h"

struct nsIntRect;

struct NS_GFX nsRect {
  nscoord x, y;
  nscoord width, height;

  // Constructors
  nsRect() : x(0), y(0), width(0), height(0) {
    MOZ_COUNT_CTOR(nsRect);
  }
  nsRect(const nsRect& aRect) {
    MOZ_COUNT_CTOR(nsRect);
    *this = aRect;
  }
  nsRect(const nsPoint& aOrigin, const nsSize &aSize) {
    MOZ_COUNT_CTOR(nsRect);
    x = aOrigin.x; y = aOrigin.y;
    width = aSize.width; height = aSize.height;
  }
  nsRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight) {
    MOZ_COUNT_CTOR(nsRect);
    x = aX; y = aY; width = aWidth; height = aHeight;
    VERIFY_COORD(x); VERIFY_COORD(y); VERIFY_COORD(width); VERIFY_COORD(height);
  }

#ifdef NS_BUILD_REFCNT_LOGGING
  ~nsRect() {
    MOZ_COUNT_DTOR(nsRect);
  }
#endif
  
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
  // fills 'this' with the result, ignoring empty input rectangles.
  // Returns FALSE and sets 'this' rect to be an empty rect if both aRect1
  // and aRect2 are empty.
  //
  // 'this' can be the same object as either aRect1 or aRect2
  PRBool UnionRect(const nsRect& aRect1, const nsRect& aRect2);

  // Computes the smallest rectangle that contains both aRect1 and aRect2,
  // where empty input rectangles are allowed to affect the result; the
  // top-left of an empty input rectangle will be inside or on the edge of
  // the result.
  //
  // 'this' can be the same object as either aRect1 or aRect2
  void UnionRectIncludeEmpty(const nsRect& aRect1, const nsRect& aRect2);
  
  // Accessors
  void SetRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight) {
    x = aX; y = aY; width = aWidth; height = aHeight;
  }
  void SetRect(const nsPoint& aPt, const nsSize& aSize) {
    SetRect(aPt.x, aPt.y, aSize.width, aSize.height);
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

  // Useful when we care about the exact x/y/width/height values being
  // equal (i.e. we care about differences in empty rectangles)
  PRBool IsExactEqual(const nsRect& aRect) const {
    return x == aRect.x && y == aRect.y &&
           width == aRect.width && height == aRect.height;
  }

  // Arithmetic with nsPoints
  nsRect  operator+(const nsPoint& aPoint) const {
    return nsRect(x + aPoint.x, y + aPoint.y, width, height);
  }
  nsRect  operator-(const nsPoint& aPoint) const {
    return nsRect(x - aPoint.x, y - aPoint.y, width, height);
  }
  nsRect& operator+=(const nsPoint& aPoint) {x += aPoint.x; y += aPoint.y; return *this;}
  nsRect& operator-=(const nsPoint& aPoint) {x -= aPoint.x; y -= aPoint.y; return *this;}

  // Arithmetic with nsMargins
  nsMargin operator-(const nsRect& aRect) const; // Find difference as nsMargin
  nsRect& operator+=(const nsMargin& aMargin) { Inflate(aMargin); return *this; }
  nsRect& operator-=(const nsMargin& aMargin) { Deflate(aMargin); return *this; }
  nsRect  operator+(const nsMargin& aMargin) const { return nsRect(*this) += aMargin; }
  nsRect  operator-(const nsMargin& aMargin) const { return nsRect(*this) -= aMargin; }

  // Scale by aScale, converting coordinates to integers so that the result
  // is the smallest integer-coordinate rectangle containing the unrounded result
  nsRect& ScaleRoundOut(float aScale);

  // Helpers for accessing the vertices
  nsPoint TopLeft() const { return nsPoint(x, y); }
  nsPoint TopRight() const { return nsPoint(XMost(), y); }
  nsPoint BottomLeft() const { return nsPoint(x, YMost()); }
  nsPoint BottomRight() const { return nsPoint(XMost(), YMost()); }

  nsSize Size() const { return nsSize(width, height); }

  // Helper methods for computing the extents
  nscoord XMost() const {return x + width;}
  nscoord YMost() const {return y + height;}

  inline nsIntRect ToNearestPixels(nscoord aAppUnitsPerPixel) const;
  inline nsIntRect ToOutsidePixels(nscoord aAppUnitsPerPixel) const;
  inline nsIntRect ToInsidePixels(nscoord aAppUnitsPerPixel) const;
};

struct NS_GFX nsIntRect {
  PRInt32 x, y;
  PRInt32 width, height;

  // Constructors
  nsIntRect() : x(0), y(0), width(0), height(0) {}
  nsIntRect(const nsIntRect& aRect) {*this = aRect;}
  nsIntRect(const nsIntPoint& aOrigin, const nsIntSize &aSize) {
    x = aOrigin.x; y = aOrigin.y;
    width = aSize.width; height = aSize.height;
  }
  nsIntRect(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight) {
    x = aX; y = aY; width = aWidth; height = aHeight;
  }

  // Emptiness. An empty rect is one that has no area, i.e. its height or width
  // is <= 0
  PRBool IsEmpty() const {
    return (PRBool) ((height <= 0) || (width <= 0));
  }
  void Empty() {width = height = 0;}

  // Inflate the rect by the specified width/height or margin
  void Inflate(PRInt32 aDx, PRInt32 aDy) {
    x -= aDx;
    y -= aDy;
    width += aDx*2;
    height += aDy*2;
  }
  void Inflate(const nsIntMargin &aMargin) {
    x -= aMargin.left;
    y -= aMargin.top;
    width += aMargin.left + aMargin.right;
    height += aMargin.top + aMargin.bottom;
  }

  // Overloaded operators. Note that '=' isn't defined so we'll get the
  // compiler generated default assignment operator.
  PRBool operator==(const nsIntRect& aRect) const {
    return (PRBool) ((IsEmpty() && aRect.IsEmpty()) ||
                     ((x == aRect.x) && (y == aRect.y) &&
                      (width == aRect.width) && (height == aRect.height)));
  }
  PRBool  operator!=(const nsIntRect& aRect) const {
    return (PRBool) !operator==(aRect);
  }

  nsIntRect  operator+(const nsIntPoint& aPoint) const {
    return nsIntRect(x + aPoint.x, y + aPoint.y, width, height);
  }
  nsIntRect  operator-(const nsIntPoint& aPoint) const {
    return nsIntRect(x - aPoint.x, y - aPoint.y, width, height);
  }
  nsIntRect& operator+=(const nsIntPoint& aPoint) {x += aPoint.x; y += aPoint.y; return *this;}
  nsIntRect& operator-=(const nsIntPoint& aPoint) {x -= aPoint.x; y -= aPoint.y; return *this;}

  void SetRect(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight) {
    x = aX; y = aY; width = aWidth; height = aHeight;
  }

  void MoveTo(PRInt32 aX, PRInt32 aY) {x = aX; y = aY;}
  void MoveTo(const nsIntPoint& aPoint) {x = aPoint.x; y = aPoint.y;}
  void MoveBy(PRInt32 aDx, PRInt32 aDy) {x += aDx; y += aDy;}
  void MoveBy(const nsIntPoint& aPoint) {x += aPoint.x; y += aPoint.y;}
  void SizeTo(PRInt32 aWidth, PRInt32 aHeight) {width = aWidth; height = aHeight;}
  void SizeTo(const nsIntSize& aSize) {SizeTo(aSize.width, aSize.height);}
  void SizeBy(PRInt32 aDeltaWidth, PRInt32 aDeltaHeight) {width += aDeltaWidth;
                                                          height += aDeltaHeight;}

  PRBool Contains(const nsIntRect& aRect) const
  {
    return (PRBool) ((aRect.x >= x) && (aRect.y >= y) &&
                     (aRect.XMost() <= XMost()) && (aRect.YMost() <= YMost()));
  }
  PRBool Contains(PRInt32 aX, PRInt32 aY) const
  {
    return (PRBool) ((aX >= x) && (aY >= y) &&
                     (aX < XMost()) && (aY < YMost()));
  }
  PRBool Contains(const nsIntPoint& aPoint) const { return Contains(aPoint.x, aPoint.y); }

  // Intersection. Returns TRUE if the receiver overlaps aRect and
  // FALSE otherwise
  PRBool Intersects(const nsIntRect& aRect) const {
    return (PRBool) ((x < aRect.XMost()) && (y < aRect.YMost()) &&
                     (aRect.x < XMost()) && (aRect.y < YMost()));
  }

  // Computes the area in which aRect1 and aRect2 overlap, and fills 'this' with
  // the result. Returns FALSE if the rectangles don't intersect, and sets 'this'
  // rect to be an empty rect.
  //
  // 'this' can be the same object as either aRect1 or aRect2
  PRBool IntersectRect(const nsIntRect& aRect1, const nsIntRect& aRect2);

  // Computes the smallest rectangle that contains both aRect1 and aRect2 and
  // fills 'this' with the result. Returns FALSE and sets 'this' rect to be an
  // empty rect if both aRect1 and aRect2 are empty
  //
  // 'this' can be the same object as either aRect1 or aRect2
  PRBool UnionRect(const nsIntRect& aRect1, const nsIntRect& aRect2);

  // Helpers for accessing the vertices
  nsIntPoint TopLeft() const { return nsIntPoint(x, y); }
  nsIntPoint TopRight() const { return nsIntPoint(XMost(), y); }
  nsIntPoint BottomLeft() const { return nsIntPoint(x, YMost()); }
  nsIntPoint BottomRight() const { return nsIntPoint(XMost(), YMost()); }

  nsIntSize Size() const { return nsIntSize(width, height); }

  // Helper methods for computing the extents
  PRInt32 XMost() const {return x + width;}
  PRInt32 YMost() const {return y + height;}

  inline nsRect ToAppUnits(nscoord aAppUnitsPerPixel) const;
};

/*
 * App Unit/Pixel conversions
 */
// scale the rect but round to preserve centers
inline nsIntRect
nsRect::ToNearestPixels(nscoord aAppUnitsPerPixel) const
{
  nsIntRect rect;
  rect.x = NSToIntRoundUp(NSAppUnitsToFloatPixels(x, float(aAppUnitsPerPixel)));
  rect.y = NSToIntRoundUp(NSAppUnitsToFloatPixels(y, float(aAppUnitsPerPixel)));
  rect.width  = NSToIntRoundUp(NSAppUnitsToFloatPixels(XMost(),
                               float(aAppUnitsPerPixel))) - rect.x;
  rect.height = NSToIntRoundUp(NSAppUnitsToFloatPixels(YMost(),
                               float(aAppUnitsPerPixel))) - rect.y;
  return rect;
}

// scale the rect but round to smallest containing rect
inline nsIntRect
nsRect::ToOutsidePixels(nscoord aAppUnitsPerPixel) const
{
  nsIntRect rect;
  rect.x = NSToIntFloor(NSAppUnitsToFloatPixels(x, float(aAppUnitsPerPixel)));
  rect.y = NSToIntFloor(NSAppUnitsToFloatPixels(y, float(aAppUnitsPerPixel)));
  rect.width  = NSToIntCeil(NSAppUnitsToFloatPixels(XMost(),
                            float(aAppUnitsPerPixel))) - rect.x;
  rect.height = NSToIntCeil(NSAppUnitsToFloatPixels(YMost(),
                            float(aAppUnitsPerPixel))) - rect.y;
  return rect;
}

// scale the rect but round to largest contained rect
inline nsIntRect
nsRect::ToInsidePixels(nscoord aAppUnitsPerPixel) const
{
  nsIntRect rect;
  rect.x = NSToIntCeil(NSAppUnitsToFloatPixels(x, float(aAppUnitsPerPixel)));
  rect.y = NSToIntCeil(NSAppUnitsToFloatPixels(y, float(aAppUnitsPerPixel)));
  rect.width  = NSToIntFloor(NSAppUnitsToFloatPixels(XMost(),
                             float(aAppUnitsPerPixel))) - rect.x;
  rect.height = NSToIntFloor(NSAppUnitsToFloatPixels(YMost(),
                             float(aAppUnitsPerPixel))) - rect.y;
  return rect;
}

// app units are integer multiples of pixels, so no rounding needed
inline nsRect
nsIntRect::ToAppUnits(nscoord aAppUnitsPerPixel) const
{
  return nsRect(NSIntPixelsToAppUnits(x, aAppUnitsPerPixel),
                NSIntPixelsToAppUnits(y, aAppUnitsPerPixel),
                NSIntPixelsToAppUnits(width, aAppUnitsPerPixel),
                NSIntPixelsToAppUnits(height, aAppUnitsPerPixel));
}

#ifdef DEBUG
// Diagnostics
extern NS_GFX FILE* operator<<(FILE* out, const nsRect& rect);
#endif // DEBUG

#endif /* NSRECT_H */
