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
nsDrawingSurfaceMac::nsDrawingSurfaceMac()
{
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
nsDrawingSurfaceMac::~nsDrawingSurfaceMac()
{
	if(mIsOffscreen && mPort){
  	GWorldPtr offscreenGWorld = (GWorldPtr)mPort;
		::UnlockPixels(::GetGWorldPixMap(offscreenGWorld));
		::DisposeGWorld(offscreenGWorld);
		
		nsGraphicsUtils::SetPortToKnownGoodPort();
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
NS_IMETHODIMP nsDrawingSurfaceMac::QueryInterface(REFNSIID aIID, void** aInstancePtr)
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

#pragma mark-

/** --------------------------------------------------- 
 * See Documentation in nsIDrawingSurface.h
 * @update 3/02/99 dwc
 * @return error status
 */
NS_IMETHODIMP nsDrawingSurfaceMac::Lock(PRInt32 aX, PRInt32 aY,
                                          PRUint32 aWidth, PRUint32 aHeight,
                                          void **aBits, PRInt32 *aStride,
                                          PRInt32 *aWidthBytes, PRUint32 aFlags)
{

  if (!mIsLocked && mIsOffscreen && mPort)
  {
    // get the offscreen gworld for our use
    GWorldPtr     offscreenGWorld = (GWorldPtr)mPort;

    // calculate the pixel data size
    PixMapHandle  thePixMap = ::GetGWorldPixMap(offscreenGWorld);
    Ptr           baseaddr  = ::GetPixBaseAddr(thePixMap);
    PRInt32       cmpSize   = ((**thePixMap).pixelSize >> 3);
    PRInt32       rowBytes  = (**thePixMap).rowBytes & 0x3FFF;

    *aBits = baseaddr + (aX * cmpSize) + aY * rowBytes;
    *aStride = rowBytes;
    *aWidthBytes = aWidth * cmpSize;

    mIsLocked = PR_TRUE;
  }
  else
  {
    NS_ASSERTION(0, "nested lock attempt");
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

/** --------------------------------------------------- 
 * See Documentation in nsIDrawingSurface.h
 * @update 3/02/99 dwc
 * @return error status
 */
NS_IMETHODIMP nsDrawingSurfaceMac::Unlock(void)
{
	mIsLocked = PR_FALSE;
  return NS_OK;
}

/** --------------------------------------------------- 
 * See Documentation in nsIDrawingSurface.h
 * @update 3/02/99 dwc
 * @return error status
 */
NS_IMETHODIMP nsDrawingSurfaceMac::GetDimensions(PRUint32 *aWidth, PRUint32 *aHeight)
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
NS_IMETHODIMP nsDrawingSurfaceMac::IsPixelAddressable(PRBool *aAddressable)
{
  NS_ASSERTION(0, "Not implemented!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/** --------------------------------------------------- 
 * See Documentation in nsIDrawingSurface.h
 * @update 3/02/99 dwc
 * @return error status
 */
NS_IMETHODIMP nsDrawingSurfaceMac::GetPixelFormat(nsPixelFormat *aFormat)
{
  //*aFormat = mPixFormat;
  NS_ASSERTION(0, "Not implemented!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

#pragma mark -

/** --------------------------------------------------- 
 * See Documentation in nsIDrawingSurfaceMac.h
 * @update 3/02/99 dwc
 * @return error status
 */
NS_IMETHODIMP nsDrawingSurfaceMac::Init(nsIDrawingSurface*	aDS)
{
	nsDrawingSurfaceMac* surface = static_cast<nsDrawingSurfaceMac*>(aDS);
	surface->GetGrafPtr(&mPort);
	mGS->Init(surface);
	
  return NS_OK;
}

/** --------------------------------------------------- 
 * See Documentation in nsIDrawingSurfaceMac.h
 * @update 3/02/99 dwc
 * @return error status
 */
NS_IMETHODIMP nsDrawingSurfaceMac::Init(CGrafPtr aPort)
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
NS_IMETHODIMP nsDrawingSurfaceMac::Init(nsIWidget *aTheWidget)
{
	// get our native graphics port from the widget
 	mPort = reinterpret_cast<CGrafPtr>(aTheWidget->GetNativeData(NS_NATIVE_GRAPHIC));
	mGS->Init(aTheWidget);
  return NS_OK;
}

/** --------------------------------------------------- 
 * See Documentation in nsIDrawingSurfaceMac.h
 * @update 3/02/99 dwc
 * @return error status
 */

NS_IMETHODIMP nsDrawingSurfaceMac::Init(PRUint32 aDepth, PRUint32 aWidth, PRUint32 aHeight, PRUint32 aFlags)
{
  PRUint32	depth;
  Rect			macRect;
  GWorldPtr offscreenGWorld = nsnull;
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
	{
	  StGWorldPortSetter  setter(offscreenGWorld);
	  ::EraseRect(&macRect);
  }

	Init(offscreenGWorld);
	mIsOffscreen = PR_TRUE;
  return NS_OK;
}

