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
#include "nsImageMac.h"
#include "nsRenderingContextMac.h"
#include "nsDeviceContextMac.h"
#include "nsCarbonHelpers.h"

#include <MacTypes.h>
#include <QuickDraw.h>

#include "nsGfxUtils.h"

/** ---------------------------------------------------
 *	See documentation in nsImageMac.h
 *	@update 
 */
nsImageMac::nsImageMac()
:	mImageBitsHandle(nsnull)
,	mWidth(0)
,	mHeight(0)
,	mRowBytes(0)
,	mBytesPerPixel(0)
,	mMaskBitsHandle(nsnull)
,	mAlphaDepth(0)
,	mAlphaWidth(0)
,	mAlphaHeight(0)
,	mARowBytes(0)
, mNaturalWidth(0)
, mNaturalHeight(0)
,	mPixelDataSize(0)
,	mIsTopToBottom(PR_TRUE)

{
	NS_INIT_REFCNT();
	
	::memset(&mImagePixmap, 0, sizeof(PixMap));
	::memset(&mMaskPixmap, 0, sizeof(PixMap));
	
}

/** ---------------------------------------------------
 *	See documentation in nsImageMac.h
 *	@update 
 */
nsImageMac::~nsImageMac()
{
	if (mImageBitsHandle)
		::DisposeHandle(mImageBitsHandle);
		
	if (mMaskBitsHandle)
		::DisposeHandle(mMaskBitsHandle);
	
	// dispose of the color tables if we have them
	if (mImagePixmap.pmTable)
	{
		::DisposeCTable(mImagePixmap.pmTable);
	}
	
	if (mMaskPixmap.pmTable)
	{
		::DisposeCTable(mMaskPixmap.pmTable);
	}
		
}

NS_IMPL_ISUPPORTS2(nsImageMac, nsIImage, nsIImageMac);

/** ---------------------------------------------------
 *	See documentation in nsImageMac.h
 *	@update 
 */
PRUint8* 
nsImageMac::GetBits()
{
	if (!mImageBitsHandle)
	{
		NS_ASSERTION(0, "Getting bits for non-existent image");
		return nsnull;
	}
	
	// pixels should be locked here!
#if DEBUG
	SInt8		pixelFlags = HGetState(mImageBitsHandle);
	NS_ASSERTION(pixelFlags & (1 << 7), "Pixels must be locked here");
#endif
	
	return (PRUint8 *)*mImageBitsHandle;
}


/** ---------------------------------------------------
 *	See documentation in nsImageMac.h
 *	@update 
 */
PRUint8* 
nsImageMac::GetAlphaBits()
{
	if (!mMaskBitsHandle)
	{
		NS_ASSERTION(0, "Getting bits for non-existent image");
		return nsnull;
	}
	
	// pixels should be locked here!
#if DEBUG
	SInt8		pixelFlags = HGetState(mMaskBitsHandle);
	NS_ASSERTION(pixelFlags & (1 << 7), "Pixels must be locked here");
#endif
	
	return (PRUint8 *)*mMaskBitsHandle;
}


/** ---------------------------------------------------
 *	See documentation in nsImageMac.h
 *	@update 08/03/99 -- cleared out the image pointer - dwc
 */
nsresult 
nsImageMac::Init(PRInt32 aWidth, PRInt32 aHeight, PRInt32 aDepth, nsMaskRequirements aMaskRequirements)
{
  OSErr err = noErr;
  
	// have we already been initted?
	if (mImageBitsHandle)
	{
		NS_ASSERTION(0, "Initting image twice");
		
		::DisposeHandle(mImageBitsHandle);
		mImageBitsHandle = nsnull;
		
		if (mMaskBitsHandle) {
		  ::DisposeHandle(mMaskBitsHandle);
		  mMaskBitsHandle = nsnull;
		}		  
	}

  mWidth = aWidth;
  mHeight = aHeight;
  SetDecodedRect(0,0,0,0);  //init
  SetNaturalWidth(0);
  SetNaturalHeight(0);

  err = CreatePixMap(aWidth, aHeight, aDepth, nsnull, mImagePixmap, mImageBitsHandle);
  if (err != noErr)
  {
    if (err == memFullErr)
      nsMemory::HeapMinimize(PR_FALSE);
    return NS_ERROR_FAILURE;
  }

  mRowBytes = mImagePixmap.rowBytes & 0x3FFF;   // we only set the top bit, but QuickDraw can use the top 2 bits
			
	if (aMaskRequirements != nsMaskRequirements_kNoMask)
	{
  	CTabHandle	grayRamp = nsnull;

		switch (aMaskRequirements)
		{
			case nsMaskRequirements_kNeeds1Bit:
				mAlphaDepth = 1;
				break;
				
			case nsMaskRequirements_kNeeds8Bit:
/*
				err = MakeGrayscaleColorTable(256, &grayRamp);
				if (err != noErr)
					return NS_ERROR_OUT_OF_MEMORY;
*/					
				mAlphaDepth = 8;		  				
				break;
				
			default:
				NS_NOTREACHED("Uknown mask depth");
				break;
		}
		
    err = CreatePixMap(aWidth, aHeight, mAlphaDepth, grayRamp, mMaskPixmap, mMaskBitsHandle);
    if (err != noErr)
    {
      if (err == memFullErr)
        nsMemory::HeapMinimize(PR_FALSE);
      return NS_ERROR_FAILURE;
    }

    mARowBytes = mMaskPixmap.rowBytes & 0x3FFF;   // we only set the top bit, but QuickDraw can use the top 2 bits
	  mAlphaWidth = aWidth;
	  mAlphaHeight = aHeight;
	}
	
	return NS_OK;
}

