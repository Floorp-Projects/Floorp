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
  /**
   * Render the border for an element using css rendering rules
   * for borders. aSkipSides is a bitmask of the sides to skip
   * when rendering. If 0 then no sides are skipped.
   * Both aDirtyRect and aBorderArea are in the local coordinate space
   * of aForFrame
   */
  static void PaintRoundedBorder(nsIPresContext& aPresContext,
                          nsIRenderingContext& aRenderingContext,
                          nsIFrame* aForFrame,
                          const nsRect& aDirtyRect,
                          const nsRect& aBorderArea,
                          const nsStyleSpacing& aBorderStyle,
                          nsIStyleContext* aStyleContext,
                          PRIntn aSkipSides,
                          PRInt16 aBorderRadius,nsRect* aGap = 0);


  static void RenderSide(nsPoint aPoints[],nsIRenderingContext& aRenderingContext,
                        const nsStyleSpacing& aBorderStyle,nsIStyleContext* aStyleContext,
                        PRUint8 aSide,nsMargin  &aBorThick,nscoord aTwipsPerPixel);

  static void PaintRoundedBackground(nsIPresContext& aPresContext,
                              nsIRenderingContext& aRenderingContext,
                              nsIFrame* aForFrame,
                              const nsRect& aDirtyRect,
                              const nsRect& aBorderArea,
                              const nsStyleColor& aColor,
                              const nsStyleSpacing& aStyle,
                              nscoord aDX,
                              nscoord aDY,
                              PRInt16 aTheRadius);


  static nscolor MakeBevelColor(PRIntn whichSide, PRUint8 style,
                                nscolor aBackgroundColor,
                                nscolor aBorderColor,
                                PRBool aSpecialCase);

  static PRIntn MakeSide(nsPoint aPoints[],
                         nsIRenderingContext& aContext,
                         PRIntn whichSide,
                         const nsRect& outside, const nsRect& inside,
                         PRIntn borderPart, float borderFrac,
                         nscoord twipsPerPixel);

  static void DrawSide(nsIRenderingContext& aContext,
                       PRIntn whichSide,
                       const PRUint8 borderStyle,
                       const nscolor borderColor,
                       const nscolor aBackgroundColor, 
                       const nsRect& borderOutside,
                       const nsRect& borderInside,
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



/** ---------------------------------------------------
 *  Class QBCurve, a quadratic bezier curve, used to implement the rounded rectangles
 *	@update 3/26/99 dwc
 */
class QBCurve
{

public:
	nsPoint	mAnc1;
	nsPoint	mCon;
	nsPoint mAnc2;

  QBCurve() {mAnc1.x=0;mAnc1.y=0;mCon=mAnc2=mAnc1;}
  void SetControls(nsPoint &aAnc1,nsPoint &aCon,nsPoint &aAnc2) { mAnc1 = aAnc1; mCon = aCon; mAnc2 = aAnc2;}
  void SetPoints(nscoord a1x,nscoord a1y,nscoord acx,nscoord acy,nscoord a2x,nscoord a2y) {mAnc1.MoveTo(a1x,a1y),mCon.MoveTo(acx,acy),mAnc2.MoveTo(a2x,a2y);}

/** ---------------------------------------------------
 *  Divide a Quadratic curve into line segments if it is not smaller than a certain size
 *  else it is so small that it can be approximated by 2 lineto calls
 *  @param aRenderingContext -- The RenderingContext to use to draw with
 *  @param aPointArray[] -- A list of points we can put line calls into instead of drawing.  If null, lines are drawn
 *  @param aCurInex -- a pointer to an Integer that tells were to put the points into the array, incremented when finished
 *	@update 3/26/99 dwc
 */
  void SubDivide(nsIRenderingContext *aRenderingContext,nsPoint  aPointArray[],PRInt32 *aCurIndex);

/** ---------------------------------------------------
 *  Divide a Quadratic Bezier curve at the mid-point
 *	@update 3/26/99 dwc
 *  @param aCurve1 -- Curve 1 as a result of the division
 *  @param aCurve2 -- Curve 2 as a result of the division
 */
  void MidPointDivide(QBCurve *A,QBCurve *B);
};


/** ---------------------------------------------------
 *  Class RoundedRect, A class to encapsulate all the rounded rect functionality, 
 *  which are based on the QBCurve
 *	@update 4/13/99 dwc
 */
class RoundedRect
{

public:
  PRInt32 mRoundness;

  PRInt16 mOuterLeft;
  PRInt16 mOuterRight;
  PRInt16 mOuterTop;
  PRInt16 mOuterBottom;
  PRInt16 mInnerLeft;
  PRInt16 mInnerRight;
  PRInt16 mInnerTop;
  PRInt16 mInnerBottom;

  /** 
   *  Construct a rounded rectangle object
   *  @update 4/19/99
   */
  void  RoundRect() {mRoundness=0;}

  /**
   *  Set the curves boundaries and then break it up into the curve pieces for rendering
   *  @update 4/13/99 dwc
   *  @param aLeft -- Left side of bounding box
   *  @param aTop -- Top side of bounding box
   *  @param aWidth -- Width of bounding box
   *  @param aHeight -- Height of bounding box
   *  @param aRadius -- radius for the rounding
   */
  void  Set(nscoord aLeft,nscoord aTop,PRInt32  aWidth,PRInt32 aHeight,PRInt16 aRadius);


  /**
   *  Calculate the inset of a curve based on a border
   *  @update 4/13/99 dwc
   *  @param aLeft -- Left side of bounding box
   *  @param aTop -- Top side of bounding box
   */
  void  CalcInsetCurves(QBCurve &aULCurve,QBCurve &aURCurve,QBCurve &aLLCurve,QBCurve &aLRCurve,nsMargin &aBorder);

  /** ---------------------------------------------------
   *  set the passed in curves to the rounded borders of the rectangle
   *	@update 4/13/99 dwc
   *  @param aULCurve -- upperleft curve
   *  @param aURCurve -- upperright curve
   *  @param aLRCurve -- lowerright curve
   *  @param aLLCurve -- lowerleft curve
   */
  void GetRoundedBorders(QBCurve &aULCurve,QBCurve &aURCurve,QBCurve &aLLCurve,QBCurve &aLRCurve);

};


#endif /* nsCSSRendering_h___ */
