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

#include <MacMemory.h>

#include "nsDrawingSurfaceMac.h"
#include "nsGraphicState.h"


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
	mGS = sGraphicStatePool.GetNewGS();	//new nsGraphicState();
  mWidth = mHeight = 0;
  mLockOffset = mLockHeight = 0;
  mLockFlags = 0;
	mIsOffscreen = PR_FALSE;
	mIsLocked = PR_FALSE;
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
		sGraphicStatePool.ReleaseGS(mGS); //delete mGS;
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

NS_IMPL_ADDREF(nsDrawingSurfaceMac);
NS_IMPL_RELEASE(nsDrawingSurfaceMac);

#pragma mark-

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
  char*					baseaddr;
  PRInt32				cmpSize, rowBytes;
  GWorldPtr 		offscreenGWorld;
  PixMapHandle	thePixMap;


  if ((mIsLocked == PR_FALSE) && mIsOffscreen && mPort) {
    // get the offscreen gworld for our use
    offscreenGWorld = (GWorldPtr)mPort;

    // calculate the pixel data size
    thePixMap = ::GetGWorldPixMap(offscreenGWorld);
    baseaddr = GetPixBaseAddr(thePixMap);
    cmpSize = ((**thePixMap).pixelSize >> 3);
    rowBytes = (**thePixMap).rowBytes & 0x3FFF;
    *aBits = baseaddr + (aX * cmpSize) + aY * rowBytes;
    *aStride = rowBytes;
    *aWidthBytes = aWidth * cmpSize;
    mIsLocked = PR_TRUE;
  }

  return NS_OK;
}

/** --------------------------------------------------- 
 * See Documentation in nsIDrawingSurface.h
 * @update 3/02/99 dwc
 * @return error status
 */
NS_IMETHODIMP nsDrawingSurfaceMac :: Unlock(void)
{
	mIsLocked = PR_FALSE;
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
  *aFormat = mPixFormat;
  return NS_OK;
}

#pragma mark -

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
	
	return SetupPixelFormat((reinterpret_cast<CGrafPtr>(mPort))->portPixMap);
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

	return SetupPixelFormat((reinterpret_cast<CGrafPtr>(mPort))->portPixMap);
}

/** --------------------------------------------------- 
 * See Documentation in nsIDrawingSurfaceMac.h
 * @update 3/02/99 dwc
 * @return error status
 */
NS_IMETHODIMP nsDrawingSurfaceMac :: Init(nsIWidget *aTheWidget)
{
	// get our native graphics port from the widget
 	mPort = reinterpret_cast<GrafPtr>(aTheWidget->GetNativeData(NS_NATIVE_GRAPHIC));
	mGS->Init(aTheWidget);

	return SetupPixelFormat((reinterpret_cast<CGrafPtr>(mPort))->portPixMap);
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
  GWorldPtr offscreenGWorld = nsnull;
  GrafPtr 	savePort;
  Boolean   tryTempMemFirst = ((aFlags & NS_CREATEDRAWINGSURFACE_SHORTLIVED) != 0);
  
  depth = aDepth;
  mWidth = aWidth;
  mHeight = aHeight;
  

	// calculate the rectangle
  if (aWidth != 0){
  	::SetRect(&macRect, 0, 0, aWidth, aHeight);
  }else{
  	::SetRect(&macRect, 0, 0, 2, 2);
	}

	// create offscreen, first with normal memory, if that fails use temp memory, if that fails, return

	// Quick and dirty check to make sure there is some memory available.
	// GWorld allocations in temp mem can still fail if the heap is totally
	// full, because some stuff is allocated in the heap
	const long kReserveHeapFreeSpace = (1024 * 1024);
	const long kReserveHeapContigSpace	= (512 * 1024);

  QDErr		err = noErr;
  long	  totalSpace, contiguousSpace;
  
  if (tryTempMemFirst)
  {
	  ::NewGWorld(&offscreenGWorld, depth, &macRect, nsnull, nsnull, useTempMem);
    if (!offscreenGWorld)
    {
      // only try the heap if there is enough space
    	::PurgeSpace(&totalSpace, &contiguousSpace);		// this does not purge memory, just measure it

    	if (totalSpace > kReserveHeapFreeSpace && contiguousSpace > kReserveHeapContigSpace)
     		::NewGWorld(&offscreenGWorld, depth, &macRect, nsnull, nsnull, 0);
    }
  }
  else    // heap first
  {
      // only try the heap if there is enough space
    	::PurgeSpace(&totalSpace, &contiguousSpace);		// this does not purge memory, just measure it

    	if (totalSpace > kReserveHeapFreeSpace && contiguousSpace > kReserveHeapContigSpace)
     		::NewGWorld(&offscreenGWorld, depth, &macRect, nsnull, nsnull, 0);
  
      if (!offscreenGWorld)
	      ::NewGWorld(&offscreenGWorld, depth, &macRect, nsnull, nsnull, useTempMem);
  }
  
  if (!offscreenGWorld)
    return NS_ERROR_OUT_OF_MEMORY;  
  
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
	
	// setup the pixel format
	return SetupPixelFormat(::GetGWorldPixMap(offscreenGWorld));
}