/** ---------------------------------------------------
 *	See documentation in nsImageMac.h
 *	@update 
 */
void nsImageMac::ImageUpdated(nsIDeviceContext *aContext, PRUint8 aFlags, nsRect *aUpdateRect)
{
	// NS_NOTYETIMPLEMENTED("nsImageMac::ImageUpdated not implemented");
}


/** ---------------------------------------------------
 *	See documentation in nsImageMac.h
 *	@update 
 */
NS_IMETHODIMP nsImageMac::Draw(nsIRenderingContext &aContext, nsDrawingSurface aSurface, PRInt32 aSX, PRInt32 aSY,
								 PRInt32 aSWidth, PRInt32 aSHeight, PRInt32 aDX, PRInt32 aDY, PRInt32 aDWidth, PRInt32 aDHeight)
{
	Rect								srcRect, dstRect, maskRect;

	if (!mImageBitsHandle)
		return NS_ERROR_FAILURE;

  // lock and set up bits handles
  StHandleLocker  imageBitsLocker(mImageBitsHandle);
  StHandleLocker  maskBitsLocker(mMaskBitsHandle);    // ok with nil handle

  mImagePixmap.baseAddr = *mImageBitsHandle;
  if (mMaskBitsHandle)
    mMaskPixmap.baseAddr = *mMaskBitsHandle;

	// currently the top is 0, this may change and this code will have to reflect that
	if (( mDecodedY2 < aSHeight) ) {
		// adjust the source and dest height to reflect this
		aDHeight = float(mDecodedY2/float(aSHeight)) * aDHeight;
		aSHeight = mDecodedY2;
	}	

	::SetRect(&srcRect, aSX, aSY, aSX + aSWidth, aSY + aSHeight);
	maskRect = srcRect;
	::SetRect(&dstRect, aDX, aDY, aDX + aDWidth, aDY + aDHeight);

	::ForeColor(blackColor);
	::BackColor(whiteColor);
	
	// get the destination pix map
	nsDrawingSurfaceMac* surface = static_cast<nsDrawingSurfaceMac*>(aSurface);
	CGrafPtr		destPort;
	nsresult		rv = surface->GetGrafPtr((GrafPtr *)&destPort);
	if (NS_FAILED(rv)) return rv;
	
	PixMapHandle		destPixels = ::GetGWorldPixMap(destPort);
	NS_ASSERTION(destPixels, "No dest pixels!");
	
	// can only do this if we are NOT printing
	if (RenderingToPrinter(aContext))		// we are printing
	{
		if (!mMaskBitsHandle)
		{
			::CopyBits((BitMap*)&mImagePixmap, (BitMap*)*destPixels, &srcRect, &dstRect, srcCopy, nsnull);
		}
		else
		{
			GWorldPtr	tempGWorld;
			
			// if we have a mask, blit the transparent image into a new GWorld which is
			// just white, and print that. This is marginally better than printing the
			// image directly, since the transparent pixels come out black.
			
			// We do all this because Copy{Deep}Mask is not supported when printing
			if (AllocateGWorld(mImagePixmap.pixelSize, nsnull, srcRect, &tempGWorld) == noErr)
			{
			  // erase it to white
				ClearGWorld(tempGWorld);

				PixMapHandle		tempPixMap = ::GetGWorldPixMap(tempGWorld);
				if (tempPixMap)
				{
					StPixelLocker		tempPixLocker(tempPixMap);			// locks the pixels
				
					// copy from the destination into our temp GWorld, to get the background
					// for some reason this copies garbage, so we erase to white above instead.
					// ::CopyBits((BitMap*)*destPixels, (BitMap*)*tempPixMap, &dstRect, &srcRect, srcCopy, nsnull);
					
					if (mAlphaDepth > 1)
						::CopyDeepMask((BitMap*)&mImagePixmap, (BitMap*)&mMaskPixmap, (BitMap*)*tempPixMap, &srcRect, &maskRect, &srcRect, srcCopy, nsnull);
					else
						::CopyMask((BitMap*)&mImagePixmap, (BitMap*)&mMaskPixmap, (BitMap*)*tempPixMap, &srcRect, &maskRect, &srcRect);

					// now copy to the screen
					::CopyBits((BitMap*)*tempPixMap, (BitMap*)*destPixels, &srcRect, &dstRect, srcCopy, nsnull);
				}
				
				::DisposeGWorld(tempGWorld);	// do this after dtor of tempPixLocker!
			}
		}
	}
	else	// not printing
  {
    CopyBitsWithMask((BitMap*)(&mImagePixmap),
        mMaskBitsHandle ? (BitMap*)(&mMaskPixmap) : nsnull, mAlphaDepth,
        (BitMap*)(*destPixels),
        srcRect, maskRect, dstRect);
	}
	
	return NS_OK;
}

/** ---------------------------------------------------
 *	See documentation in nsImageMac.h
 *	@update 
 */
