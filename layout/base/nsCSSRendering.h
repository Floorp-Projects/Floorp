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
#ifndef nsCSSRendering_h___
#define nsCSSRendering_h___

#include "nsIRenderingContext.h"
#include "nsIStyleContext.h"
struct nsPoint;

class nsCSSRendering {
public:
  /**
   * Render the border for an element using css rendering rules
   * for borders. aSkipSides is a bitmask of the sides to skip
   * when rendering. If 0 then no sides are skipped.
   */
  static void PaintBorder(nsIPresContext& aPresContext,
                          nsIRenderingContext& aRenderingContext,
                          nsIFrame* aForFrame,
                          const nsRect& aDirtyRect,
                          const nsRect& aBounds,
                          const nsStyleSpacing& aStyle,
                          PRIntn aSkipSides);

  /**
   * Render the background for an element using css rendering rules
   * for backgrounds.
   */
  static void PaintBackground(nsIPresContext& aPresContext,
                              nsIRenderingContext& aRenderingContext,
                              nsIFrame* aForFrame,
                              const nsRect& aDirtyRect,
                              const nsRect& aBounds,
                              const nsStyleColor& aColor);

  /**
   * Special method to brighten a Color and have it shift to white when
   * fully saturated.
   */
  static nscolor Brighten(nscolor inColor);

  /**
   * Special method to darken a Color and have it shift to black when
   * darkest component underflows
   */
  static nscolor Darken(nscolor inColor);

  // Weird color computing code stolen from winfe which was stolen
  // from the xfe which was written originally by Eric Bina. So there.
	static void Get3DColors(nscolor aResult[2], nscolor aColor);
  static const int RED_LUMINOSITY;
  static const int GREEN_LUMINOSITY;
  static const int BLUE_LUMINOSITY;
  static const int INTENSITY_FACTOR;
  static const int LIGHT_FACTOR;
  static const int LUMINOSITY_FACTOR;
  static const int MAX_COLOR;
  static const int COLOR_DARK_THRESHOLD;
  static const int COLOR_LIGHT_THRESHOLD;

protected:
  static nscolor MakeBevelColor(PRIntn whichSide, PRUint8 style,
                                nscolor baseColor,
                                PRBool printing);

  static PRIntn MakeSide(nsPoint aPoints[],
                         nsIRenderingContext& aContext,
                         PRIntn whichSide,
                         const nsRect& inside, const nsRect& outside,
                         PRIntn borderPart, float borderFrac);

  static void DrawSide(nsIRenderingContext& aContext,
                       PRIntn whichSide,
                       const PRUint8 borderStyles[],
                       const nscolor borderColors[],
                       const nsRect& borderOutside,
                       const nsRect& borderInside,
                       PRBool printing);

  static void DrawDashedSides(PRIntn startSide,
                              nsIRenderingContext& aContext,
                              const PRUint8 borderStyles[],
                              const nscolor borderColors[],
                              const nsRect& borderOutside,
                              const nsRect& borderInside,
                              PRIntn aSkipSides);
};

#endif /* nsCSSRendering_h___ */
