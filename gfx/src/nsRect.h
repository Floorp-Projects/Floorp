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
#include "mozilla/BaseRect.h"

struct nsIntRect;

struct NS_GFX nsRect :
  public mozilla::BaseRect<nscoord, nsRect, nsPoint, nsSize, nsMargin> {
  typedef mozilla::BaseRect<nscoord, nsRect, nsPoint, nsSize, nsMargin> Super;

  static void VERIFY_COORD(nscoord aValue) { ::VERIFY_COORD(aValue); }

  // Constructors
  nsRect() : Super()
  {
    MOZ_COUNT_CTOR(nsRect);
  }
  nsRect(const nsRect& aRect) : Super(aRect)
  {
    MOZ_COUNT_CTOR(nsRect);
  }
  nsRect(const nsPoint& aOrigin, const nsSize &aSize) : Super(aOrigin, aSize)
  {
    MOZ_COUNT_CTOR(nsRect);
  }
  nsRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight) :
      Super(aX, aY, aWidth, aHeight)
  {
    MOZ_COUNT_CTOR(nsRect);
  }

#ifdef NS_BUILD_REFCNT_LOGGING
  ~nsRect() {
    MOZ_COUNT_DTOR(nsRect);
  }
#endif

  // Extend the rect outwards such that the edges are on integer boundaries
  // and the edges scaled by aXMult/aYMult are also on integer boundaries.
  // aXMult/aYMult must be N or 1/N for integer N.
  nsRect& ExtendForScaling(float aXMult, float aYMult);

  // Converts this rect from aFromAPP, an appunits per pixel ratio, to aToAPP.
  // In the RoundOut version we make the rect the smallest rect containing the
  // unrounded result. In the RoundIn version we make the rect the largest rect
  // contained in the unrounded result.
  inline nsRect ConvertAppUnitsRoundOut(PRInt32 aFromAPP, PRInt32 aToAPP) const;
  inline nsRect ConvertAppUnitsRoundIn(PRInt32 aFromAPP, PRInt32 aToAPP) const;

  inline nsIntRect ToNearestPixels(nscoord aAppUnitsPerPixel) const;
  inline nsIntRect ToOutsidePixels(nscoord aAppUnitsPerPixel) const;
  inline nsIntRect ToInsidePixels(nscoord aAppUnitsPerPixel) const;
};

struct NS_GFX nsIntRect :
  public mozilla::BaseRect<PRInt32, nsIntRect, nsIntPoint, nsIntSize, nsIntMargin> {
  typedef mozilla::BaseRect<PRInt32, nsIntRect, nsIntPoint, nsIntSize, nsIntMargin> Super;

  // Constructors
  nsIntRect() : Super()
  {
  }
  nsIntRect(const nsIntRect& aRect) : Super(aRect)
  {
  }
  nsIntRect(const nsIntPoint& aOrigin, const nsIntSize &aSize) : Super(aOrigin, aSize)
  {
  }
  nsIntRect(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight) :
      Super(aX, aY, aWidth, aHeight)
  {
  }

  inline nsRect ToAppUnits(nscoord aAppUnitsPerPixel) const;

  // Returns a special nsIntRect that's used in some places to signify
  // "all available space".
  static const nsIntRect& GetMaxSizedIntRect() { return kMaxSizedIntRect; }

  // This is here only to keep IPDL-generated code happy. DO NOT USE.
  bool operator==(const nsIntRect& aRect) const
  {
    return IsEqualEdges(aRect);
  }

protected:
  static const nsIntRect kMaxSizedIntRect;
};

/*
 * App Unit/Pixel conversions
 */

inline nsRect
nsRect::ConvertAppUnitsRoundOut(PRInt32 aFromAPP, PRInt32 aToAPP) const
{
  if (aFromAPP == aToAPP) {
    return *this;
  }

  nsRect rect;
  nscoord right = NSToCoordCeil(NSCoordScale(XMost(), aFromAPP, aToAPP));
  nscoord bottom = NSToCoordCeil(NSCoordScale(YMost(), aFromAPP, aToAPP));
  rect.x = NSToCoordFloor(NSCoordScale(x, aFromAPP, aToAPP));
  rect.y = NSToCoordFloor(NSCoordScale(y, aFromAPP, aToAPP));
  rect.width = (right - rect.x);
  rect.height = (bottom - rect.y);

  return rect;
}

inline nsRect
nsRect::ConvertAppUnitsRoundIn(PRInt32 aFromAPP, PRInt32 aToAPP) const
{
  if (aFromAPP == aToAPP) {
    return *this;
  }

  nsRect rect;
  nscoord right = NSToCoordFloor(NSCoordScale(XMost(), aFromAPP, aToAPP));
  nscoord bottom = NSToCoordFloor(NSCoordScale(YMost(), aFromAPP, aToAPP));
  rect.x = NSToCoordCeil(NSCoordScale(x, aFromAPP, aToAPP));
  rect.y = NSToCoordCeil(NSCoordScale(y, aFromAPP, aToAPP));
  rect.width = (right - rect.x);
  rect.height = (bottom - rect.y);

  return rect;
}

// scale the rect but round to preserve centers
inline nsIntRect
nsRect::ToNearestPixels(nscoord aAppUnitsPerPixel) const
{
  nsIntRect rect;
  rect.x = NSToIntRoundUp(NSAppUnitsToDoublePixels(x, aAppUnitsPerPixel));
  rect.y = NSToIntRoundUp(NSAppUnitsToDoublePixels(y, aAppUnitsPerPixel));
  rect.width  = NSToIntRoundUp(NSAppUnitsToDoublePixels(XMost(),
                               aAppUnitsPerPixel)) - rect.x;
  rect.height = NSToIntRoundUp(NSAppUnitsToDoublePixels(YMost(),
                               aAppUnitsPerPixel)) - rect.y;
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