NS_IMETHODIMP nsImageMac :: Draw(nsIRenderingContext &aContext, 
								 nsDrawingSurface aSurface,
								 PRInt32 aX, PRInt32 aY, 
								 PRInt32 aWidth, PRInt32 aHeight)
{

	return Draw(aContext,aSurface,0,0,mWidth,mHeight,aX,aY,aWidth,aHeight);
}
 
 #ifdef USE_IMG2
/** ---------------------------------------------------
 *	See documentation in nsImageMac.h
 *	@update 
 */
NS_IMETHODIMP nsImageMac :: DrawToImage(nsIImage* aDstImage, PRInt32 aDX, PRInt32 aDY, PRInt32 aDWidth, PRInt32 aDHeight)
{
  Rect srcRect, dstRect, maskRect;

  if (!mImageBitsHandle)
    return NS_ERROR_FAILURE;

  // lock and set up bits handles
  LockImagePixels(PR_FALSE);
  LockImagePixels(PR_TRUE);

  ::SetRect(&srcRect, 0, 0, mWidth, mHeight);
  maskRect = srcRect;
  ::SetRect(&dstRect, aDX, aDY, aDX + aDWidth, aDY + aDHeight);

  ::ForeColor(blackColor);
  ::BackColor(whiteColor);
	
  // get the destination pix map
  aDstImage->LockImagePixels(PR_FALSE);
  aDstImage->LockImagePixels(PR_TRUE);
  //nsImageMac* dstMacImage = static_cast<nsImageMac*>(aDstImage);
  nsCOMPtr<nsIImageMac> dstMacImage( do_QueryInterface(aDstImage));
	
  PixMap* destPixels;
  dstMacImage->GetPixMap(&destPixels);
  NS_ASSERTION(destPixels, "No dest pixels!");
          
  CopyBitsWithMask((BitMap*)(&mImagePixmap),
      mMaskBitsHandle ? (BitMap*)(&mMaskPixmap) : nsnull, mAlphaDepth,
      (BitMap*)(destPixels), srcRect, maskRect, dstRect);
  
  aDstImage->UnlockImagePixels(PR_FALSE);
  aDstImage->UnlockImagePixels(PR_TRUE);
  UnlockImagePixels(PR_FALSE);
  UnlockImagePixels(PR_TRUE);
  
  return NS_OK;
}
#endif // USE_IMG2
  

/** ---------------------------------------------------
 *	See documentation in nsImageMac.h
 *	@update 
 */
nsresult nsImageMac::Optimize(nsIDeviceContext* aContext)
{
	return NS_OK;
}


#pragma mark -

/** ---------------------------------------------------
 *	See documentation in nsImageMac.h
 *	@update 
 */
void nsImageMac::SetAlphaLevel(PRInt32 aAlphaLevel)
{
	NS_NOTYETIMPLEMENTED("nsImageMac::SetAlphaLevel not implemented");
}


/** ---------------------------------------------------
 *	See documentation in nsImageMac.h
 *	@update 
 */
PRInt32 nsImageMac::GetAlphaLevel()
{
	NS_NOTYETIMPLEMENTED("nsImageMac::GetAlphaLevel not implemented");
	return 0;
}

#pragma mark -

/** ---------------------------------------------------
 *	Lock down the image pixels
 */
NS_IMETHODIMP
nsImageMac::LockImagePixels(PRBool aMaskPixels)
{
  if (!mImageBitsHandle)
    return NS_ERROR_NOT_INITIALIZED;
  
  if (aMaskPixels && !mMaskBitsHandle)
  	return NS_ERROR_NOT_INITIALIZED;
  	
  Handle thePixelsHandle = (aMaskPixels ? mMaskBitsHandle : mImageBitsHandle);
  ::HLock(thePixelsHandle);

  if(aMaskPixels)
    mMaskPixmap.baseAddr = *thePixelsHandle;
  else
    mImagePixmap.baseAddr = *thePixelsHandle;
    
  return NS_OK;
}

/** ---------------------------------------------------
 *	Unlock the pixels
 */
NS_IMETHODIMP
nsImageMac::UnlockImagePixels(PRBool aMaskPixels)
{
  if (!mImageBitsHandle)
    return NS_ERROR_NOT_INITIALIZED;
  
  if (aMaskPixels && !mMaskBitsHandle)
  	return NS_ERROR_NOT_INITIALIZED;
  	
	Handle		thePixelsHandle = (aMaskPixels ? mMaskBitsHandle : mImageBitsHandle);
	::HUnlock(thePixelsHandle);
	
  if(aMaskPixels)
    mMaskPixmap.baseAddr = 0;
  else
    mImagePixmap.baseAddr = 0;
    
	return NS_OK;
}

#pragma mark -

/** -----------------------------------------------------------------
 *	Create a PixMap, filling in ioPixMap
 */
