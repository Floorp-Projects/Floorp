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

#ifndef NSRECT_H
#define NSRECT_H

#include <stdio.h>
#include "gfxtypes.h"

#include "nsPoint2.h"
#include "nsSize2.h"
#include "nsUnitConverters.h"

struct nsMargin2;

/**
 * nsRect2
 *
 * @version 1.1
 **/
struct nsRect2 {
  gfx_coord x, y;
  gfx_dimension width, height;

  // Constructors
  nsRect2() : x(0), y(0), width(0), height(0) {}
  nsRect2(const nsRect2& aRect) {
    *this = aRect;
  }
  nsRect2(const nsPoint2& aOrigin, const nsSize2 &aSize) {
    x = aOrigin.x; y = aOrigin.y;
    width = aSize.width; height = aSize.height;
  }
  nsRect2(gfx_coord aX, gfx_coord aY, gfx_dimension aWidth, gfx_dimension aHeight) {
    x = aX; y = aY;
    width = aWidth; height = aHeight;
  }

  // Emptiness. An empty rect is one that has no area, i.e. its width or height
  // is <= 0
  PRBool IsEmpty() const {
    return (PRBool) ((width <= 0) || (height <= 0));
  }
  void   Empty() {width = height = 0;}

  /**
   * Contains \a aRect
   * @overload
   *
   * @param aRect
   * @return TRUE if \a aRect is contained inside of 'this'.
   */
  PRBool Contains(const nsRect2& aRect) const;

  /**
   * Contains the point (\a aX, \a aY)
   * @overload
   *
   * @param aX
   * @param aY
   * @return TRUE if (\a aX, \a aY) is contained inside of 'this'.
   */
  PRBool Contains(gfx_coord aX, gfx_coord aY) const;

  /**
   * Contains the point \a aPoint
   * @overload
   *
   * @param aPoint
   * @return TRUE if \a aPoint is contained inside of 'this'.
   */
  PRBool Contains(const nsPoint2& aPoint) const {
    return Contains(aPoint.x, aPoint.y);
  }

  /**
   * Intersect 'this' and \a aRect
   * @param aRect The rectangle to intersect 'this' with.
   * @return TRUE if the receiver overlaps \a aRect and FALSE otherwise.
   */
  PRBool Intersects(const nsRect2& aRect) const;

  /**
   * Computes the area in which \a aRect1 and \a aRect2 overlap, and fills 'this' with
   * the result.
   *
   * @param aRect1
   * @param aRect2
   *
   * @return FALSE if the rectangles don't intersect, and sets 'this'
   *         rect to be an empty rect.
   *
   * @note 'this' can be the same object as either \a aRect1 or \a aRect2
   */
  PRBool IntersectRect(const nsRect2& aRect1, const nsRect2& aRect2);

  /**
   * Computes the smallest rectangle that contains both \a aRect1 and \a aRect2 and
   * fills 'this' with the result.
   *
   * @param aRect1
   * @param aRect2
   *
   * @return FALSE and sets 'this' rect to be an empty rect if both
   *         \a aRect1 and \a aRect2 are empty
   *
   * @note 'this' can be the same object as either aRect1 or aRect2
   */
  PRBool UnionRect(const nsRect2& aRect1, const nsRect2& aRect2);

  // Accessors
  void SetRect(gfx_coord aX, gfx_coord aY, gfx_dimension aWidth, gfx_dimension aHeight) {
    x = aX; y = aY; width = aWidth; height = aHeight;
  }
  void MoveTo(gfx_coord aX, gfx_coord aY) {
    x = aX; y = aY;
  }
  void MoveTo(const nsPoint2& aPoint) {
    x = aPoint.x; y = aPoint.y;
  }
  void MoveBy(gfx_coord aDx, gfx_coord aDy) {
    x += aDx; y += aDy;
  }
  void SizeTo(gfx_dimension aWidth, gfx_dimension aHeight) {
    width = aWidth; height = aHeight;
  }
  void SizeTo(const nsSize2& aSize) {
    SizeTo(aSize.width, aSize.height);
  }
  void SizeBy(gfx_coord aDeltaWidth, gfx_coord aDeltaHeight) {
    width += aDeltaWidth;
    height += aDeltaHeight;
  }

  // Inflate the rect by the specified width/height or margin
  void Inflate(gfx_coord aDx, gfx_coord aDy);
  void Inflate(const nsSize2& aSize) {
    Inflate(aSize.width, aSize.height);
  }
  void Inflate(const nsMargin2& aMargin);

  // Deflate the rect by the specified width/height or margin
  void Deflate(gfx_coord aDx, gfx_coord aDy);
  void Deflate(const nsSize2& aSize) {
    Deflate(aSize.width, aSize.height);
  }
  void Deflate(const nsMargin2& aMargin);

  // Overloaded operators. Note that '=' isn't defined so we'll get the
  // compiler generated default assignment operator.
  PRBool  operator==(const nsRect2& aRect) const {
    return (PRBool) ((IsEmpty() && aRect.IsEmpty()) ||
                     ((x == aRect.x) && (y == aRect.y) &&
                      (width == aRect.width) && (height == aRect.height)));
  }
  PRBool  operator!=(const nsRect2& aRect) const {
    return (PRBool) !operator==(aRect);
  }
  nsRect2  operator+(const nsRect2& aRect) const {
    return nsRect2(x + aRect.x, y + aRect.y,
                  width + aRect.width, height + aRect.height);
  }
  nsRect2  operator-(const nsRect2& aRect) const {
    return nsRect2(x - aRect.x, y - aRect.y,
                  width - aRect.width, height - aRect.height);
  }
  nsRect2& operator+=(const nsPoint2& aPoint) {
    x += aPoint.x; y += aPoint.y; return *this;
  }
  nsRect2& operator-=(const nsPoint2& aPoint) {
    x -= aPoint.x; y -= aPoint.y; return *this;
  }

  nsRect2& operator*=(const float aScale) {
    x = x * aScale;
    y = y * aScale;
    width = width * aScale;
    height = height * aScale;
    return *this;
  }

  nsRect2& ScaleRoundOut(const float aScale);
  nsRect2& ScaleRoundIn(const float aScale);

  // Helper methods for computing the extents
  gfx_coord XMost() const {
    return x + width;
  }
  gfx_coord YMost() const {
    return y + height;
  }
};

#ifdef DEBUG_RECT
// Diagnostics
extern NS_GFX FILE* operator<<(FILE* out, const nsRect2& rect);
#endif

#endif /* NSRECT_H */
