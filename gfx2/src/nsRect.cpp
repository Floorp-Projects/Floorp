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

#include "nsRect2.h"
#include "nsMargin2.h"

#ifdef DEBUG_STRING
#include "nsString.h"
#endif

#ifdef MIN
#undef MIN
#endif

#ifdef MAX
#undef MAX
#endif

#define MIN(a,b)\
  ((a) < (b) ? (a) : (b))
#define MAX(a,b)\
  ((a) > (b) ? (a) : (b))

// Containment
PRBool nsRect2::Contains(gfx_coord aX, gfx_coord aY) const
{
  return (PRBool) ((aX >= x) && (aY >= y) &&
                   (aX < XMost()) && (aY < YMost()));
}

PRBool nsRect2::Contains(const nsRect2 &aRect) const
{
  return (PRBool) ((aRect.x >= x) && (aRect.y >= y) &&
                   (aRect.XMost() <= XMost()) && (aRect.YMost() <= YMost()));
}

// Intersection. Returns TRUE if the receiver overlaps aRect and
// FALSE otherwise
PRBool nsRect2::Intersects(const nsRect2 &aRect) const
{
  return (PRBool) ((x < aRect.XMost()) && (y < aRect.YMost()) &&
                   (aRect.x < XMost()) && (aRect.y < YMost()));
}

// Computes the area in which aRect1 and aRect2 overlap and fills 'this' with
// the result. Returns FALSE if the rectangles don't intersect.
PRBool nsRect2::IntersectRect(const nsRect2 &aRect1, const nsRect2 &aRect2)
{
  gfx_coord  xmost1 = aRect1.XMost();
  gfx_coord  ymost1 = aRect1.YMost();
  gfx_coord  xmost2 = aRect2.XMost();
  gfx_coord  ymost2 = aRect2.YMost();
  gfx_coord  temp;

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
PRBool nsRect2::UnionRect(const nsRect2 &aRect1, const nsRect2 &aRect2)
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
    gfx_coord xmost1 = aRect1.XMost();
    gfx_coord xmost2 = aRect2.XMost();
    gfx_coord ymost1 = aRect1.YMost();
    gfx_coord ymost2 = aRect2.YMost();

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
void nsRect2::Inflate(gfx_coord aDx, gfx_coord aDy)
{
  x -= aDx;
  y -= aDy;
  width += 2 * aDx;
  height += 2 * aDy;
}

// Inflate the rect by the specified margin
void nsRect2::Inflate(const nsMargin2 &aMargin)
{
  x -= aMargin.left;
  y -= aMargin.top;
  width += aMargin.left + aMargin.right;
  height += aMargin.top + aMargin.bottom;
}

// Deflate the rect by the specified width and height
void nsRect2::Deflate(gfx_coord aDx, gfx_coord aDy)
{
  x += aDx;
  y += aDy;
  width -= 2 * aDx;
  height -= 2 * aDy;
}

// Deflate the rect by the specified margin
void nsRect2::Deflate(const nsMargin2 &aMargin)
{
  x += aMargin.left;
  y += aMargin.top;
  width -= aMargin.left + aMargin.right;
  height -= aMargin.top + aMargin.bottom;
}

// scale the rect but round to smallest containing rect
nsRect2& nsRect2::ScaleRoundOut(const float aScale) 
{
  gfx_coord right = GFXCoordCeil((x + width) * aScale);
  gfx_coord bottom = GFXCoordCeil((y + height) * aScale);
  x = GFXCoordFloor(x * aScale);
  y = GFXCoordFloor(y * aScale);
  width = (right - x);
  height = (bottom - y);
  return *this;
}

// scale the rect but round to largest contained rect
nsRect2& nsRect2::ScaleRoundIn(const float aScale) 
{
  gfx_coord right = GFXCoordFloor((x + width) * aScale);
  gfx_coord bottom = GFXCoordFloor((y + height) * aScale);
  x = GFXCoordCeil(x * aScale);
  y = GFXCoordCeil(y * aScale);
  width = (right - x);
  height = (bottom - y);
  return *this;
}

// Diagnostics
#ifdef DEBUG_RECT
FILE* operator<<(FILE* out, const nsRect2& rect)
{
  nsAutoString tmp;

  // Output the coordinates in fractional points so they're easier to read
  tmp.AppendWithConversion("{");
  tmp.AppendFloat(NSTwipsToFloatPoints(rect.x));
  tmp.AppendWithConversion(", ");
  tmp.AppendFloat(NSTwipsToFloatPoints(rect.y));
  tmp.AppendWithConversion(", ");
  tmp.AppendFloat(NSTwipsToFloatPoints(rect.width));
  tmp.AppendWithConversion(", ");
  tmp.AppendFloat(NSTwipsToFloatPoints(rect.height));
  tmp.AppendWithConversion("}");
  fputs(tmp, out);
  return out;
}
#endif