OSErr nsImageMac::CreatePixMap(PRInt32 aWidth, PRInt32 aHeight, PRInt32 aDepth, CTabHandle aColorTable, PixMap& ioPixMap, Handle& ioBitsHandle)
{
	PRInt16 bufferdepth;
	OSErr   err = noErr;

	ioPixMap.cmpCount = 0;
	
	// See IM:QuickDraw pp 4-92 for GetCTable params
  switch(aDepth)
  {
    case 1:
      ioPixMap.pixelType = 0;
      ioPixMap.cmpCount = 1;
      ioPixMap.cmpSize = 1;
      ioPixMap.pmTable = aColorTable ? aColorTable : GetCTable(32 + 1);		// default to black & white colortable
      bufferdepth = 1;
      break;
      
    case 8:
      ioPixMap.pixelType = 0;
      ioPixMap.cmpCount = 1;
      ioPixMap.cmpSize = 8;
      ioPixMap.pmTable = aColorTable ? aColorTable : GetCTable(32 + 8);		// default to gray ramp colortable
      bufferdepth = 8;
      break;
      
    case 16:
      ioPixMap.pixelType = RGBDirect;
      ioPixMap.cmpCount = 3;
      ioPixMap.cmpSize = 5;
      ioPixMap.pmTable = nsnull;
      bufferdepth = 16;
      break;
      
    case 24:      // 24 and 32 bit are basically the same
    case 32:
      ioPixMap.pixelType = RGBDirect;
      ioPixMap.cmpCount = 3;
      ioPixMap.cmpSize = 8;
      ioPixMap.pmTable = nsnull;
      bufferdepth = 32;
      break;
      
    default:
      NS_ASSERTION(0, "Unhandled image depth");
      return paramErr;
  }
  
  if (ioPixMap.cmpCount)
  {
    PRInt32   imageSize;
    PRInt32   rowBytes = CalculateRowBytes(aWidth, bufferdepth);
    
    if (rowBytes >= 0x4000)
    {
      NS_ASSERTION(0, "PixMap too big for QuickDraw");
      return paramErr;
    }
      
    imageSize = rowBytes * aHeight;
    
    err = AllocateBitsHandle(imageSize, &ioBitsHandle);
    if (err != noErr)
			return err;

    ioPixMap.baseAddr = nsnull;     // We can only set this after locking the pixels handle
    ioPixMap.rowBytes = rowBytes | 0x8000;    // set the high bit to tell CopyBits that this is a PixMap
    ioPixMap.bounds.top = 0;
    ioPixMap.bounds.left = 0;
    ioPixMap.bounds.bottom = aHeight;
    ioPixMap.bounds.right = aWidth;
    ioPixMap.pixelSize = bufferdepth;
    ioPixMap.packType = 0;
    ioPixMap.packSize = 0;
    ioPixMap.hRes = nsDeviceContextMac::GetScreenResolution() << 16;    // is this correct? printing?
    ioPixMap.vRes = nsDeviceContextMac::GetScreenResolution() << 16;
#if TARGET_CARBON
    ioPixMap.pixelFormat = 0;				/*fourCharCode representation*/
    ioPixMap.pmExt = 0;	
#else
    ioPixMap.planeBytes = 0;
    ioPixMap.pmReserved = 0;
#endif
    ioPixMap.pmVersion = 0;
  }	

  return noErr;
}


/** -----------------------------------------------------------------
 *	Allocate bits handle, trying first in the heap, and then in temp mem.
 */
OSErr nsImageMac::AllocateBitsHandle(PRInt32 imageSizeBytes, Handle *outHandle)
{
	*outHandle = nsnull;
	
  // We have to be careful here not to fill the heap.  // 
  // The strategy is this:
  //   1. If we have plenty of heap space free, allocate the GWorld in
  //      the heap.
  //
  //   2. When below a certain threshold of free space in the heap,
  //      allocate GWorlds in temp mem.
  //

  // threshold at which we go to temp mem
	const long kUseTempMemFreeSpace = (1024 * 1024);
	const long kUseTempMemContigSpace	= (768 * 1024);
	
	long	totalSpace, contiguousSpace;
	::PurgeSpace(&totalSpace, &contiguousSpace);		// this does not purge memory!
	
	if (totalSpace > kUseTempMemFreeSpace && contiguousSpace > kUseTempMemContigSpace)
	{
		*outHandle = ::NewHandleClear(imageSizeBytes);
		if (*outHandle) return noErr;
	}
  
  OSErr err;
  *outHandle = ::TempNewHandle(imageSizeBytes, &err);
  if (! *outHandle)
  	return memFullErr;
  
  ::BlockZero(**outHandle, imageSizeBytes);
  return noErr;
}


/** ---------------------------------------------------
 *	Set the decoded dimens of the image
 */
NS_IMETHODIMP
nsImageMac::SetDecodedRect(PRInt32 x1, PRInt32 y1, PRInt32 x2, PRInt32 y2 )
{
    
  mDecodedX1 = x1; 
  mDecodedY1 = y1; 
  mDecodedX2 = x2; 
  mDecodedY2 = y2; 
  return NS_OK;
}


/** ---------------------------------------------------
 *  Calculate rowBytes, making sure that it comes out as
 *  a multiple of 4. ( 32 / 4 == 8)
 *	See <http://developer.apple.com/technotes/qd/qd_15.html>
 */
PRInt32  nsImageMac::CalculateRowBytes(PRUint32 aWidth, PRUint32 aDepth)
{
  PRInt32   rowBytes = ((aWidth * aDepth + 31) / 32) * 4;
  return rowBytes;
}


#pragma mark -

/** ---------------------------------------------------
 *	Erase the GWorld contents
 */
