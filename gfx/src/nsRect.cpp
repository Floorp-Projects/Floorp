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

#include "nsRect.h"
#include "nsString.h"
#include "nsUnitConversion.h"

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

// scale the rect but round to smallest containing rect
nsRect& nsRect::ScaleRoundOut(const float aScale) 
{
  nscoord right = NSToCoordCeil(float(x + width) * aScale);
  nscoord bottom = NSToCoordCeil(float(y + height) * aScale);
  x = NSToCoordFloor(float(x) * aScale);
  y = NSToCoordFloor(float(y) * aScale);
  width = (right - x);
  height = (bottom - y);
  return *this;
}

// scale the rect but round to largest contained rect
nsRect& nsRect::ScaleRoundIn(const float aScale) 
{
  nscoord right = NSToCoordFloor(float(x + width) * aScale);
  nscoord bottom = NSToCoordFloor(float(y + height) * aScale);
  x = NSToCoordCeil(float(x) * aScale);
  y = NSToCoordCeil(float(y) * aScale);
  width = (right - x);
  height = (bottom - y);
  return *this;
}

#ifdef DEBUG
// Diagnostics

FILE* operator<<(FILE* out, const nsRect& rect)
{
  nsAutoString tmp;

  // Output the coordinates in fractional points so they're easier to read
  tmp.AppendLiteral("{");
  tmp.AppendFloat(NSTwipsToFloatPoints(rect.x));
  tmp.AppendLiteral(", ");
  tmp.AppendFloat(NSTwipsToFloatPoints(rect.y));
  tmp.AppendLiteral(", ");
  tmp.AppendFloat(NSTwipsToFloatPoints(rect.width));
  tmp.AppendLiteral(", ");
  tmp.AppendFloat(NSTwipsToFloatPoints(rect.height));
  tmp.AppendLiteral("}");
  fputs(NS_LossyConvertUCS2toASCII(tmp).get(), out);
  return out;
}

#ifdef NS_COORD_IS_FLOAT
// Computes the area in which aRect1 and aRect2 overlap and fills 'this' with
// the result. Returns FALSE if the rectangles don't intersect.
PRBool nsIntRect::IntersectRect(const nsIntRect &aRect1, const nsIntRect &aRect2)
{
  PRInt32  xmost1 = aRect1.XMost();
  PRInt32  ymost1 = aRect1.YMost();
  PRInt32  xmost2 = aRect2.XMost();
  PRInt32  ymost2 = aRect2.YMost();
  PRInt32  temp;

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
PRBool nsIntRect::UnionRect(const nsIntRect &aRect1, const nsIntRect &aRect2)
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
    PRInt32 xmost1 = aRect1.XMost();
    PRInt32 xmost2 = aRect2.XMost();
    PRInt32 ymost1 = aRect1.YMost();
    PRInt32 ymost2 = aRect2.YMost();

    // Compute the origin
    x = MIN(aRect1.x, aRect2.x);
    y = MIN(aRect1.y, aRect2.y);

    // Compute the size
    width = MAX(xmost1, xmost2) - x;
    height = MAX(ymost1, ymost2) - y;
  }

  return result;
}
#endif

#endif // DEBUG
