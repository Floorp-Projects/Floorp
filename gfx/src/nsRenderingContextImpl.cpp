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

#include "nsRenderingContextImpl.h"
#include "nsIDeviceContext.h"
#include "nsIImage.h"
#include "nsTransform2D.h"
#include <stdlib.h>


const nsPoint *gPts;

// comparison routines for qsort 
PRInt32 PR_CALLBACK compare_ind(const void *u,const void *v){return gPts[(PRInt32)*((PRInt32*)u)].y <= gPts[(PRInt32)*((PRInt32*)v)].y ? -1 : 1;}
PRInt32 PR_CALLBACK compare_active(const void *u,const void *v){return ((Edge*)u)->x <= ((Edge*)v)->x ? -1 : 1;}


/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 3/16/00 dwc
 */
nsRenderingContextImpl :: nsRenderingContextImpl()
{

  mPenMode = nsPenMode_kNone;

}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 3/16/00 dwc
 */
nsRenderingContextImpl :: ~nsRenderingContextImpl()
{


}


#if 0

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 3/29/00 dwc
 */
NS_IMETHODIMP 
nsRenderingContextImpl::DrawTile(nsIImage *aImage,nscoord aX0,nscoord aY0,nscoord aX1,nscoord aY1,
                                                    nscoord aWidth,nscoord aHeight)
{
PRBool              hasMask,clip;
nscoord             x,y;
nsRect              srcRect,destRect,vrect,tvrect;
PRInt32             flag = NS_COPYBITS_TO_BACK_BUFFER | NS_COPYBITS_XFORM_DEST_VALUES;
PRUint32            dsFlag = 0;
nsIDrawingSurface   *theSurface,*ts=nsnull;
float               t2p,app2dev;
nsIDeviceContext    *theDevCon;
nsTransform2D       *theTransform;

  // we have to do things ourselves
  hasMask = aImage->GetHasAlphaMask();

  tvrect.SetRect(0,0,aX1-aX0,aY1-aY0);

  if(!hasMask && ((aWidth<(tvrect.width/16)) || (aHeight<(tvrect.height/16)))) {

    // create a larger tile to use
    GetDeviceContext(theDevCon);
    theDevCon->GetTwipsToDevUnits(t2p);
    this->GetDrawingSurface((void**)&theSurface);

    tvrect.width = ((tvrect.width)/aWidth); 
    tvrect.width *=aWidth;

    tvrect.height = ((tvrect.height)/aHeight);
    tvrect.height *=aHeight;

    // create a new drawing surface... using pixels as the size
    vrect.height = (nscoord)(tvrect.height * t2p);
    vrect.width = (nscoord)(tvrect.width * t2p);
    this->CreateDrawingSurface(&vrect,dsFlag,(nsDrawingSurface&)ts);

    if (nsnull != ts) {
      this->SelectOffScreenDrawingSurface(ts);

      // create a bigger tile in our new drawingsurface                    
      // XXX pushing state to fix clipping problem, need to look into why the clip is set here
      this->PushState();
      this->GetCurrentTransform(theTransform);
      theDevCon->GetAppUnitsToDevUnits(app2dev);
      theTransform->SetToIdentity();  
	    theTransform->AddScale(app2dev, app2dev);

#ifdef XP_UNIX
      srcRect.SetRect(0,0,tvrect.width,tvrect.height);
      SetClipRect(srcRect, nsClipCombine_kReplace, clip);
#endif

      // copy the initial image to our buffer, this takes twips and converts to pixels.. 
      // which is what the image is in
      NS_STATIC_CAST(nsIRenderingContext*, this)->DrawImage(aImage,0,0,aWidth,aHeight);

      // duplicate the image in the upperleft corner to fill up the nsDrawingSurface
      srcRect.SetRect(0,0,aWidth,aHeight);
      TileImage(ts,srcRect,tvrect.width,tvrect.height);

      // setting back the clip from the background clip push
      this->PopState(clip);
  
      // set back to the old drawingsurface
      this->SelectOffScreenDrawingSurface((void**)theSurface);

     // now duplicate our tile into the background
      destRect = srcRect;
      for(y=aY0;y<aY1;y+=tvrect.height){
        for(x=aX0;x<aX1;x+=tvrect.width){
          destRect.x = x;
          destRect.y = y;
          this->CopyOffScreenBits(ts,0,0,destRect,flag);
        }
      } 
      this->DestroyDrawingSurface(ts);
    }
  NS_RELEASE(theDevCon);
  } else {
    // slow blitting, one tile at a time.... ( will create a mask and fall into code below -next task-)
    for(y=aY0;y<aY1;y+=aHeight){
      for(x=aX0;x<aX1;x+=aWidth){
        NS_STATIC_CAST(nsIRenderingContext*, this)->DrawImage(aImage,x,y,aWidth,aHeight);
      }
    }
  }

  return NS_OK;
}


NS_IMETHODIMP 
nsRenderingContextImpl::DrawTile(nsIImage *aImage, nscoord aSrcXOffset,
                                 nscoord aSrcYOffset,
                                 const nsRect &aDirtyRect)
{
  return NS_OK;
}


#endif


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



/** ---------------------------------------------------
 *  See documentation in nsRenderingContextImpl.h
 *	@update 3/29/00 dwc
 */