void nsImageMac::ClearGWorld(GWorldPtr theGWorld)
{
	PixMapHandle	thePixels;
	GWorldPtr			curPort;
	GDHandle			curDev;

	thePixels = ::GetGWorldPixMap(theGWorld);
	::GetGWorld(&curPort, &curDev);

	StPixelLocker	pixelLocker(thePixels);
	
	// Black the offscreen
	::SetGWorld(theGWorld, nil);
	::BackColor(whiteColor);

  Rect portRect;
  ::GetPortBounds(reinterpret_cast<GrafPtr>(theGWorld), &portRect);
	::EraseRect(&portRect);

	::SetGWorld(curPort, curDev);
}

/** -----------------------------------------------------------------
 *	Allocate a GWorld, trying first in the heap, and then in temp mem.
 */
OSErr nsImageMac::AllocateGWorld(PRInt16 depth, CTabHandle colorTable, const Rect& bounds, GWorldPtr *outGWorld)
{
	*outGWorld = nsnull;
	
  // We have to be careful here not to fill the heap with GWorld pieces.
  // The key to understanding this is that, even if you allocate a GWorld
  // with the 'useTempMem' flag, it puts data handles in the heap (GDevices,
  // color tables etc).
  // 
  // The strategy is this:
  //   1. If we have plenty of heap space free, allocate the GWorld in
  //      the heap.
  //
  //   2. When below a certain threshold of free space in the heap,
  //      allocate GWorlds in temp mem.
  //
  //   3. When memory is really tight, refuse to allocate a GWorld at
  //      all.

  // threshold at which we go to temp mem
	const long kUseTempMemFreeSpace = (1024 * 1024);
	const long kUseTempMemContigSpace	= (768 * 1024);
	
	long	totalSpace, contiguousSpace;
	::PurgeSpace(&totalSpace, &contiguousSpace);		// this does not purge memory!
	
	if (totalSpace > kUseTempMemFreeSpace && contiguousSpace > kUseTempMemContigSpace)
	{
		::NewGWorld(outGWorld, depth, &bounds, colorTable, nsnull, 0);
		if (*outGWorld) return noErr;
	}

	// theshold at which we refuse to allocate at all
	const long kReserveHeapFreeSpace = (512 * 1024);
	const long kReserveHeapContigSpace	= (384 * 1024);
	
	if (totalSpace > kReserveHeapFreeSpace && contiguousSpace > kReserveHeapContigSpace)
	{
  	::NewGWorld(outGWorld, depth, &bounds, colorTable, nsnull, useTempMem);
		if (*outGWorld) return noErr;
	}

	return memFullErr;
}

void nsImageMac::CopyBitsWithMask(BitMap* srcBits, BitMap* maskBits, PRInt16 maskDepth, BitMap* destBits,
        const Rect& srcRect, const Rect& maskRect, const Rect& destRect)
{
  if (maskBits)
  {
    if (maskDepth > 1)
      ::CopyDeepMask(srcBits, maskBits, destBits, &srcRect, &maskRect, &destRect, srcCopy, nsnull);
    else
      ::CopyMask(srcBits, maskBits, destBits, &srcRect, &maskRect, &destRect);
  }
  else
  {
    ::CopyBits(srcBits, destBits, &srcRect, &destRect, srcCopy, nsnull);
  }
}


PRBool nsImageMac::RenderingToPrinter(nsIRenderingContext &aContext)
{
  nsCOMPtr<nsIDeviceContext> dc;                   // (this screams for a private interface, sigh!)
  aContext.GetDeviceContext(*getter_AddRefs(dc));
  // a weird and wonderful world of scanky casts and oddly-named intefaces.
  nsDeviceContextMac* theDevContext = NS_REINTERPRET_CAST(nsDeviceContextMac*, dc.get());
  return theDevContext && theDevContext->IsPrinter();
}



#pragma mark -


//
// ConvertToPICT
//
// Convert from image bits to a PICT, probably for placement on the clipboard.
// Blit the transparent image into a new GWorld which is just white, and
// then blit that into the picture. We can't just blit directly into
// the picture because CopyDeepMask isn't supported on PICTs.
//
NS_IMETHODIMP
nsImageMac :: ConvertToPICT ( PicHandle* outPicture )
{
  *outPicture = nsnull;

  Rect picFrame = mImagePixmap.bounds;
  Rect maskFrame = mMaskPixmap.bounds;
  GWorldPtr	tempGWorld;
  if (AllocateGWorld(mImagePixmap.pixelSize, nsnull, picFrame, &tempGWorld) == noErr) 
  {
    // erase it to white
  	ClearGWorld(tempGWorld);

  	PixMapHandle tempPixMap = ::GetGWorldPixMap(tempGWorld);
  	if (tempPixMap) {
  		StPixelLocker tempPixLocker(tempPixMap);			// locks the pixels
  	
  	  // the handles may have moved, make sure we reset baseAddr of our
  	  // pixmaps to the new pointer.
      StHandleLocker  imageBitsLocker(mImageBitsHandle);
      StHandleLocker  maskBitsLocker(mMaskBitsHandle);    // ok with nil handle
      mImagePixmap.baseAddr = *mImageBitsHandle;
      if (mMaskBitsHandle)
        mMaskPixmap.baseAddr = *mMaskBitsHandle;

  		// copy from the destination into our temp GWorld, to get the background
      CopyBitsWithMask((BitMap*)(&mImagePixmap),
          mMaskBitsHandle ? (BitMap*)(&mMaskPixmap) : nsnull, mAlphaDepth,
          (BitMap*)(*tempPixMap), picFrame, maskFrame, picFrame);
      
  		// now copy into the picture
  		GWorldPtr currPort;
  		GDHandle currDev;
      ::GetGWorld(&currPort, &currDev);
      ::SetGWorld(tempGWorld, nil);    
      PicHandle thePicture = ::OpenPicture(&picFrame);
      OSErr err = noErr;
      if ( thePicture ) {
        ::ForeColor(blackColor);
        ::BackColor(whiteColor);
      
  		  ::CopyBits((BitMap*)*tempPixMap, (BitMap*)*tempPixMap,
  		               &picFrame, &picFrame, srcCopy, nsnull);
      
        ::ClosePicture();
        err = QDError();
      }
      
      ::SetGWorld(currPort, currDev);     // restore to the way things were
      
      if ( err == noErr )       
        *outPicture = thePicture;
  	}
	
	  ::DisposeGWorld(tempGWorld);	      // do this after dtor of tempPixLocker!
  }
  else
    return NS_ERROR_FAILURE;
  
  return NS_OK;
 
} // ConvertToPICT


