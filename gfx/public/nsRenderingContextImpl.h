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

#ifndef nsRenderingContextImpl_h___
#define nsRenderingContextImpl_h___

#include "nsIRenderingContext.h"
#include "nsPoint.h"


typedef struct {	
    double x;	  // x coordinate of edge's intersection with current scanline */
    double dx;	// change in x with respect to y 
    int i;	    // edge number: edge i goes from mPointList[i] to mPointList[i+1] 
} Edge;


class NS_GFX nsRenderingContextImpl : public nsIRenderingContext
{

// CLASS MEMBERS
public:


protected:
  nsTransform2D		  *mTranMatrix;				// The rendering contexts transformation matrix
  nsLineStyle       mLineStyle;					// The current linestyle, currenly used on mac, other platfroms to follow
  int               mAct;		        		// number of active edges 
  Edge              *mActive;	      		// active edge list:edges crossing scanline y

public:
  nsRenderingContextImpl();


// CLASS METHODS
  /** ---------------------------------------------------
   *  See documentation in nsIRenderingContext.h
   *	@update 03/29/00 dwc
   */
  NS_IMETHOD DrawPath(nsPathPoint aPointArray[],PRInt32 aNumPts);

    /** ---------------------------------------------------
   *  See documentation in nsIRenderingContext.h
   *	@update 03/29/00 dwc
   */
  NS_IMETHOD FillPath(nsPathPoint aPointArray[],PRInt32 aNumPts);

  /**
   * Fill a poly in the current foreground color
   * @param aPoints points to use for the drawing, last must equal first
   * @param aNumPonts number of points in the polygon
   */
  NS_IMETHOD RasterPolygon(const nsPoint aPoints[], PRInt32 aNumPoints);

  /** ---------------------------------------------------
   *  See documentation in nsIRenderingContext.h
   *	@update 05/01/00 dwc
   */
  NS_IMETHOD DrawStdLine(nscoord aX0, nscoord aY0, nscoord aX1, nscoord aY1) { return NS_OK;}

  /** ---------------------------------------------------
   *  See documentation in nsIRenderingContext.h
   *	@update 05/01/00 dwc
   */
  NS_IMETHOD FillStdPolygon(const nsPoint aPoints[], PRInt32 aNumPoints) { return NS_OK; }

#ifdef IBMBIDI
  /**
   * Let the device context know whether we want text reordered with
   * right-to-left base direction
   */
  NS_IMETHOD SetRightToLeftText(PRBool aIsRTL);
#endif // IBMBIDI

#ifdef USE_IMG2
  NS_IMETHOD DrawImage(imgIContainer *aImage, const nsRect * aSrcRect, const nsPoint * aDestPoint);
  NS_IMETHOD DrawScaledImage(imgIContainer *aImage, const nsRect * aSrcRect, const nsRect * aDestRect);
  NS_IMETHOD DrawTile(imgIContainer *aImage, nscoord aXOffset, nscoord aYOffset, const nsRect * aTargetRect);
  NS_IMETHOD DrawScaledTile(imgIContainer *aImage, nscoord aXOffset, nscoord aYOffset, nscoord aTileWidth, nscoord aTileHeight, const nsRect * aTargetRect);
#endif


protected:
  virtual ~nsRenderingContextImpl();

  /** ---------------------------------------------------
   *  Check to see if the given size of tile can be imaged by the RenderingContext
   *	@update 03/29/00 dwc
   *  @param aWidth The width of the tile
   *  @param aHeight The height of the tile
   *  @return PR_TRUE the RenderingContext can handle this tile
   */
  virtual PRBool CanTile(nscoord aWidth,nscoord aHeight) { return PR_FALSE; }

  
  /** ---------------------------------------------------
   *  A bit blitter to tile images to the background recursively
   *	@update 3/29/00 dwc
   *  @param aDS -- Target drawing surface for the rendering context
   *  @param aSrcRect -- Rectangle we are build with the image
   *  @param aHeight -- height of the tile
   *  @param aWidth -- width of the tile
   */
  void  TileImage(nsDrawingSurface  aDS,nsRect &aSrcRect,PRInt16 aWidth,PRInt16 aHeight);

  void cdelete(int i);
  void cinsert(int i,int y,const nsPoint aPointArray[],PRInt32 aNumPts);

public:

};


/** ---------------------------------------------------
 *  Class QBezierCurve, a quadratic bezier curve
 *	@update 4/27/2000 dwc
 */
class QBezierCurve
{

public:
	nsFloatPoint	mAnc1;
	nsFloatPoint	mCon;
	nsFloatPoint  mAnc2;

  QBezierCurve() {mAnc1.x=0;mAnc1.y=0;mCon=mAnc2=mAnc1;}
  void SetControls(nsFloatPoint &aAnc1,nsFloatPoint &aCon,nsFloatPoint &aAnc2) { mAnc1 = aAnc1; mCon = aCon; mAnc2 = aAnc2;}
  void SetPoints(nscoord a1x,nscoord a1y,nscoord acx,nscoord acy,nscoord a2x,nscoord a2y) {mAnc1.MoveTo(a1x,a1y),mCon.MoveTo(acx,acy),mAnc2.MoveTo(a2x,a2y);}
  void SetPoints(float a1x,float a1y,float acx,float acy,float a2x,float a2y) {mAnc1.MoveTo(a1x,a1y),mCon.MoveTo(acx,acy),mAnc2.MoveTo(a2x,a2y);}
  void DebugPrint();
/** ---------------------------------------------------
 *  Divide a Quadratic curve into line segments if it is not smaller than a certain size
 *  else it is so small that it can be approximated by 2 lineto calls
 *  @param aRenderingContext -- The RenderingContext to use to draw with
 *	@update 3/26/99 dwc
 */
  void SubDivide(nsIRenderingContext *aRenderingContext);

/** ---------------------------------------------------
 *  Divide a Quadratic curve into line segments if it is not smaller than a certain size
 *  else it is so small that it can be approximated by 2 lineto calls
 *  @param nsPoint* -- The points array to rasterize into
 *  @param aNumPts* -- Current number of points in this array
 *	@update 3/26/99 dwc
 */
  void SubDivide(nsPoint aThePoints[],PRInt16 *aNumPts);

/** ---------------------------------------------------
 *  Divide a Quadratic Bezier curve at the mid-point
 *	@update 3/26/99 dwc
 *  @param aCurve1 -- Curve 1 as a result of the division
 *  @param aCurve2 -- Curve 2 as a result of the division
 */
  void MidPointDivide(QBezierCurve *A,QBezierCurve *B);
};

  enum eSegType {eUNDEF,eLINE,eQCURVE,eCCURVE};


/** ---------------------------------------------------
 *  A class to iterate through a nsPathPoint array and return segments
 *	@update 04/27/00 dwc
 */
class nsPathIter {

public:
  enum eSegType {eUNDEF,eLINE,eQCURVE,eCCURVE};

private:
  PRUint32    mCurPoint;
  PRUint32    mNumPoints;
  nsPathPoint *mThePath;

public:
  nsPathIter();
  nsPathIter(nsPathPoint* aThePath,PRUint32 aNumPts);

  PRBool  NextSeg(QBezierCurve& TheSegment,eSegType& aCurveType);

};


#endif /* nsRenderingContextImpl */
