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
   *
   * Both aDirtyRect and aBorderArea are in the local coordinate space
   * of aForFrame
   */
  static void PaintBorder(nsIPresContext& aPresContext,
                          nsIRenderingContext& aRenderingContext,
                          nsIFrame* aForFrame,
                          const nsRect& aDirtyRect,
                          const nsRect& aBorderArea,
                          const nsStyleSpacing& aBorderStyle,
						  nsIStyleContext* aStyleContext,
                          PRIntn aSkipSides,
                          nsRect* aGap = 0);

  /**
   * Just like PaintBorder, but takes as input a list of border segments
   * rather than a single border style.  Useful for any object that needs to
   * draw a border where an edge is not necessarily homogenous.
   * Render the border for an element using css rendering rules
   * for borders. aSkipSides is a bitmask of the sides to skip
   * when rendering. If 0 then no sides are skipped.
   *
   * Both aDirtyRect and aBorderArea are in the local coordinate space
   * of aForFrame
   */
  static void PaintBorderEdges(nsIPresContext& aPresContext,
                               nsIRenderingContext& aRenderingContext,
                               nsIFrame* aForFrame,
                               const nsRect& aDirtyRect,
                               const nsRect& aBorderArea,
                               nsBorderEdges * aBorderEdges,
							   nsIStyleContext* aStyleContext,
                               PRIntn aSkipSides,
                               nsRect* aGap = 0);


  /**
   * Render the background for an element using css rendering rules
   * for backgrounds.
   *
   * Both aDirtyRect and aBorderArea are in the local coordinate space
   * of aForFrame
   */
  static void PaintBackground(nsIPresContext& aPresContext,
                              nsIRenderingContext& aRenderingContext,
                              nsIFrame* aForFrame,
                              const nsRect& aDirtyRect,
                              const nsRect& aBorderArea,
                              const nsStyleColor& aColor,
                              const nsStyleSpacing& aStyle,
                              nscoord aDX,
                              nscoord aDY);

  static void DrawDashedSides(PRIntn startSide,
                              nsIRenderingContext& aContext,
                              const PRUint8 borderStyles[],
                              const nscolor borderColors[],    
                              const nsRect& borderOutside,
                              const nsRect& borderInside,
                              PRIntn aSkipSides,
                              nsRect* aGap);

  static void DrawDashedSides(PRIntn startSide,
                              nsIRenderingContext& aContext,
                              const nsStyleSpacing& aSpacing,  
                              const nsRect& borderOutside,
                              const nsRect& borderInside,
                              PRIntn aSkipSides,
                              nsRect* aGap);

  /** draw the dashed segements of a segmented border */
  //XXX: boy is it annoying that we have 3 methods to draw dashed sides!
  //     they clearly can be factored.
  static void DrawDashedSegments(nsIRenderingContext& aContext,
                                 const nsRect& aBounds,
                                 nsBorderEdges * aBorderEdges,
                                 PRIntn aSkipSides,
                                 nsRect* aGap);

protected:
  static nscolor MakeBevelColor(PRIntn whichSide, PRUint8 style,
                                nscolor aBackgroundColor,
								nscolor aElementColor,
								PRBool printing,PRBool aSpecialCase);

  static PRIntn MakeSide(nsPoint aPoints[],
                         nsIRenderingContext& aContext,
                         PRIntn whichSide,
                         const nsRect& outside, const nsRect& inside,
                         PRIntn borderPart, float borderFrac,
                         nscoord twipsPerPixel);

  //static void DrawSide(nsIRenderingContext& aContext,
  //                     PRIntn whichSide,
  //                     const PRUint8 borderStyles[],
  //                    const nscolor borderColors[],
  //                     const nsRect& borderOutside,     
  //                     const nsRect& borderInside,
  //                     PRBool printing,
  //                     nscoord twipsPerPixel,
  //                     nsRect* aGap = 0);
  
  static void DrawSide(nsIRenderingContext& aContext,
                       PRIntn whichSide,
                       const PRUint8 borderStyle,
                       const nscolor borderColor,
					   const nscolor aElementColor, 
                       const nscolor aBackgroundColor, 
					   const nsRect& borderOutside,
                       const nsRect& borderInside,
                       PRBool printing,
                       nscoord twipsPerPixel,
                       nsRect* aGap = 0);


  static void DrawLine (nsIRenderingContext& aContext, 
                        nscoord aX1, nscoord aY1, nscoord aX2, nscoord aY2,
                        nsRect* aGap);

  static void FillPolygon (nsIRenderingContext& aContext, 
                           const nsPoint aPoints[],
                           PRInt32 aNumPoints,
                           nsRect* aGap);
};

#endif /* nsCSSRendering_h___ */
