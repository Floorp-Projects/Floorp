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


#include "nsDrawingSurfaceMac.h"


static NS_DEFINE_IID(kIDrawingSurfaceIID, NS_IDRAWING_SURFACE_IID);
static NS_DEFINE_IID(kIDrawingSurfaceMacIID, NS_IDRAWING_SURFACE_MAC_IID);


/** --------------------------------------------------- 
 * See Documentation in nsIDrawingSurface.h
 * @update 3/02/99 dwc
 * @return error status
 */
nsDrawingSurfaceMac :: nsDrawingSurfaceMac()
{
  NS_INIT_REFCNT();

  mPort = NULL;
	mGS = new GraphicState();
  mWidth = mHeight = 0;
  mLockOffset = mLockHeight = 0;
  mLockFlags = 0;
	mIsOffscreen = PR_FALSE;

}

/** --------------------------------------------------- 
 * See Documentation in nsIDrawingSurface.h
 * @update 3/02/99 dwc
 * @return error status
 */
nsDrawingSurfaceMac :: ~nsDrawingSurfaceMac()
{
GWorldPtr offscreenGWorld;

	if(mIsOffscreen && mPort){
  	offscreenGWorld = (GWorldPtr)mPort;
		::UnlockPixels(::GetGWorldPixMap(offscreenGWorld));
		::DisposeGWorld(offscreenGWorld);
	}

	if (mGS){
		delete mGS;
	}
		
}

/** --------------------------------------------------- 
 * See Documentation in nsIDrawingSurface.h
 * @update 3/02/99 dwc
 * @return error status
 */
NS_IMETHODIMP nsDrawingSurfaceMac :: QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr)
    return NS_ERROR_NULL_POINTER;

  if (aIID.Equals(kIDrawingSurfaceIID)){
    nsIDrawingSurface* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }

  if (aIID.Equals(kIDrawingSurfaceMacIID)){
    nsIDrawingSurfaceMac* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }

  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

  if (aIID.Equals(kISupportsIID)){
    nsIDrawingSurface* tmp = this;
    nsISupports* tmp2 = tmp;
    *aInstancePtr = (void*) tmp2;
    NS_ADDREF_THIS();
    return NS_OK;
  }

  return NS_NOINTERFACE;
}

NS_IMPL_ADDREF(nsDrawingSurfaceMac)
NS_IMPL_RELEASE(nsDrawingSurfaceMac)

/** --------------------------------------------------- 
 * See Documentation in nsIDrawingSurface.h
 * @update 3/02/99 dwc
 * @return error status
 */
NS_IMETHODIMP nsDrawingSurfaceMac :: Lock(PRInt32 aX, PRInt32 aY,
                                          PRUint32 aWidth, PRUint32 aHeight,
                                          void **aBits, PRInt32 *aStride,
                                          PRInt32 *aWidthBytes, PRUint32 aFlags)
{

  return NS_OK;
}

/** --------------------------------------------------- 
 * See Documentation in nsIDrawingSurface.h
 * @update 3/02/99 dwc
 * @return error status
 */
NS_IMETHODIMP nsDrawingSurfaceMac :: Unlock(void)
{

  return NS_OK;
}

/** --------------------------------------------------- 
 * See Documentation in nsIDrawingSurface.h
 * @update 3/02/99 dwc
 * @return error status
 */
NS_IMETHODIMP nsDrawingSurfaceMac :: GetDimensions(PRUint32 *aWidth, PRUint32 *aHeight)
{
  *aWidth = mWidth;
  *aHeight = mHeight;

  return NS_OK;
}

/** --------------------------------------------------- 
 * See Documentation in nsIDrawingSurface.h
 * @update 3/02/99 dwc
 * @return error status
 */
NS_IMETHODIMP nsDrawingSurfaceMac :: IsPixelAddressable(PRBool *aAddressable)
{
  return NS_OK;
}

/** --------------------------------------------------- 
 * See Documentation in nsIDrawingSurface.h
 * @update 3/02/99 dwc
 * @return error status
 */
NS_IMETHODIMP nsDrawingSurfaceMac :: GetPixelFormat(nsPixelFormat *aFormat)
{
  //*aFormat = mPixFormat;

  return NS_OK;
}

/** --------------------------------------------------- 
 * See Documentation in nsIDrawingSurfaceMac.h
 * @update 3/02/99 dwc
 * @return error status
 */
NS_IMETHODIMP nsDrawingSurfaceMac :: Init(nsDrawingSurface	aDS)
{
GrafPtr	gport;

	nsDrawingSurfaceMac* surface = static_cast<nsDrawingSurfaceMac*>(aDS);
	surface->GetGrafPtr(&gport);
	mPort = gport;
	mGS->Init(surface);
	
  return NS_OK;
}

/** --------------------------------------------------- 
 * See Documentation in nsIDrawingSurfaceMac.h
 * @update 3/02/99 dwc
 * @return error status
 */
NS_IMETHODIMP nsDrawingSurfaceMac :: Init(GrafPtr aPort)
{

	// set our grafPtr to the passed in port
  mPort = aPort;
	mGS->Init(aPort);
  return NS_OK;
}

/** --------------------------------------------------- 
 * See Documentation in nsIDrawingSurfaceMac.h
 * @update 3/02/99 dwc
 * @return error status
 */
NS_IMETHODIMP nsDrawingSurfaceMac :: Init(nsIWidget *aTheWidget)
{
	// get our native graphics port from the widget
 	mPort = static_cast<GrafPtr>(aTheWidget->GetNativeData(NS_NATIVE_DISPLAY));
	mGS->Init(aTheWidget);
  return NS_OK;
}

/** --------------------------------------------------- 
 * See Documentation in nsIDrawingSurfaceMac.h
 * @update 3/02/99 dwc
 * @return error status
 */
NS_IMETHODIMP nsDrawingSurfaceMac :: Init(PRUint32 aDepth,PRUint32 aWidth,PRUint32 aHeight, PRUint32 aFlags)
{
PRUint32	depth;
Rect			macRect;
GWorldPtr offscreenGWorld;
GrafPtr 	savePort;

  depth = aDepth;

	// calculate the rectangle
  if (aWidth != 0){
  	::SetRect(&macRect, 0, 0, aWidth, aHeight);
  }else{
  	::SetRect(&macRect, 0, 0, 2, 2);
	}

	// create offscreen
  QDErr osErr = ::NewGWorld(&offscreenGWorld, depth, &macRect, nil, nil, 0);
  if (osErr != noErr)
  	return NS_ERROR_FAILURE;

	// keep the pixels locked... that's how it works on Windows and  we are forced to do
	// the same because the API doesn't give us any hook to do it at drawing time.
  ::LockPixels(::GetGWorldPixMap(offscreenGWorld));

	// erase the offscreen area
	
	::GetPort(&savePort);
	::SetPort((GrafPtr)offscreenGWorld);
	::EraseRect(&macRect);
	::SetPort(savePort);

	this->Init((GrafPtr)offscreenGWorld);
	mIsOffscreen = PR_TRUE;
  return NS_OK;
}