#pragma mark -

nsresult nsDrawingSurfaceMac::SetupPixelFormat(PixMapHandle pixmap)
{
	long masks[3];
	
	switch ((*pixmap)->pixelSize) {

	case 16:
		// QuickDraw specified a 16-bit surfaces is 5/5/5 with the highest bit unused
		masks[0] = 0x7C00;
		masks[1] = 0x03e0;
		masks[2] = 0x001f;
		
		mPixFormat.mRedZeroMask = 0x1f;
		mPixFormat.mGreenZeroMask = 0x1f;
		mPixFormat.mBlueZeroMask = 0x1f;
		mPixFormat.mAlphaZeroMask = 0;
		mPixFormat.mRedMask = masks[0];
		mPixFormat.mGreenMask = masks[1];
		mPixFormat.mBlueMask = masks[2];
		mPixFormat.mAlphaMask = 0;
		mPixFormat.mRedCount = 5;
		mPixFormat.mGreenCount = 5;
		mPixFormat.mBlueCount = 5;
		mPixFormat.mAlphaCount = 0;
		mPixFormat.mRedShift = 10;
		mPixFormat.mGreenShift = 5;
		mPixFormat.mBlueShift = 0;
		mPixFormat.mAlphaShift = 0;
		
		return NS_OK;
	case 32:
		masks[0] = 0xff0000;
		masks[1] = 0x00ff00;
		masks[2] = 0x0000ff;
		
		mPixFormat.mRedZeroMask = 0xff;
		mPixFormat.mGreenZeroMask = 0xff;
		mPixFormat.mBlueZeroMask = 0xff;
		
		mPixFormat.mRedMask = masks[0];
		mPixFormat.mGreenMask = masks[1];
		mPixFormat.mBlueMask = masks[2];
		
		mPixFormat.mRedCount = 8;
		mPixFormat.mGreenCount = 8;
		mPixFormat.mBlueCount = 8;
		
		mPixFormat.mRedShift = 16;
		mPixFormat.mGreenShift = 8;
		mPixFormat.mBlueShift = 0;
		
		/* jturner - I'm not sure if this is strictly necessary, but I'd be worried
		about some weak OS 8/9 video driver getting upset becuase the high byte wasn't
		zero. IM is a bit vauge about if this matters, so I'm being paranoid */
		
		#ifdef CARBON
			mPixFormat.mAlphaZeroMask = 0xff;
			mPixFormat.mAlphaMask = 0xff000000;
			mPixFormat.mAlphaShift = 24;
			mPixFormat.mAlphaCount = 8;
		#else
			mPixFormat.mAlphaZeroMask = 0;
			mPixFormat.mAlphaMask = 0;
			mPixFormat.mAlphaShift = 0;
			mPixFormat.mAlphaCount = 0;
		#endif
		
		return NS_OK;

	default:
		/** originally, I had this returning NS_ERROR_NOT_IMPLEMENTED; however, that's
		going to break the Init() methods for 8-bit depths (and lower). Instead we leave the
		PixelFormat wit it's default state, i.e all zeroes. Every single gfx implementation has
		different behaviour for these cases! */
		return NS_OK;
	}
}