void
nsRenderingContextImpl::TileImage(nsDrawingSurface  aDS,nsRect &aSrcRect,PRInt16 aWidth,PRInt16 aHeight)
{
nsRect  destRect;
PRInt32 flag = NS_COPYBITS_TO_BACK_BUFFER | NS_COPYBITS_XFORM_DEST_VALUES;
  
  if( aSrcRect.width < aWidth) {
    // width is less than double so double our source bitmap width
    destRect = aSrcRect;
    destRect.x += aSrcRect.width;
    this->CopyOffScreenBits(aDS,aSrcRect.x,aSrcRect.y,destRect,flag);
    aSrcRect.width*=2; 
    TileImage(aDS,aSrcRect,aWidth,aHeight);
  } else if (aSrcRect.height < aHeight) {
    // height is less than double so double our source bitmap height
    destRect = aSrcRect;
    destRect.y += aSrcRect.height;
    this->CopyOffScreenBits(aDS,aSrcRect.x,aSrcRect.y,destRect,flag);
    aSrcRect.height*=2;
    TileImage(aDS,aSrcRect,aWidth,aHeight);
  } 
}

#ifdef IBMBIDI
/**
 * Let the device context know whether we want text reordered with
 * right-to-left base direction
 */
NS_IMETHODIMP
nsRenderingContextImpl::SetRightToLeftText(PRBool aIsRTL)
{
  return NS_OK;
}
#endif // IBMBIDI

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


#ifdef USE_IMG2

#include "imgIContainer.h"
#include "gfxIImageFrame.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"

/* [noscript] void drawImage (in imgIContainer aImage, [const] in nsRect aSrcRect, [const] in nsPoint aDestPoint); */
NS_IMETHODIMP nsRenderingContextImpl::DrawImage(imgIContainer *aImage, const nsRect * aSrcRect, const nsPoint * aDestPoint)
{
  nsPoint pt;
  nsRect sr;

  pt = *aDestPoint;
  mTranMatrix->TransformCoord(&pt.x, &pt.y);

  sr = *aSrcRect;
  mTranMatrix->TransformCoord(&sr.x, &sr.y, &sr.width, &sr.height);

  sr.x = aSrcRect->x;
  sr.y = aSrcRect->y;
  mTranMatrix->TransformNoXLateCoord(&sr.x, &sr.y);

  nsCOMPtr<gfxIImageFrame> iframe;
  aImage->GetCurrentFrame(getter_AddRefs(iframe));
  if (!iframe) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIImage> img(do_GetInterface(iframe));
  if (!img) return NS_ERROR_FAILURE;

  nsIDrawingSurface *surface = nsnull;
  GetDrawingSurface((void**)&surface);
  if (!surface) return NS_ERROR_FAILURE;

  return img->Draw(*this, surface, sr.x, sr.y, sr.width, sr.height,
                   pt.x + sr.x, pt.y + sr.y, sr.width, sr.height);
}

/* [noscript] void drawScaledImage (in imgIContainer aImage, [const] in nsRect aSrcRect, [const] in nsRect aDestRect); */
NS_IMETHODIMP nsRenderingContextImpl::DrawScaledImage(imgIContainer *aImage, const nsRect * aSrcRect, const nsRect * aDestRect)
{
  nsRect dr;
  nsRect sr;

  dr = *aDestRect;
  mTranMatrix->TransformCoord(&dr.x, &dr.y, &dr.width, &dr.height);

  sr = *aSrcRect;
  mTranMatrix->TransformCoord(&sr.x, &sr.y, &sr.width, &sr.height);

  sr.x = aSrcRect->x;
  sr.y = aSrcRect->y;
  mTranMatrix->TransformNoXLateCoord(&sr.x, &sr.y);

  nsCOMPtr<gfxIImageFrame> iframe;
  aImage->GetCurrentFrame(getter_AddRefs(iframe));
  if (!iframe) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIImage> img(do_GetInterface(iframe));
  if (!img) return NS_ERROR_FAILURE;

  nsIDrawingSurface *surface = nsnull;
  GetDrawingSurface((void**)&surface);
  if (!surface) return NS_ERROR_FAILURE;

  return img->Draw(*this, surface, sr.x, sr.y, sr.width, sr.height, dr.x, dr.y, dr.width, dr.height);
}

/* [noscript] void drawTile (in imgIContainer aImage, in nscoord aXOffset, in nscoord aYOffset, [const] in nsRect aTargetRect); */
NS_IMETHODIMP nsRenderingContextImpl::DrawTile(imgIContainer *aImage, nscoord aXOffset, nscoord aYOffset, const nsRect * aTargetRect)
{
  nsRect dr(*aTargetRect);
  nsRect so(0, 0, aXOffset, aYOffset);

  mTranMatrix->TransformCoord(&dr.x, &dr.y, &dr.width, &dr.height);

  // i want one of these...
  mTranMatrix->TransformCoord(&so.x, &so.y, &so.width, &so.height);

  nsCOMPtr<gfxIImageFrame> iframe;
  aImage->GetCurrentFrame(getter_AddRefs(iframe));
  if (!iframe) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIImage> img(do_GetInterface(iframe));
  if (!img) return NS_ERROR_FAILURE;

  nsIDrawingSurface *surface = nsnull;
  GetDrawingSurface((void**)&surface);
  if (!surface) return NS_ERROR_FAILURE;

  return img->DrawTile(*this, surface, so.width, so.height, dr);
}

/* [noscript] void drawScaledTile (in imgIContainer aImage, in nscoord aXOffset, in nscoord aYOffset, in nscoord aTileWidth, in nscoord aTileHeight, [const] in nsRect aTargetRect); */
NS_IMETHODIMP nsRenderingContextImpl::DrawScaledTile(imgIContainer *aImage, nscoord aXOffset, nscoord aYOffset, nscoord aTileWidth, nscoord aTileHeight, const nsRect * aTargetRect)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

#endif
