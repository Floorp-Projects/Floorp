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

#include "nsRect.h"
#include "nsString.h"
#include "nsUnitConversion.h"

#define MIN(a,b)\
  ((a) < (b) ? (a) : (b))
#define MAX(a,b)\
  ((a) > (b) ? (a) : (b))

// Containment
PRBool nsRect::Contains(nscoord aX, nscoord aY) const
{
  return (PRBool) ((aX >= x) && (aY >= y) &&
                   (aX < XMost()) && (aY < YMost()));
}

PRBool nsRect::Contains(const nsRect &aRect) const
{
  return (PRBool) ((aRect.x >= x) && (aRect.y >= y) &&
                   (aRect.XMost() <= XMost()) && (aRect.YMost() <= YMost()));
}

// Intersection. Returns TRUE if the receiver overlaps aRect and
// FALSE otherwise
PRBool nsRect::Intersects(const nsRect &aRect) const
{
  return (PRBool) ((x < aRect.XMost()) && (y < aRect.YMost()) &&
                   (aRect.x < XMost()) && (aRect.y < YMost()));
}

// Computes the area in which aRect1 and aRect2 overlap and fills 'this' with
// the result. Returns FALSE if the rectangles don't intersect.
PRBool nsRect::IntersectRect(const nsRect &aRect1, const nsRect &aRect2)
{
  nscoord  xmost1 = aRect1.XMost();
  nscoord  ymost1 = aRect1.YMost();
  nscoord  xmost2 = aRect2.XMost();
  nscoord  ymost2 = aRect2.YMost();
  nscoord  temp;

  x = MAX(aRect1.x, aRect2.x);
  y = MAX(aRect1.y, aRect2.y);

  // Compute the destination width
  temp = MIN(xmost1, xmost2);
  if (temp <= x) {
    Empty();
    return PR_FALSE;
  }
  width = temp - x;

  // Compute the destination height
  temp = MIN(ymost1, ymost2);
  if (temp <= y) {
    Empty();
    return PR_FALSE;
  }
  height = temp - y;

  return PR_TRUE;
}

// Computes the smallest rectangle that contains both aRect1 and aRect2 and
// fills 'this' with the result. Returns FALSE if both aRect1 and aRect2 are
// empty and TRUE otherwise
PRBool nsRect::UnionRect(const nsRect &aRect1, const nsRect &aRect2)
{
  PRBool  result = PR_TRUE;

  // Is aRect1 empty?
  if (aRect1.IsEmpty()) {
    if (aRect2.IsEmpty()) {
      // Both rectangles are empty which is an error
      Empty();
      result = PR_FALSE;
    } else {
      // aRect1 is empty so set the result to aRect2
      *this = aRect2;
    }
  } else if (aRect2.IsEmpty()) {
    // aRect2 is empty so set the result to aRect1
    *this = aRect1;
  } else {
    nscoord xmost1 = aRect1.XMost();
    nscoord xmost2 = aRect2.XMost();
    nscoord ymost1 = aRect1.YMost();
    nscoord ymost2 = aRect2.YMost();

    // Compute the origin
    x = MIN(aRect1.x, aRect2.x);
    y = MIN(aRect1.y, aRect2.y);

    // Compute the size
    width = MAX(xmost1, xmost2) - x;
    height = MAX(ymost1, ymost2) - y;
  }

  return result;
}

// Inflate the rect by the specified width and height
void nsRect::Inflate(nscoord aDx, nscoord aDy)
{
  x -= aDx;
  y -= aDy;
  width += 2 * aDx;
  height += 2 * aDy;
}

// Inflate the rect by the specified margin
void nsRect::Inflate(const nsMargin &aMargin)
{
  x -= aMargin.left;
  y -= aMargin.top;
  width += aMargin.left + aMargin.right;
  height += aMargin.top + aMargin.bottom;
}

// Deflate the rect by the specified width and height
void nsRect::Deflate(nscoord aDx, nscoord aDy)
{
  x += aDx;
  y += aDy;
  width -= 2 * aDx;
  height -= 2 * aDy;
}

// Deflate the rect by the specified margin
void nsRect::Deflate(const nsMargin &aMargin)
{
  x += aMargin.left;
  y += aMargin.top;
  width -= aMargin.left + aMargin.right;
  height -= aMargin.top + aMargin.bottom;
}

// Diagnostics

FILE* operator<<(FILE* out, const nsRect& rect)
{
  nsAutoString tmp;

  // Output the coordinates in fractional points so they're easier to read
  tmp.Append("{");
  tmp.Append(NS_TWIPS_TO_POINTS_FLOAT(rect.x));
  tmp.Append(", ");
  tmp.Append(NS_TWIPS_TO_POINTS_FLOAT(rect.y));
  tmp.Append(", ");
  tmp.Append(NS_TWIPS_TO_POINTS_FLOAT(rect.width));
  tmp.Append(", ");
  tmp.Append(NS_TWIPS_TO_POINTS_FLOAT(rect.height));
  tmp.Append("}");
  fputs(tmp, out);
  return out;
}
