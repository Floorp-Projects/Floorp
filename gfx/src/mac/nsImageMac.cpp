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
 * Communications Corporation.	Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#include "nsImageMac.h"
#include "nsRenderingContextMac.h"
#include "nsDeviceContextMac.h"
#include "nsCarbonHelpers.h"

#include <Types.h>
#include <QuickDraw.h>

#include "nsGfxUtils.h"

#include "nspr.h"

#define IsFlagSet(a,b) (a & b)

static NS_DEFINE_IID(kIImageIID, NS_IIMAGE_IID);

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
,	mIsTopToBottom(PR_TRUE)
,	mPixelDataSize(0)
, mNaturalWidth(0)
, mNaturalHeight(0)

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

NS_IMPL_ISUPPORTS(nsImageMac, kIImageIID);

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
		PRInt16     mAlphaDepth = 0;

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
	
	PixMapHandle		destPixels = GetGWorldPixMap(destPort);
	NS_ASSERTION(destPixels, "No dest pixels!");
	
	// can only do this if we are NOT printing
	nsCOMPtr<nsDeviceContextMac> theDevContext;
	aContext.GetDeviceContext(*getter_AddRefs(theDevContext));
	if (theDevContext->IsPrinter())		// we are printing
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
			if (AllocateGWorld(mImagePixmap.packSize, nsnull, srcRect, &tempGWorld) == noErr)
			{
			  // erase it to white
				ClearGWorld(tempGWorld);

				PixMapHandle		tempPixMap = GetGWorldPixMap(tempGWorld);
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
		if (!mMaskBitsHandle)
		{
			::CopyBits((BitMap*)&mImagePixmap, (BitMap*)*destPixels, &srcRect, &dstRect, srcCopy, nsnull);
		}
		else
		{
			if (mAlphaDepth > 1)
				::CopyDeepMask((BitMap*)&mImagePixmap, (BitMap*)&mMaskPixmap, (BitMap*)*destPixels, &srcRect, &maskRect, &dstRect, srcCopy, nsnull);
			else
				::CopyMask((BitMap*)&mImagePixmap, (BitMap*)&mMaskPixmap, (BitMap*)*destPixels, &srcRect, &maskRect, &dstRect);
		}
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
  	
	Handle		thePixelsHandle = (aMaskPixels ? mMaskBitsHandle : mImageBitsHandle);
	::HLock(thePixelsHandle);
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
	OSErr					err = noErr;

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
