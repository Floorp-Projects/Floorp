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

#ifndef nsRenderingContextImpl_h___
#define nsRenderingContextImpl_h___

#include "gfxCore.h"
#include "nsIRenderingContext.h"
#include "nsPoint.h"
#include "nsSize.h"


typedef struct {	
    double x;	  // x coordinate of edge's intersection with current scanline */
    double dx;	// change in x with respect to y 
    int i;	    // edge number: edge i goes from mPointList[i] to mPointList[i+1] 
} Edge;

#undef  IMETHOD_VISIBILITY
#define IMETHOD_VISIBILITY NS_VISIBILITY_DEFAULT

class nsRenderingContextImpl : public nsIRenderingContext
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

  // See the description in nsIRenderingContext.h
  NS_IMETHOD FlushRect(const nsRect& aRect);
  NS_IMETHOD FlushRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight);

  /**
   * Gets the Pen Mode for the RenderingContext
   * @param aPenMode The Pen Mode to be retrieved
   * @return NS_OK if the Pen Mode is correctly retrieved
   */
  NS_IMETHOD GetPenMode(nsPenMode &aPenMode) { return NS_ERROR_FAILURE;}

  /**
   * Sets the Pen Mode for the RenderingContext 
   * @param aPenMode The Pen Mode
   * @return NS_OK if the Pen Mode is correctly set
   */
  NS_IMETHOD SetPenMode(nsPenMode aPenMode) { return NS_ERROR_FAILURE;};


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

  NS_IMETHOD GetBackbuffer(const nsRect &aRequestedSize, const nsRect &aMaxSize, nsDrawingSurface &aBackbuffer); 
  NS_IMETHOD ReleaseBackbuffer(void);
  NS_IMETHOD DestroyCachedBackbuffer(void);
  NS_IMETHOD UseBackbuffer(PRBool* aUseBackbuffer);
  
  /**
   * Let the device context know whether we want text reordered with
   * right-to-left base direction
   */
  NS_IMETHOD SetRightToLeftText(PRBool aIsRTL);

  NS_IMETHOD DrawImage(imgIContainer *aImage, const nsRect & aSrcRect, const nsRect & aDestRect);
  NS_IMETHOD DrawTile(imgIContainer *aImage, nscoord aXOffset, nscoord aYOffset, const nsRect * aTargetRect);

  NS_IMETHOD RenderPostScriptDataFragment(const unsigned char *aData, unsigned long aDatalen);

protected:
  virtual ~nsRenderingContextImpl();

  void cdelete(int i);
  void cinsert(int i,int y,const nsPoint aPointArray[],PRInt32 aNumPts);

  /**
   * Determine if a rect's width and height will fit within a specified width and height
   * @param aRect rectangle to test
   * @param aWidth width to determine if the rectangle's width will fit within
   * @param aHeight height to determine if the rectangles height will fit within
   * @returns PR_TRUE if the rect width and height fits with aWidth, aHeight, PR_FALSE
   * otherwise.
   */

  PRBool RectFitsInside(const nsRect& aRect, PRInt32 aWidth, PRInt32 aHeight) const;

  /**
   * Determine if two rectangles width and height will fit within a specified width and height
   * @param aRect1 first rectangle to test
   * @param aRect1 second rectangle to test
   * @param aWidth width to determine if both rectangle's width will fit within
   * @param aHeight height to determine if both rectangles height will fit within
   * @returns PR_TRUE if the rect1's and rect2's width and height fits with aWidth,
   * aHeight, PR_FALSE otherwise.
   */
  PRBool BothRectsFitInside(const nsRect& aRect1, const nsRect& aRect2, PRInt32 aWidth, PRInt32 aHeight, nsRect& aNewSize) const;

  /**
   * Return an offscreen surface size from a set of discrete surface sizes.
   * The smallest discrete surface size that can enclose both the Maximum widget 
   * size (@see GetMaxWidgetBounds) and the requested size is returned.
   *
   * @param aMaxBackbufferSize Maximum size that may be requested for the backbuffer
   * @param aRequestedSize Requested size for the offscreen.
   * @param aSurfaceSize contains the surface size 
   */
  void CalculateDiscreteSurfaceSize(const nsRect& aMaxBackbufferSize, const nsRect& aRequestedSize, nsRect& aSize);

  /**
   * Get the size of the offscreen drawing surface..
   *
   * @param aMaxBackbufferSize Maximum size that may be requested for the backbuffer
   * @param aRequestedSize Desired size for the offscreen.
   * @param aSurfaceSize   Offscreen adjusted to a discrete size which encloses aRequestedSize.
   */
  void GetDrawingSurfaceSize(const nsRect& aMaxBackbufferSize, const nsRect& aRequestedSize, nsRect& aSurfaceSize);

  /**
   * Utility method used to implement NS_IMETHOD GetBackbuffer
   *
   * @param aRequestedSize size of the backbuffer area requested
   * @param aMaxSize maximum size that may be requested for the backbuffer
   * @param aBackbuffer drawing surface used as the backbuffer
   * @param aCacheBackbuffer PR_TRUE then the backbuffer will be cached, if PR_FALSE it is created each time
   */
  nsresult AllocateBackbuffer(const nsRect &aRequestedSize, const nsRect &aMaxSize, nsDrawingSurface &aBackbuffer, PRBool aCacheBackbuffer);

public:

protected:
  nsPenMode   mPenMode;
private:
  static nsDrawingSurface  gBackbuffer;         //singleton backbuffer 
  static nsRect            gBackbufferBounds;   //backbuffer bounds
    // Largest requested offscreen size if larger than a full screen.
  static nsSize            gLargestRequestedSize;

};

#undef  IMETHOD_VISIBILITY
#define IMETHOD_VISIBILITY NS_VISIBILITY_HIDDEN

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
