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

#include "nsCOMPtr.h"
#include "nsRenderingContextImpl.h"
#include "nsIDeviceContext.h"
#include "nsIImage.h"
#include "nsTransform2D.h"
#include <stdlib.h>


const nsPoint *gPts;

nsDrawingSurface nsRenderingContextImpl::gBackbuffer = nsnull;
nsRect nsRenderingContextImpl::gBackbufferBounds = nsRect(0, 0, 0, 0);
nsSize nsRenderingContextImpl::gLargestRequestedSize = nsSize(0, 0);

// comparison routines for qsort 
PRInt32 PR_CALLBACK compare_ind(const void *u,const void *v){return gPts[(PRInt32)*((PRInt32*)u)].y <= gPts[(PRInt32)*((PRInt32*)v)].y ? -1 : 1;}
PRInt32 PR_CALLBACK compare_active(const void *u,const void *v){return ((Edge*)u)->x <= ((Edge*)v)->x ? -1 : 1;}


/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 3/16/00 dwc
 */
nsRenderingContextImpl :: nsRenderingContextImpl()
: mTranMatrix(nsnull)
, mLineStyle(nsLineStyle_kSolid)
, mAct(0)
, mActive(nsnull)
, mPenMode(nsPenMode_kNone)
{
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 3/16/00 dwc
 */
nsRenderingContextImpl :: ~nsRenderingContextImpl()
{


}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 3/29/00 dwc
 */
NS_IMETHODIMP 
nsRenderingContextImpl::DrawPath(nsPathPoint aPointArray[],PRInt32 aNumPts)
{
PRInt32               i;
nsPathPoint           pts[20];
nsPathPoint           *pp0,*np=0,*pp;
QBezierCurve          thecurve;
nsPathIter::eSegType  curveType;


  // Transform the points first
  if (aNumPts > 20){
    pp0 = new nsPathPoint[aNumPts];
  } else {
    pp0 = &pts[0];
  }
  pp = pp0;
  np = &aPointArray[0];

	for ( i= 0; i < aNumPts; i++,np++,pp++){
		pp->x = np->x;
		pp->y = np->y;
    pp->mIsOnCurve = np->mIsOnCurve;
		mTranMatrix->TransformCoord((int*)&pp->x,(int*)&pp->y);
	}

  nsPathIter thePathIter (pp0,aNumPts);
	while ( thePathIter.NextSeg(thecurve,curveType) ) {
    // draw the curve we found
    if(nsPathIter::eLINE == curveType){
      DrawStdLine(NSToCoordRound(thecurve.mAnc1.x),NSToCoordRound(thecurve.mAnc1.y),NSToCoordRound(thecurve.mAnc2.x),NSToCoordRound(thecurve.mAnc2.y));
    } else {
      thecurve.SubDivide(this);
    }
	}

  // Release temporary storage if necessary
  if (pp0 != pts)
    delete [] pp0;

  return NS_OK;
}


#define MAXPATHSIZE 1000

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 3/29/00 dwc
 */
NS_IMETHODIMP 
nsRenderingContextImpl::FillPath(nsPathPoint aPointArray[],PRInt32 aNumPts)
{
PRInt32               i;
nsPathPoint           pts[20];
nsPathPoint           *pp0,*np=0,*pp;
QBezierCurve          thecurve;
nsPathIter::eSegType  curveType;
nsPoint               thePath[MAXPATHSIZE];
PRInt16               curPoint=0;

  // Transform the points first
  if (aNumPts > 20){
    pp0 = new nsPathPoint[aNumPts];
  } else {
    pp0 = &pts[0];
  }
  pp = pp0;
  np = &aPointArray[0];

	for ( i= 0; i < aNumPts; i++,np++,pp++){
		pp->x = np->x;
		pp->y = np->y;
    pp->mIsOnCurve = np->mIsOnCurve;
		mTranMatrix->TransformCoord((int*)&pp->x,(int*)&pp->y);
	}

  nsPathIter thePathIter (pp0,aNumPts);
	while ( thePathIter.NextSeg(thecurve,curveType) ) {
    // build a polygon with the points
    if(nsPathIter::eLINE == curveType){
      thePath[curPoint++].MoveTo(NSToCoordRound(thecurve.mAnc1.x),NSToCoordRound(thecurve.mAnc1.y));
      thePath[curPoint++].MoveTo(NSToCoordRound(thecurve.mAnc2.x),NSToCoordRound(thecurve.mAnc2.y));
    } else {
      thecurve.SubDivide(thePath,&curPoint);
    }
	}

  this->FillStdPolygon(thePath,curPoint);

  // Release temporary storage if necessary
  if (pp0 != pts)
    delete [] pp0;

  return NS_OK;
}


/** ---------------------------------------------------
 *  See documentation in nsRenderingContextImpl.h
 *	@update 3/29/00 dwc
 */
NS_IMETHODIMP
nsRenderingContextImpl::RasterPolygon(const nsPoint aPointArray[],PRInt32 aNumPts)
{
PRInt32       k,y0,y1,y,i,j,xl,xr,extra=0;
PRInt32       *ind;
nsPoint       pts[20];
nsPoint       *pp,*pp0;
const nsPoint *np;

  if (aNumPts<=0)
    return NS_OK;

#ifdef XP_WIN
  extra = 1;
#endif

  // Transform the points first
  if (aNumPts > 20){
    pp0 = new nsPoint[aNumPts];
  } else {
    pp0 = &pts[0];
  }
  pp = pp0;
  np = &aPointArray[0];

	for ( i= 0; i < aNumPts; i++,np++,pp++){
		pp->x = np->x;
		pp->y = np->y;
		mTranMatrix->TransformCoord((PRInt32*)&pp->x,(PRInt32*)&pp->y);
	}

  ind = new PRInt32[aNumPts];
  mActive = new Edge[aNumPts];

  gPts = pp0;

  // create y-sorted array of indices ind[k] into vertex list 
  for (k=0; k<aNumPts; k++)
	  ind[k] = k;
  qsort(ind, aNumPts, sizeof ind[0], compare_ind);	// sort ind by mPointList[ind[k]].y 

  mAct = 0;			// start with empty active list 
  k = 0;				// ind[k] is next vertex to process 
  y0 = (PRInt32)ceil(pp0[ind[0]].y-.5);
  y1 = (PRInt32)floor(pp0[ind[aNumPts-1]].y-.5);

  for (y=y0; y<=y1; y++) {		// step through scanlines
	// check vertices between previous scanline and current one, if any */
	  for (; k<aNumPts && pp0[ind[k]].y<=y+.5; k++) {
	    i = ind[k];	
	    j = i>0 ? i-1 : aNumPts-1;	
	    if (pp0[j].y <= y-.5)	
		    cdelete(j);
	    else if (pp0[j].y > y+.5)	
		    cinsert(j, y,pp0, aNumPts);
	    j = i<aNumPts-1 ? i+1 : 0;	
	    if (pp0[j].y <= y-.5)	
		    cdelete(i);
	    else if (pp0[j].y > y+.5)	
		    cinsert(i, y,pp0, aNumPts);
	  }

	  // sort active edge list by active[j].x 
	  qsort(mActive, mAct, sizeof mActive[0], compare_active);

	  // draw horizontal segments for scanline y
	  for (j=0; j<mAct; j+=2) {	// draw horizontal segments
	    xl = (PRInt32) ceil(mActive[j].x-.5);		/* left end of span */

	    xr = (PRInt32)floor(mActive[j+1].x-.5);	/* right end of span */

      if(xl<=xr){
        DrawStdLine(xl,y,xr+extra,y);
      }
	    mActive[j].x += mActive[j].dx;	/* increment edge coords */
	    mActive[j+1].x += mActive[j+1].dx;
	  }
  }

  delete[] ind;
  delete[] mActive;

  if (pp0 != pts)
    delete [] pp0;

  return NS_OK;
}



/**-------------------------------------------------------------------
 * remove edge i from active list
 * @update dc 12/06/1999
 */
void
nsRenderingContextImpl::cdelete(PRInt32 i)		
{
PRInt32 j;

  for(j=0;j<mAct;j++){
    if (mActive[j].i==i)
      break;
  }

  if (j>=mAct) 
    return;	
  mAct--;
  memcpy(&mActive[j], &mActive[j+1],(mAct-j)*sizeof mActive[0]);
}

/**-------------------------------------------------------------------
 * append edge i to end of active list 
 * @update dc 12/06/1999
 */
void 
nsRenderingContextImpl::cinsert(PRInt32 i,PRInt32 y,const nsPoint aPointArray[],PRInt32 aNumPts)		
{
PRInt32       j;
double        dx;
const nsPoint *p, *q;

  j = i<aNumPts-1 ? i+1 : 0;
  if (aPointArray[i].y < aPointArray[j].y) {
    p = &aPointArray[i]; q = &aPointArray[j];
  }else{
    p = &aPointArray[j]; q = &aPointArray[i];
  }
  // initialize x position at intersection of edge with scanline y
  mActive[mAct].dx = dx = ((double)q->x-(double)p->x)/((double)q->y-(double)p->y);
  mActive[mAct].x = dx*(y+.5-(double)p->y)+(double)p->x;
  mActive[mAct].i = i;
  mAct++;
}

NS_IMETHODIMP nsRenderingContextImpl::GetBackbuffer(const nsRect &aRequestedSize, const nsRect &aMaxSize, nsDrawingSurface &aBackbuffer)
{
  // Default implementation assumes the backbuffer will be cached.
  // If the platform implementation does not require the backbuffer to
  // be cached override this method and make the following call instead:
  // AllocateBackbuffer(aRequestedSize, aMaxSize, aBackbuffer, PR_FALSE);
  return AllocateBackbuffer(aRequestedSize, aMaxSize, aBackbuffer, PR_TRUE);
}

nsresult nsRenderingContextImpl::AllocateBackbuffer(const nsRect &aRequestedSize, const nsRect &aMaxSize, nsDrawingSurface &aBackbuffer, PRBool aCacheBackbuffer)
{
  nsRect newBounds;
  nsresult rv = NS_OK;

   if (! aCacheBackbuffer) {
    newBounds = aRequestedSize;
  } else {
    GetDrawingSurfaceSize(aMaxSize, aRequestedSize, newBounds);
  }

  if ((nsnull == gBackbuffer)
      || (gBackbufferBounds.width != newBounds.width)
      || (gBackbufferBounds.height != newBounds.height))
    {
      if (gBackbuffer) {
        //destroy existing DS
        DestroyDrawingSurface(gBackbuffer);
        gBackbuffer = nsnull;
      }

      rv = CreateDrawingSurface(newBounds, 0, gBackbuffer);
      //   printf("Allocating a new drawing surface %d %d\n", newBounds.width, newBounds.height);
      if (NS_SUCCEEDED(rv)) {
        gBackbufferBounds = newBounds;
        SelectOffScreenDrawingSurface(gBackbuffer);
      } else {
        gBackbufferBounds.SetRect(0,0,0,0);
        gBackbuffer = nsnull;
      }
    } else {
      SelectOffScreenDrawingSurface(gBackbuffer);

      float p2t;
      nsCOMPtr<nsIDeviceContext>  dx;
      GetDeviceContext(*getter_AddRefs(dx));
      p2t = dx->DevUnitsToAppUnits();
      nsRect bounds = aRequestedSize;
      bounds *= p2t;

      SetClipRect(bounds, nsClipCombine_kReplace);
    }

  aBackbuffer = gBackbuffer;
  return rv;
}

NS_IMETHODIMP nsRenderingContextImpl::ReleaseBackbuffer(void)
{
  // If the platform does not require the backbuffer to be cached
  // override this method and call DestroyCachedBackbuffer
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextImpl::DestroyCachedBackbuffer(void)
{
  if (gBackbuffer) {
    DestroyDrawingSurface(gBackbuffer);
    gBackbuffer = nsnull;
  }
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextImpl::UseBackbuffer(PRBool* aUseBackbuffer)
{
  *aUseBackbuffer = PR_TRUE;
  return NS_OK;
}


PRBool nsRenderingContextImpl::RectFitsInside(const nsRect& aRect, PRInt32 aWidth, PRInt32 aHeight) const
{
  if (aRect.width > aWidth)
    return (PR_FALSE);

  if (aRect.height > aHeight)
    return (PR_FALSE);

  return PR_TRUE;
}

PRBool nsRenderingContextImpl::BothRectsFitInside(const nsRect& aRect1, const nsRect& aRect2, PRInt32 aWidth, PRInt32 aHeight, nsRect& aNewSize) const
{
  if (PR_FALSE == RectFitsInside(aRect1, aWidth, aHeight)) {
    return PR_FALSE;
  }

  if (PR_FALSE == RectFitsInside(aRect2, aWidth, aHeight)) {
    return PR_FALSE;
  }

  aNewSize.width = aWidth;
  aNewSize.height = aHeight;

  return PR_TRUE;
}

void nsRenderingContextImpl::GetDrawingSurfaceSize(const nsRect& aMaxBackbufferSize, const nsRect& aRequestedSize, nsRect& aNewSize) 
{ 
  CalculateDiscreteSurfaceSize(aMaxBackbufferSize, aRequestedSize, aNewSize);
  aNewSize.MoveTo(aRequestedSize.x, aRequestedSize.y);
}

void nsRenderingContextImpl::CalculateDiscreteSurfaceSize(const nsRect& aMaxBackbufferSize, const nsRect& aRequestedSize, nsRect& aSurfaceSize) 
{
  // Get the height and width of the screen
  PRInt32 height;
  PRInt32 width;

  nsCOMPtr<nsIDeviceContext>  dx;
  GetDeviceContext(*getter_AddRefs(dx));
  dx->GetDeviceSurfaceDimensions(width, height);

  float devUnits;
  devUnits = dx->DevUnitsToAppUnits();
  PRInt32 screenHeight = NSToIntRound(float( height) / devUnits );
  PRInt32 screenWidth = NSToIntRound(float( width) / devUnits );

  // These tests must go from smallest rectangle to largest rectangle.

  // 1/8 screen
  if (BothRectsFitInside(aRequestedSize, aMaxBackbufferSize, screenWidth / 8, screenHeight / 8, aSurfaceSize)) {
    return;
  }

  // 1/4 screen
  if (BothRectsFitInside(aRequestedSize, aMaxBackbufferSize, screenWidth / 4, screenHeight / 4, aSurfaceSize)) {
    return;
  }

  // 1/2 screen
  if (BothRectsFitInside(aRequestedSize, aMaxBackbufferSize, screenWidth / 2, screenHeight / 2, aSurfaceSize)) {
    return;
  }

  // 3/4 screen
  if (BothRectsFitInside(aRequestedSize, aMaxBackbufferSize, (screenWidth * 3) / 4, (screenHeight * 3) / 4, aSurfaceSize)) {
    return;
  }

  // 3/4 screen width full screen height
  if (BothRectsFitInside(aRequestedSize, aMaxBackbufferSize, (screenWidth * 3) / 4, screenHeight, aSurfaceSize)) {
    return;
  }

  // Full screen
  if (BothRectsFitInside(aRequestedSize, aMaxBackbufferSize, screenWidth, screenHeight, aSurfaceSize)) {
    return;
  }

  // Bigger than Full Screen use the largest request every made.
  if (BothRectsFitInside(aRequestedSize, aMaxBackbufferSize, gLargestRequestedSize.width, gLargestRequestedSize.height, aSurfaceSize)) {
    return;
  } else {
    gLargestRequestedSize.width = PR_MAX(aRequestedSize.width, aMaxBackbufferSize.width);
    gLargestRequestedSize.height = PR_MAX(aRequestedSize.height, aMaxBackbufferSize.height);
    aSurfaceSize.width = gLargestRequestedSize.width;
    aSurfaceSize.height = gLargestRequestedSize.height;
    //   printf("Expanding the largested requested size to %d %d\n", gLargestRequestedSize.width, gLargestRequestedSize.height);
  }
}



/**
 * Let the device context know whether we want text reordered with
 * right-to-left base direction
 */
NS_IMETHODIMP
nsRenderingContextImpl::SetRightToLeftText(PRBool aIsRTL)
{
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsRenderingContextImpl.h
 *	@update 3/29/00 dwc
 */
void 
QBezierCurve::SubDivide(nsIRenderingContext *aRenderingContext)
{
QBezierCurve  curve1,curve2;
float         fx,fy,smag;

  // divide the curve into 2 pieces
	MidPointDivide(&curve1,&curve2);
	
  // for now to fix the build
	fx = (float) fabs(curve1.mAnc2.x - this->mCon.x);
	fy = (float) fabs(curve1.mAnc2.y - this->mCon.y);

	//smag = fx+fy-(PR_MIN(fx,fy)>>1);
  smag = fx*fx + fy*fy;
 
	if (smag>1){
		// split the curve again
    curve1.SubDivide(aRenderingContext);
    curve2.SubDivide(aRenderingContext);
	}else{
    // draw the curve 
#ifdef DEBUGCURVE
    printf("LINE 1- %d,%d %d,%d\n",NSToCoordRound(curve1.mAnc1.x),NSToCoordRound(curve1.mAnc1.y),
                                NSToCoordRound(curve1.mAnc1.x),NSToCoordRound(curve1.mAnc1.y));
    printf("LINE 2- %d,%d %d,%d\n",NSToCoordRound(curve1.mAnc2.x),NSToCoordRound(curve1.mAnc2.y),
                                NSToCoordRound(curve2.mAnc2.x),NSToCoordRound(curve2.mAnc2.y));
#endif
    aRenderingContext->DrawStdLine(NSToCoordRound(curve1.mAnc1.x),NSToCoordRound(curve1.mAnc1.y),NSToCoordRound(curve1.mAnc2.x),NSToCoordRound(curve1.mAnc2.y)); 
    aRenderingContext->DrawStdLine(NSToCoordRound(curve1.mAnc2.x),NSToCoordRound(curve1.mAnc2.y),NSToCoordRound(curve2.mAnc2.x),NSToCoordRound(curve2.mAnc2.y));
	}
}


/** ---------------------------------------------------
 *  See documentation in nsRenderingContextImpl.h
 *	@update 3/29/00 dwc
 */
void 
QBezierCurve::DebugPrint()
{
  printf("CURVE COORDINATES\n");
  printf("Anchor 1 %f %f\n",mAnc1.x,mAnc1.y);
  printf("Control %f %f\n",mCon.x,mCon.y);
  printf("Anchor %f %f\n",mAnc2.x,mAnc2.y);

}

/** ---------------------------------------------------
 *  See documentation in nsRenderingContextImpl.h
 *	@update 3/29/00 dwc
 */
void 
QBezierCurve::SubDivide(nsPoint  aThePoints[],PRInt16 *aNumPts)
{
QBezierCurve  curve1,curve2;
float         fx,fy,smag;

  // divide the curve into 2 pieces
	MidPointDivide(&curve1,&curve2);
	

  // for now to fix the build
	fx = (float) fabs(curve1.mAnc2.x - this->mCon.x);
	fy = (float) fabs(curve1.mAnc2.y - this->mCon.y);
	//smag = fx+fy-(PR_MIN(fx,fy)>>1);
  smag = fx*fx + fy*fy;
 
	if (smag>1){
		// split the curve again
    curve1.SubDivide(aThePoints,aNumPts);
    curve2.SubDivide(aThePoints,aNumPts);
	}else{
    // draw the curve 
    aThePoints[(*aNumPts)++].MoveTo(NSToCoordRound(curve1.mAnc1.x),NSToCoordRound(curve1.mAnc1.y));
    aThePoints[(*aNumPts)++].MoveTo(NSToCoordRound(curve1.mAnc2.x),NSToCoordRound(curve1.mAnc2.y));
    aThePoints[(*aNumPts)++].MoveTo(NSToCoordRound(curve2.mAnc2.x),NSToCoordRound(curve2.mAnc2.y));

#ifdef DEBUGCURVE
    printf("LINE 1- %d,%d %d,%d\n",NSToCoordRound(curve1.mAnc1.x),NSToCoordRound(curve1.mAnc1.y),
                                NSToCoordRound(curve1.mAnc1.x),NSToCoordRound(curve1.mAnc1.y));
    printf("LINE 2- %d,%d %d,%d\n",NSToCoordRound(curve1.mAnc2.x),NSToCoordRound(curve1.mAnc2.y),
                                NSToCoordRound(curve2.mAnc2.x),NSToCoordRound(curve2.mAnc2.y));
#endif

	}
}

/** ---------------------------------------------------
 *  See documentation in nsRenderingContextImpl.h
 *	@update 4/27/2000 dwc
 */
void 
QBezierCurve::MidPointDivide(QBezierCurve *A,QBezierCurve *B)
{
float         c1x,c1y,c2x,c2y;
nsFloatPoint	a1;

  c1x = (float) ((mAnc1.x+mCon.x)/2.0);
  c1y = (float) ((mAnc1.y+mCon.y)/2.0);
  c2x = (float) ((mAnc2.x+mCon.x)/2.0);
  c2y = (float) ((mAnc2.y+mCon.y)/2.0);

  a1.x = (float) ((c1x + c2x)/2.0);
	a1.y = (float) ((c1y + c2y)/2.0);

  // put the math into our 2 new curves
  A->mAnc1 = this->mAnc1;
  A->mCon.x = c1x;
  A->mCon.y = c1y;
  A->mAnc2 = a1;
  B->mAnc1 = a1;
  B->mCon.x = c2x;
  B->mCon.y = c2y;
  B->mAnc2 = this->mAnc2;
}

/** ---------------------------------------------------
 *  See documentation in nsRenderingContextImpl.h
 *	@update 4/27/2000 dwc
 */
nsPathIter::nsPathIter()
{

  mCurPoint = 0;
  mNumPoints = 0;
  mThePath = 0;

}

/** ---------------------------------------------------
 *  See documentation in nsRenderingContextImpl.h
 *	@update 4/27/2000 dwc
 */
nsPathIter::nsPathIter(nsPathPoint* aThePath,PRUint32 aNumPts)
{

  mCurPoint = 0;
  mNumPoints = aNumPts;
  mThePath = aThePath;

}

/** ---------------------------------------------------
 *  See documentation in nsRenderingContextImpl.h
 *	@update 4/27/2000 dwc
 */
PRBool  
nsPathIter::NextSeg(QBezierCurve& TheSegment,eSegType& aCurveType)
{
PRInt8        code=0,number=1;
PRBool        result = PR_TRUE;
nsPathPoint   *pt1,*pt2,*pt3;
float         avx,avy,av1x,av1y;

  if ( mCurPoint < mNumPoints) {
    // 1st point
    pt1 = &(mThePath[mCurPoint]);
    if(PR_TRUE == pt1->mIsOnCurve) {
      code += 0x04;
    } 
    // 2nd point
    if ( (mCurPoint+1) < mNumPoints) {
      number++;
      pt2 = &(mThePath[mCurPoint+1]);
      if(PR_TRUE == pt2->mIsOnCurve) {
        code += 0x02;
      }

      // 3rd point
      if( (mCurPoint+2) < mNumPoints) {
        number++;
        pt3 = &(mThePath[mCurPoint+2]);
        if(PR_TRUE == pt3->mIsOnCurve) {
          code += 0x01;
        }
        // have all three points..
        switch(code) {
          case 07:                        // 111
          case 06:                        // 110
            TheSegment.SetPoints(pt1->x,pt1->y,0.0,0.0,pt2->x,pt2->y);
            aCurveType = eLINE;  
            mCurPoint++;
            break;
          case 05:                        // 101
            TheSegment.SetPoints(pt1->x,pt1->y,pt2->x,pt2->y,pt3->x,pt3->y);
            aCurveType = eQCURVE;
            mCurPoint+=2;
            break;
          case 04:                        // 100
              avx = (float)((pt2->x+pt3->x)/2.0);
              avy = (float)((pt2->y+pt3->y)/2.0);
              TheSegment.SetPoints(pt1->x,pt1->y,pt2->x,pt2->y,avx,avy);
              aCurveType = eQCURVE;
              mCurPoint++;
          case 03:                        // 011
          case 02:                        // 010
              TheSegment.SetPoints(pt1->x,pt1->y,0.0,0.0,pt2->x,pt2->y);
              aCurveType = eLINE;  
              mCurPoint++;
          case 01:                        // 001
              avx = (float)((pt1->x+pt2->x)/2.0);
              avy = (float)((pt1->y+pt2->y)/2.0);
              TheSegment.SetPoints(avx,avy,pt2->x,pt3->y,pt2->x,pt3->y);
              aCurveType = eQCURVE;
              mCurPoint+=2;
          case 00:                        // 000
              avx = (float)((pt1->x+pt2->x)/2.0);
              avy = (float)((pt1->y+pt2->y)/2.0);
              av1x = (float)((pt2->x+pt3->x)/2.0);
              av1y = (float)((pt2->y+pt3->y)/2.0);
              TheSegment.SetPoints(avx,avy,pt2->x,pt2->y,av1x,av1y);
          default:
            break;
        }
      } else {
        // have two points.. draw a line
        TheSegment.SetPoints(pt1->x,pt1->y,0.0,0.0,pt2->x,pt2->y);
        aCurveType = eLINE;  
        mCurPoint++;
      }
    } else {
      // just have one point
      result = PR_FALSE;
    }
  } else {
    // all finished
    result = PR_FALSE;
  }

  return result;
}


#include "imgIContainer.h"
#include "gfxIImageFrame.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"

NS_IMETHODIMP nsRenderingContextImpl::DrawImage(imgIContainer *aImage, const nsRect & aSrcRect, const nsRect & aDestRect)
{
  nsRect dr = aDestRect;
  mTranMatrix->TransformCoord(&dr.x, &dr.y, &dr.width, &dr.height);

  nsRect sr = aSrcRect;
  mTranMatrix->TransformCoord(&sr.x, &sr.y, &sr.width, &sr.height);
  
  if (sr.IsEmpty() || dr.IsEmpty())
    return NS_OK;

  sr.x = aSrcRect.x;
  sr.y = aSrcRect.y;
  mTranMatrix->TransformNoXLateCoord(&sr.x, &sr.y);

  nsCOMPtr<gfxIImageFrame> iframe;
  aImage->GetCurrentFrame(getter_AddRefs(iframe));
  if (!iframe) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIImage> img(do_GetInterface(iframe));
  if (!img) return NS_ERROR_FAILURE;

  nsIDrawingSurface *surface = nsnull;
  GetDrawingSurface((void**)&surface);
  if (!surface) return NS_ERROR_FAILURE;

  // For Bug 87819
  // iframe may want image to start at different position, so adjust
  nsRect iframeRect;
  iframe->GetRect(iframeRect);
  
  if (iframeRect.x > 0) {
    nscoord scaled_x = sr.x;
    if (dr.width != sr.width) {
      PRFloat64 scale_ratio = PRFloat64(dr.width) / PRFloat64(sr.width);
      scaled_x = NSToCoordRound(scaled_x * scale_ratio);
    }
    sr.x -= iframeRect.x;
    if (sr.x < 0) {
      dr.x -= scaled_x;
      sr.width += sr.x;
      dr.width += scaled_x;
      if (sr.width <= 0 || dr.width <= 0)
        return NS_OK;
      sr.x = 0;
    } else if (sr.x > iframeRect.width) {
      return NS_OK;
    }
  }

  if (iframeRect.y > 0) {
    nscoord scaled_y = sr.y;
    if (dr.height != sr.height) {
      PRFloat64 scale_ratio = PRFloat64(dr.height) / PRFloat64(sr.height);
      scaled_y = NSToCoordRound(scaled_y * scale_ratio);
    }

    // adjust for offset  
    sr.y -= iframeRect.y;
    if (sr.y < 0) {
      dr.y -= scaled_y;
      sr.height += sr.y;
      dr.height += scaled_y;
      if (sr.height <= 0 || dr.height <= 0)
        return NS_OK;
      sr.y = 0;
    } else if (sr.y > iframeRect.height) {
      return NS_OK;
    }
  }

  return img->Draw(*this, surface, sr.x, sr.y, sr.width, sr.height,
                   dr.x, dr.y, dr.width, dr.height);
}

/* [noscript] void drawTile (in imgIContainer aImage, in nscoord aXImageStart, in nscoord aYImageStart, [const] in nsRect aTargetRect); */
NS_IMETHODIMP
nsRenderingContextImpl::DrawTile(imgIContainer *aImage,
                                 nscoord aXImageStart, nscoord aYImageStart,
                                 const nsRect * aTargetRect)
{
  nsRect dr(*aTargetRect);
  mTranMatrix->TransformCoord(&dr.x, &dr.y, &dr.width, &dr.height);
  mTranMatrix->TransformCoord(&aXImageStart, &aYImageStart);

  nscoord width, height;
  aImage->GetWidth(&width);
  aImage->GetHeight(&height);

  if (width == 0 || height == 0)
    return PR_FALSE;

  nscoord xOffset = (dr.x - aXImageStart) % width;
  nscoord yOffset = (dr.y - aYImageStart) % height;

  nsCOMPtr<gfxIImageFrame> iframe;
  aImage->GetCurrentFrame(getter_AddRefs(iframe));
  if (!iframe) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIImage> img(do_GetInterface(iframe));
  if (!img) return NS_ERROR_FAILURE;

  nsIDrawingSurface *surface = nsnull;
  GetDrawingSurface((void**)&surface);
  if (!surface) return NS_ERROR_FAILURE;

  /* bug 113561 - frame can be smaller than container */
  nsRect iframeRect;
  iframe->GetRect(iframeRect);
  PRInt32 padx = width - iframeRect.width;
  PRInt32 pady = height - iframeRect.height;

  return img->DrawTile(*this, surface,
                       xOffset - iframeRect.x, yOffset - iframeRect.y,
                       padx, pady,
                       dr);
}

NS_IMETHODIMP
nsRenderingContextImpl::FlushRect(const nsRect& aRect)
{
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextImpl::FlushRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
    return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextImpl::RenderPostScriptDataFragment(const unsigned char *aData, unsigned long aDatalen)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


