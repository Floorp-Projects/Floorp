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

 if( !hasMask && ((aWidth<(tvrect.width/16)) || (aHeight<(tvrect.height/16)))) {

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
