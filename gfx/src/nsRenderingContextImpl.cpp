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

#include "nsRenderingContextImpl.h"
#include "nsIDeviceContext.h"
#include "nsIImage.h"
#include "nsTransform2D.h"


/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 3/16/00 dwc
 */
nsRenderingContextImpl :: nsRenderingContextImpl()
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
      this->DrawImage(aImage,0,0,aWidth,aHeight);

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
        this->DrawImage(aImage,x,y,aWidth,aHeight);
      }
    }
  }

  return NS_OK;
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
nsPathIter            *thePathIter;
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

  thePathIter = new nsPathIter(pp0,aNumPts);
	while ( thePathIter->NextSeg(thecurve,curveType) ) {
    // draw the curve we found
    if(eLINE == curveType){
      DrawStdLine(NSToCoordRound(thecurve.mAnc1.x),NSToCoordRound(thecurve.mAnc1.y),NSToCoordRound(thecurve.mAnc2.x),NSToCoordRound(thecurve.mAnc2.y));
    } else {
      thecurve.SubDivide(this);
    }
	}

  // Release temporary storage if necessary
  if (pp0 != pts)
    delete pp0;

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
nsPathIter            *thePathIter;
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

  thePathIter = new nsPathIter(pp0,aNumPts);
	while ( thePathIter->NextSeg(thecurve,curveType) ) {
    // build a polygon with the points
    if(eLINE == curveType){
      thePath[curPoint++].MoveTo(NSToCoordRound(thecurve.mAnc1.x),NSToCoordRound(thecurve.mAnc1.y));
      thePath[curPoint++].MoveTo(NSToCoordRound(thecurve.mAnc2.x),NSToCoordRound(thecurve.mAnc2.y));
    } else {
      thecurve.SubDivide(thePath,&curPoint);
    }
	}

  this->FillStdPolygon(thePath,curPoint);

  // Release temporary storage if necessary
  if (pp0 != pts)
    delete pp0;

  return NS_OK;
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
    aRenderingContext->DrawStdLine(NSToCoordRound(curve1.mAnc1.x),NSToCoordRound(curve1.mAnc1.y),NSToCoordRound(curve1.mAnc2.x),NSToCoordRound(curve1.mAnc2.y)); 
    aRenderingContext->DrawStdLine(NSToCoordRound(curve1.mAnc2.x),NSToCoordRound(curve1.mAnc2.y),NSToCoordRound(curve2.mAnc2.x),NSToCoordRound(curve2.mAnc2.y));
	}
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
	//fx = (float) fabs(curve1.mAnc2.x - this->mCon.x);
	//fy = (float) fabs(curve1.mAnc2.y - this->mCon.y);
  fx = fy = 0;

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
            TheSegment.SetPoints(pt1->x,pt1->y,0,0,pt2->x,pt2->y);
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
              TheSegment.SetPoints((float)pt1->x,(float)pt1->y,(float)pt2->x,(float)pt2->y,avx,avy);
              aCurveType = eQCURVE;
              mCurPoint++;
          case 03:                        // 011
          case 02:                        // 010
              TheSegment.SetPoints(pt1->x,pt1->y,0,0,pt2->x,pt2->y);
              aCurveType = eLINE;  
              mCurPoint++;
          case 01:                        // 001
              avx = (float)((pt1->x+pt2->x)/2.0);
              avy = (float)((pt1->y+pt2->y)/2.0);
              TheSegment.SetPoints(avx,avy,(float)pt2->x,(float)pt3->y,(float)pt2->x,(float)pt3->y);
              aCurveType = eQCURVE;
              mCurPoint+=2;
          case 00:                        // 000
              avx = (float)((pt1->x+pt2->x)/2.0);
              avy = (float)((pt1->y+pt2->y)/2.0);
              av1x = (float)((pt2->x+pt3->x)/2.0);
              av1y = (float)((pt2->y+pt3->y)/2.0);
              TheSegment.SetPoints(avx,avy,(float)pt2->x,(float)pt2->y,av1x,av1y);
          default:
            break;
        }
      } else {
        // have two points.. draw a line
        TheSegment.SetPoints(pt1->x,pt1->y,0,0,pt2->x,pt2->y);
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