NS_IMETHODIMP
nsImageMac :: ConvertFromPICT ( PicHandle inPicture )
{
  return NS_ERROR_FAILURE;
 
} // ConvertFromPICT

NS_IMETHODIMP
nsImageMac::GetPixMap ( PixMap** aPixMap )
{
  *aPixMap = &mImagePixmap;
  return NS_OK;
}

nsresult nsImageMac::SlowTile(nsIRenderingContext &aContext,
                                   nsDrawingSurface aSurface,
                                   PRInt32 aSXOffset, PRInt32 aSYOffset,
                                   const nsRect &aTileRect)
{
  PRInt32
    validX = 0,
    validY = 0,
    validWidth  = mWidth,
    validHeight = mHeight;
  
  // limit the image rectangle to the size of the image data which
  // has been validated.
  if (mDecodedY2 < mHeight) {
    validHeight = mDecodedY2 - mDecodedY1;
  }
  if (mDecodedX2 < mWidth) {
    validWidth = mDecodedX2 - mDecodedX1;
  }
  if (mDecodedY1 > 0) {   
    validHeight -= mDecodedY1;
    validY = mDecodedY1;
  }
  if (mDecodedX1 > 0) {
    validWidth -= mDecodedX1;
    validX = mDecodedX1; 
  }

  PRInt32 aY0 = aTileRect.y - aSYOffset,
          aX0 = aTileRect.x - aSXOffset,
          aY1 = aTileRect.y + aTileRect.height,
          aX1 = aTileRect.x + aTileRect.width;

  for (PRInt32 y = aY0; y < aY1; y += mHeight)
    for (PRInt32 x = aX0; x < aX1; x += mWidth)
    {
      Draw(aContext, aSurface,
           0, 0, PR_MIN(validWidth, aX1-x), PR_MIN(validHeight, aY1-y),     // src coords
           x, y, PR_MIN(validWidth, aX1-x), PR_MIN(validHeight, aY1-y));    // dest coords
    }

  return NS_OK;
}



// Fast tiling algorithm that uses successive doubling of the tile in a GWorld
// to scale it up. This code does not deal with images whose masks are a different
// size to the image (currently never happens), nor does it deal with partially
// decoded images. Because we allocate our image bits handles zero'd out, I don't
// think this matters.
//
// This code is not called for printing (because all the CopyDeepMask stuff doesn't
// work when printing).

