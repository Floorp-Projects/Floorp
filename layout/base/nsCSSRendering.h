/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef nsCSSRendering_h___
#define nsCSSRendering_h___

#include "nsIRenderingContext.h"
struct nsPoint;
class nsIStyleContext;
class nsIPresContext;

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
  static void PaintBorder(nsIPresContext* aPresContext,
                          nsIRenderingContext& aRenderingContext,
                          nsIFrame* aForFrame,
                          const nsRect& aDirtyRect,
                          const nsRect& aBorderArea,
                          const nsStyleBorder& aBorderStyle,
                          nsIStyleContext* aStyleContext,
                          PRIntn aSkipSides,
                          nsRect* aGap = 0,
                          nscoord aHardBorderSize = 0,
                          PRBool aShouldIgnoreRounded = PR_FALSE);

  /**
   * Render the outline for an element using css rendering rules
   * for borders. aSkipSides is a bitmask of the sides to skip
   * when rendering. If 0 then no sides are skipped.
   *
   * Both aDirtyRect and aBorderArea are in the local coordinate space
   * of aForFrame
   */
  static void PaintOutline(nsIPresContext* aPresContext,
                          nsIRenderingContext& aRenderingContext,
                          nsIFrame* aForFrame,
                          const nsRect& aDirtyRect,
                          const nsRect& aBorderArea,
                          const nsStyleBorder& aBorderStyle,
                          const nsStyleOutline& aOutlineStyle,
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
  static void PaintBorderEdges(nsIPresContext* aPresContext,
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
  static void PaintBackground(nsIPresContext* aPresContext,
                              nsIRenderingContext& aRenderingContext,
                              nsIFrame* aForFrame,
                              const nsRect& aDirtyRect,
                              const nsRect& aBorderArea,
                              const nsStyleBackground& aColor,
                              const nsStyleBorder& aBorder,
                              nscoord aDX,
                              nscoord aDY);

  static void DrawDashedSides(PRIntn startSide,
                              nsIRenderingContext& aContext,
                              const nsRect& aDirtyRect,
                              const PRUint8 borderStyles[],
                              const nscolor borderColors[],    
                              const nsRect& borderOutside,
                              const nsRect& borderInside,
                              PRIntn aSkipSides,
                              nsRect* aGap);

  static void DrawDashedSides(PRIntn startSide,
                              nsIRenderingContext& aContext,
                              const nsRect& aDirtyRect,
                              const nsStyleColor* aColorStyle,
                              const nsStyleBorder* aBorderStyle,  
                              const nsStyleOutline* aOutlineStyle,  
                              PRBool aDoOutline,
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

  /**
   * transform a color to a color that will show up on a printer if needed
   * aMapColor - color to evaluate
   * aIsPrinter - Is this a printing device
   * return - the transformed color
   */
  static nscolor TransformColor(nscolor  aMapColor,PRBool aNoBackGround);

protected:
  /**
   * Render the border for an element using css rendering rules
   * for borders. aSkipSides is a bitmask of the sides to skip
   * when rendering. If 0 then no sides are skipped.
   * Both aDirtyRect and aBorderArea are in the local coordinate space
   * of aForFrame
   */
  static void PaintRoundedBorder(nsIPresContext* aPresContext,
                          nsIRenderingContext& aRenderingContext,
                          nsIFrame* aForFrame,
                          const nsRect& aDirtyRect,
                          const nsRect& aBorderArea,
                          const nsStyleBorder* aBorderStyle,
                          const nsStyleOutline* aOutlineStyle,
                          nsIStyleContext* aStyleContext,
                          PRIntn aSkipSides,
                          PRInt16 aBorderRadius[4],nsRect* aGap = 0,
                          PRBool aIsOutline=PR_FALSE);


  static void RenderSide(nsFloatPoint aPoints[],nsIRenderingContext& aRenderingContext,
                        const nsStyleBorder* aBorderStyle,const nsStyleOutline* aOutlineStyle,nsIStyleContext* aStyleContext,
                        PRUint8 aSide,nsMargin  &aBorThick,nscoord aTwipsPerPixel,
                        PRBool aIsOutline=PR_FALSE);

  static void PaintRoundedBackground(nsIPresContext* aPresContext,
                              nsIRenderingContext& aRenderingContext,
                              nsIFrame* aForFrame,
                              const nsRect& aDirtyRect,
                              const nsRect& aBorderArea,
                              const nsStyleBackground& aColor,
                              nscoord aDX,
                              nscoord aDY,
                              PRInt16 aTheRadius[4]);


  static nscolor MakeBevelColor(PRIntn whichSide, PRUint8 style,
                                nscolor aBackgroundColor,
                                nscolor aBorderColor,
                                PRBool aSpecialCase);

  static PRIntn MakeSide(nsPoint aPoints[],
                         nsIRenderingContext& aContext,
                         PRIntn whichSide,
                         const nsRect& outside, const nsRect& inside,
                         PRIntn aSkipSides,
                         PRIntn borderPart, float borderFrac,
                         nscoord twipsPerPixel);

  static void DrawSide(nsIRenderingContext& aContext,
                       PRIntn whichSide,
                       const PRUint8 borderStyle,
                       const nscolor borderColor,
                       const nscolor aBackgroundColor, 
                       const nsRect& borderOutside,
                       const nsRect& borderInside,
                       PRIntn aSkipSides,
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
	nsFloatPoint	mAnc1;
	nsFloatPoint	mCon;
	nsFloatPoint mAnc2;

  QBCurve() {mAnc1.x=0;mAnc1.y=0;mCon=mAnc2=mAnc1;}
  void SetControls(nsFloatPoint &aAnc1,nsFloatPoint &aCon,nsFloatPoint &aAnc2) { mAnc1 = aAnc1; mCon = aCon; mAnc2 = aAnc2;}
  void SetPoints(float a1x,float a1y,float acx,float acy,float a2x,float a2y) {mAnc1.MoveTo(a1x,a1y),mCon.MoveTo(acx,acy),mAnc2.MoveTo(a2x,a2y);}

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
  PRInt32 mRoundness[4];

  PRBool  mDoRound;

  PRInt16 mLeft;
  PRInt16 mRight;
  PRInt16 mTop;
  PRInt16 mBottom;

  /** 
   *  Construct a rounded rectangle object
   *  @update 4/19/99
   */
  void  RoundRect() {mRoundness[0]=0;}

  /**
   *  Set the curves boundaries and then break it up into the curve pieces for rendering
   *  @update 4/13/99 dwc
   *  @param aLeft -- Left side of bounding box
   *  @param aTop -- Top side of bounding box
   *  @param aWidth -- Width of bounding box
   *  @param aHeight -- Height of bounding box
   *  @param aRadius -- radius for the rounding
   */
  void  Set(nscoord aLeft,nscoord aTop,PRInt32  aWidth,PRInt32 aHeight,PRInt16 aRadius[4],PRInt16 aNumTwipPerPix);


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
