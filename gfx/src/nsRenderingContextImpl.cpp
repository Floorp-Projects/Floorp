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
nsPathPoint           *pp0,*np,*pp;
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

	for ( i= 0; i < aNumPts; i++,np++,pp++){
		pp->x = np->x;
		pp->y = np->y;
		mTranMatrix->TransformCoord((int*)&pp->x,(int*)&pp->y);
	}


  thePathIter = new nsPathIter(pp0,aNumPts);
	while ( thePathIter->NextSeg(thecurve,curveType) ) {
		//thecurve.Display_Graphic(TheProcess,NULL);
	}

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
QBezierCurve::SubDivide(nsIRenderingContext *aRenderingContext,nsPoint aPointArray[],PRInt32 *aCurIndex)
{
QBezierCurve    curve1,curve2;
PRInt16         fx,fy,smag;

  // divide the curve into 2 pieces
	MidPointDivide(&curve1,&curve2);
	
	fx = (PRInt16)abs(curve1.mAnc2.x - this->mCon.x);
	fy = (PRInt16)abs(curve1.mAnc2.y - this->mCon.y);

	smag = fx+fy-(PR_MIN(fx,fy)>>1);
  //smag = fx*fx + fy*fy;
 
	if (smag>1){
		// split the curve again
    curve1.SubDivide(aRenderingContext,aPointArray,aCurIndex);
    curve2.SubDivide(aRenderingContext,aPointArray,aCurIndex);
	}else{
    if(aPointArray ) {
      // save the points for further processing
      aPointArray[*aCurIndex].x = curve1.mAnc2.x;
      aPointArray[*aCurIndex].y = curve1.mAnc2.y;
      (*aCurIndex)++;
      aPointArray[*aCurIndex].x = curve2.mAnc2.x;
      aPointArray[*aCurIndex].y = curve2.mAnc2.y;
      (*aCurIndex)++;
    }else{
		  // draw the curve 
      aRenderingContext->DrawLine(curve1.mAnc1.x,curve1.mAnc1.y,curve1.mAnc2.x,curve1.mAnc2.y);
      aRenderingContext->DrawLine(curve1.mAnc2.x,curve1.mAnc2.y,curve2.mAnc2.x,curve2.mAnc2.y);
    }
	}
}

/** ---------------------------------------------------
 *  See documentation in nsRenderingContextImpl.h
 *	@update 4/27/2000 dwc
 */
void 
QBezierCurve::MidPointDivide(QBezierCurve *A,QBezierCurve *B)
{
double  c1x,c1y,c2x,c2y;
nsPoint	a1;

  c1x = (mAnc1.x+mCon.x)/2.0;
  c1y = (mAnc1.y+mCon.y)/2.0;
  c2x = (mAnc2.x+mCon.x)/2.0;
  c2y = (mAnc2.y+mCon.y)/2.0;

  a1.x = (PRInt32)((c1x + c2x)/2.0);
	a1.y = (PRInt32)((c1y + c2y)/2.0);

  // put the math into our 2 new curves
  A->mAnc1 = this->mAnc1;
  A->mCon.x = (PRInt16)c1x;
  A->mCon.y = (PRInt16)c1y;
  A->mAnc2 = a1;
  B->mAnc1 = a1;
  B->mCon.x = (PRInt16)c2x;
  B->mCon.y = (PRInt16)c2y;
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
PRInt8        code=0;
PRBool        result = PR_FALSE;
nsPathPoint   *pt1,*pt2,*pt3;
nsPathPoint   ptAvg,ptAvg1;


  if ( mCurPoint < mNumPoints) {
    // 1st point
    pt1 = &(mThePath[mCurPoint]);
    if(PR_TRUE == pt1->mIsOnCurve) {
      code += 0x04;
    }
    
    // 2nd point
    if ( (mCurPoint+1) < mNumPoints) {
      pt2 = &(mThePath[mCurPoint+1]);
    } else{
      pt2 = &(mThePath[0]);
    }
    if(PR_TRUE == pt2->mIsOnCurve) {
      code += 0x02;
    }

    // 3rd point
    if( (mCurPoint+2) < mNumPoints) {
      pt3 = &(mThePath[mCurPoint+2]);
    } else if ( (mCurPoint+1) < mNumPoints) {
      pt3 = &(mThePath[0]);
    } else {
      pt3 = &(mThePath[1]);
    }
    if(PR_TRUE == pt3->mIsOnCurve) {
        code += 0x01;
    }
  
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
          ptAvg.x = (nscoord) (((pt2->x+pt3->x)/2.0));
          ptAvg.y = (nscoord) (((pt2->y+pt3->y)/2.0));
          TheSegment.SetPoints(pt1->x,pt1->y,pt2->x,pt2->y,ptAvg.x,ptAvg.y);
          aCurveType = eQCURVE;
          mCurPoint++;
      case 03:                        // 011
      case 02:                        // 010
          TheSegment.SetPoints(pt1->x,pt1->y,0,0,pt2->x,pt2->y);
          aCurveType = eLINE;  
          mCurPoint++;
      case 01:                        // 001
          ptAvg.x = (nscoord) (((pt1->x+pt2->x)/2.0));
          ptAvg.y = (nscoord) (((pt1->y+pt2->y)/2.0));
          TheSegment.SetPoints(ptAvg.x,ptAvg.y,pt2->x,pt3->y,pt2->x,pt3->y);
          aCurveType = eQCURVE;
          mCurPoint+=2;
      case 00:                        // 000
          ptAvg.x = (nscoord) (((pt1->x+pt2->x)/2.0));
          ptAvg.y = (nscoord) (((pt1->y+pt2->y)/2.0));
          ptAvg1.x = (nscoord) (((pt2->x+pt3->x)/2.0));
          ptAvg1.y = (nscoord) (((pt2->y+pt3->y)/2.0));
          TheSegment.SetPoints(ptAvg.x,ptAvg.y,pt2->x,pt2->y,ptAvg1.x,ptAvg1.y);
      default:
        break;
    }
  }

  return result;
}