nsresult nsImageMac::DrawTileQuickly(nsIRenderingContext &aContext,
                                   nsDrawingSurface aSurface,
                                   PRInt32 aSXOffset, PRInt32 aSYOffset,
                                   const nsRect &aTileRect)
{
  // lock and set up bits handles
  StHandleLocker  imageBitsLocker(mImageBitsHandle);
  StHandleLocker  maskBitsLocker(mMaskBitsHandle);    // ok with nil handle

  mImagePixmap.baseAddr = *mImageBitsHandle;
  if (mMaskBitsHandle)
    mMaskPixmap.baseAddr = *mMaskBitsHandle;

	Rect	imageRect;
	imageRect.left = 0;
	imageRect.top = 0;
	imageRect.right = mWidth;
	imageRect.bottom = mHeight;

  // set up the dest port
  ::ForeColor(blackColor);
  ::BackColor(whiteColor);
  
  // this code assumes that, if we have a mask, it's the same size as the image
  NS_ASSERTION((mMaskBitsHandle == nsnull) || (mAlphaWidth == mWidth && mAlphaHeight == mHeight), "Mask should be same dimensions as image");
  
  // get the destination pix map
  nsDrawingSurfaceMac* surface = static_cast<nsDrawingSurfaceMac*>(aSurface);
  CGrafPtr    destPort;
  nsresult    rv = surface->GetGrafPtr((GrafPtr *)&destPort);
  if (NS_FAILED(rv)) return rv;
  PixMapHandle    destPixels = ::GetGWorldPixMap(destPort);
  StPixelLocker   destPixLocker(destPixels);
  
  // How many tiles will we need? Allocating GWorlds is expensive,
  // so if only a few tilings are required, the old way is preferable.
  const PRInt32	kTilingCopyThreshold = 64;
  
  PRInt32	tilingBoundsWidth 	= aSXOffset + aTileRect.width;
  PRInt32	tilingBoundsHeight 	= aSYOffset + aTileRect.height;

  PRInt32	tiledRows = (tilingBoundsHeight + mHeight - 1) / mHeight;   // round up
  PRInt32	tiledCols = (tilingBoundsWidth + mWidth - 1) / mWidth;      // round up

  PRInt32	numTiles = tiledRows * tiledCols;
  if (numTiles <= kTilingCopyThreshold)
  {
    // the outside bounds of the tiled area
    PRInt32 aY0 = aTileRect.y - aSYOffset,
            aX0 = aTileRect.x - aSXOffset,
            aY1 = aTileRect.y + aTileRect.height,
            aX1 = aTileRect.x + aTileRect.width;

  	for (PRInt32 y = aY0; y < aY1; y += mHeight)
  	{
	    for (PRInt32 x = aX0; x < aX1; x += mWidth)
  		{
	  		Rect		imageDestRect = imageRect;
	  		::OffsetRect(&imageDestRect, x, y);
	  		
	  		// CopyBits will do the truncation for us at the edges
        CopyBitsWithMask((BitMap*)(&mImagePixmap),
            mMaskBitsHandle ? (BitMap*)(&mMaskPixmap) : nsnull, mAlphaDepth,
            (BitMap*)(*destPixels), imageRect, imageRect, imageDestRect);
	  	}
  	}
  
  	return NS_OK;
  }

  Rect  tileDestRect;   // aTileRect as a Mac rect
  tileDestRect.left   = aTileRect.x;
  tileDestRect.top    = aTileRect.y;
  tileDestRect.bottom = tileDestRect.top  + aTileRect.height;
  tileDestRect.right  = tileDestRect.left + aTileRect.width;

  Rect  tileRect = tileDestRect;
  ::OffsetRect(&tileRect, -tileRect.left, -tileRect.top);   // offset to {0, 0}
  GWorldPtr   tilingGWorld = nsnull;
  OSErr err = AllocateGWorld(mImagePixmap.pixelSize, nsnull, tileRect, &tilingGWorld);
  if (err != noErr) return NS_ERROR_OUT_OF_MEMORY;
  
  GWorldPtr   maskingGWorld = nsnull;
  if (mMaskBitsHandle)
  {
    err = AllocateGWorld(mMaskPixmap.pixelSize, nsnull, tileRect, &maskingGWorld);
    if (err != noErr) {
      ::DisposeGWorld(tilingGWorld);
      return NS_ERROR_OUT_OF_MEMORY;
    }
    
    ClearGWorld(maskingGWorld);
  }  
  
  PixMapHandle    tilingPixels = ::GetGWorldPixMap(tilingGWorld);
  PixMapHandle    maskingPixels = (maskingGWorld) ? ::GetGWorldPixMap(maskingGWorld) : nsnull;

  {   // scope for locks
    StPixelLocker   tempPixLocker(tilingPixels);
    StPixelLocker   tempMaskLocker(maskingPixels);   // OK will null pixels

    // our strategy here is to avoid allocating a GWorld which is bigger than the destination
    // area, by creating a tileable area in the top left of the GWorld by taking parts of
    // our tiled image. If aXOffset and aYOffset are 0, this tile is simply the image. If not, it
    // is a composite of 2 or 4 segments of the image.
    // Then we double up that tile as necessary.

/*

     +---------+
     |         |
     | X |  Y  |
     |         |
     | - +-------------------------+
     | Z |  W  | Z |               |
     +---|-----+---          |     |
         |     |   |    1          |
         |  Y  | X |         |     |
         |     |   |               |
         |---------+ - - - - +   3 |
         |                         |
         |                   |     |
         |         2               |
         |                   |     |
         |                         |
         | - - - - - - - - - + - - |
         |                         |
         |            4            |
         |                         |
         +-------------------------+

*/
    
    Rect    tilePartRect = imageRect;

    // top left of offset tile (W)
    Rect    offsetTileSrc = tilePartRect;
    offsetTileSrc.left  = aSXOffset;
    offsetTileSrc.top   = aSYOffset;

    Rect    offsetTileDest = {0};
    offsetTileDest.right  = tilePartRect.right - aSXOffset;
    offsetTileDest.bottom = tilePartRect.bottom - aSYOffset;
    
    ::CopyBits((BitMap*)&mImagePixmap, (BitMap*)*tilingPixels, &offsetTileSrc, &offsetTileDest, srcCopy, nsnull);
    if (maskingPixels)
      ::CopyBits((BitMap*)&mMaskPixmap, (BitMap*)*maskingPixels, &offsetTileSrc, &offsetTileDest, srcCopy, nsnull);

    // top right of offset tile (Z)
    if (aSXOffset > 0)
    {
      offsetTileSrc = tilePartRect;
      offsetTileSrc.right = aSXOffset;
      offsetTileSrc.top   = aSYOffset;
      
      offsetTileDest = tilePartRect;
      offsetTileDest.left   = tilePartRect.right - aSXOffset;
      offsetTileDest.bottom = tilePartRect.bottom - aSYOffset;

      ::CopyBits((BitMap*)&mImagePixmap, (BitMap*)*tilingPixels, &offsetTileSrc, &offsetTileDest, srcCopy, nsnull);
      if (maskingPixels)
        ::CopyBits((BitMap*)&mMaskPixmap, (BitMap*)*maskingPixels, &offsetTileSrc, &offsetTileDest, srcCopy, nsnull);    
    }
    
    if (aSYOffset > 0)
    {
      // bottom left of offset tile (Y)
      offsetTileSrc = tilePartRect;
      offsetTileSrc.left    = aSXOffset;
      offsetTileSrc.bottom  = aSYOffset;
      
      offsetTileDest = tilePartRect;
      offsetTileDest.right  = tilePartRect.right - aSXOffset;
      offsetTileDest.top    = tilePartRect.bottom - aSYOffset;

      ::CopyBits((BitMap*)&mImagePixmap, (BitMap*)*tilingPixels, &offsetTileSrc, &offsetTileDest, srcCopy, nsnull);
      if (maskingPixels)
        ::CopyBits((BitMap*)&mMaskPixmap, (BitMap*)*maskingPixels, &offsetTileSrc, &offsetTileDest, srcCopy, nsnull);      
    }
    
    if (aSXOffset > 0 && aSYOffset > 0)
    {
      // bottom right of offset tile (X)
      offsetTileSrc = tilePartRect;
      offsetTileSrc.right   = aSXOffset;
      offsetTileSrc.bottom  = aSYOffset;
      
      offsetTileDest = tilePartRect;
      offsetTileDest.left   = tilePartRect.right - aSXOffset;
      offsetTileDest.top    = tilePartRect.bottom - aSYOffset;

      ::CopyBits((BitMap*)&mImagePixmap, (BitMap*)*tilingPixels, &offsetTileSrc, &offsetTileDest, srcCopy, nsnull);
      if (maskingPixels)
        ::CopyBits((BitMap*)&mMaskPixmap, (BitMap*)*maskingPixels, &offsetTileSrc, &offsetTileDest, srcCopy, nsnull);
    }

    // now double up this tile to cover the area
    PRBool doneWidth = PR_FALSE, doneHeight = PR_FALSE;
    
    Rect srcRect, dstRect;
    PRInt32 tileWidth  = mWidth;
    PRInt32 tileHeight = mHeight;
    while (!doneWidth || !doneHeight)
    {
      if (tileWidth < tileRect.right)
      {
        srcRect.left = 0; srcRect.top = 0;
        srcRect.bottom = tileHeight; srcRect.right = tileWidth;

        dstRect.left = tileWidth; dstRect.top = 0;
        dstRect.bottom = tileHeight; dstRect.right = tileWidth + tileWidth;
        
        ::CopyBits((BitMap*)*tilingPixels, (BitMap*)*tilingPixels, &srcRect, &dstRect, srcCopy, nsnull);
        if (maskingPixels)
          ::CopyBits((BitMap*)*maskingPixels, (BitMap*)*maskingPixels, &srcRect, &dstRect, srcCopy, nsnull);

        tileWidth *= 2;
      }
      else
        doneWidth = PR_TRUE;
    
      if (tileHeight < tileRect.bottom)
      {
        srcRect.left = 0; srcRect.top = 0;
        srcRect.bottom = tileHeight; srcRect.right = tileWidth;

        dstRect.left = 0; dstRect.top = tileHeight;
        dstRect.bottom = tileHeight + tileHeight; dstRect.right = tileWidth;
        
        ::CopyBits((BitMap*)*tilingPixels, (BitMap*)*tilingPixels, &srcRect, &dstRect, srcCopy, nsnull);
        if (maskingPixels)
          ::CopyBits((BitMap*)*maskingPixels, (BitMap*)*maskingPixels, &srcRect, &dstRect, srcCopy, nsnull);
      
        tileHeight *= 2;
      }
      else
        doneHeight = PR_TRUE;
    }
    
    // We could optimize this a little more by making the temp GWorld 1/4 the size of the dest, 
    // and doing 4 final blits directly to the destination.
    
    // finally, copy to the destination
    CopyBitsWithMask((BitMap*)(*tilingPixels),
        maskingPixels ? (BitMap*)(*maskingPixels) : nsnull, mAlphaDepth,
        (BitMap*)(*destPixels), tileRect, tileRect, tileDestRect);
    
  } // scope for locks

  // clean up  
  ::DisposeGWorld(tilingGWorld);
  if (maskingGWorld)
    ::DisposeGWorld(maskingGWorld);

  return NS_OK;
}


NS_IMETHODIMP nsImageMac::DrawTile(nsIRenderingContext &aContext,
                                   nsDrawingSurface aSurface,
                                   PRInt32 aSXOffset, PRInt32 aSYOffset,
                                   const nsRect &aTileRect)
{
  nsresult rv = NS_ERROR_FAILURE;
  
	if (!RenderingToPrinter(aContext))
    rv = DrawTileQuickly(aContext, aSurface, aSXOffset, aSYOffset, aTileRect);

  if (NS_FAILED(rv))
    rv = SlowTile(aContext, aSurface, aSXOffset, aSYOffset, aTileRect);
    
  return rv;
}


