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

#include "nspr.h"

#define IsFlagSet(a,b) (a & b)

static NS_DEFINE_IID(kIImageIID, NS_IIMAGE_IID);


/** ------------------------------------------------------------
 *	Utility class for saving, locking, and restoring pixel state
 */

class StPixelLocker
{
public:
				
										StPixelLocker(PixMapHandle thePixMap)
										:	mPixMap(thePixMap)
										{
											mPixelState = ::GetPixelsState(mPixMap);
											::LockPixels(mPixMap);
										}
										
										~StPixelLocker()
										{
											::SetPixelsState(mPixMap, mPixelState);
										}

protected:


		PixMapHandle		mPixMap;
		GWorldFlags			mPixelState;

};


/** ---------------------------------------------------
 *	See documentation in nsImageMac.h
 *	@update 
 */
nsImageMac::nsImageMac()
:	mImageGWorld(nsnull)
,	mWidth(0)
,	mHeight(0)
,	mRowBytes(0)
,	mBytesPerPixel(0)
,	mAlphaGWorld(nsnull)
,	mAlphaDepth(0)
,	mAlphaWidth(0)
,	mAlphaHeight(0)
,	mARowBytes(0)
,	mIsTopToBottom(PR_TRUE)
,	mPixelDataSize(0)
{
	NS_INIT_REFCNT();
}

/** ---------------------------------------------------
 *	See documentation in nsImageMac.h
 *	@update 
 */
nsImageMac::~nsImageMac()
{
	if (mImageGWorld)
		::DisposeGWorld(mImageGWorld);
		
	if (mAlphaGWorld)
		::DisposeGWorld(mAlphaGWorld);
		
}

NS_IMPL_ISUPPORTS(nsImageMac, kIImageIID);

/** ---------------------------------------------------
 *	See documentation in nsImageMac.h
 *	@update 
 */
PRUint8* 
nsImageMac::GetBits()
{
	if (!mImageGWorld)
	{
		NS_ASSERTION(0, "Getting bits for non-existent image");
		return nsnull;
	}

	PixMapHandle		thePixMap = ::GetGWorldPixMap(mImageGWorld);
	
	// pixels should be locked here!
#if DEBUG
	GWorldFlags		pixelFlags = GetPixelsState(thePixMap);
	NS_ASSERTION(pixelFlags & pixelsLocked, "Pixels must be locked here");
#endif
	
	Ptr			pixels = ::GetPixBaseAddr(thePixMap);
	NS_ASSERTION(pixels, "Getting bits for image failed");
	return (PRUint8 *)pixels;
}


/** ---------------------------------------------------
 *	See documentation in nsImageMac.h
 *	@update 
 */
PRUint8* 
nsImageMac::GetAlphaBits()
{
	if (!mAlphaGWorld)
	{
		NS_ASSERTION(0, "Getting alpha bits for non-existent mask");
		return nsnull;
	}

	PixMapHandle		thePixMap = GetGWorldPixMap(mAlphaGWorld);
	
	// pixels should be locked here!
#if DEBUG
	GWorldFlags		pixelFlags = GetPixelsState(thePixMap);
	NS_ASSERTION(pixelFlags & pixelsLocked, "Pixels must be locked here");
#endif

	Ptr	pixels = GetPixBaseAddr(thePixMap);
	NS_ASSERTION(pixels, "Getting alpha bits failed");
	return (PRUint8 *)pixels;
}


/** ---------------------------------------------------
 *	See documentation in nsImageMac.h
 *	@update 08/03/99 -- cleared out the image pointer - dwc
 */
nsresult 
nsImageMac::Init(PRInt32 aWidth, PRInt32 aHeight, PRInt32 aDepth, nsMaskRequirements aMaskRequirements)
{

	// have we already been initted?
	if (mImageGWorld)
	{
		NS_ASSERTION(0, "Initting image twice");
		::DisposeGWorld(mImageGWorld);
		mImageGWorld = nsnull;
	}

  mWidth = aWidth;
  mHeight = aHeight;
  
	PRInt16 bufferdepth;

	switch(aDepth)
	{
		case 8:
			//mThePixelmap.pmTable = ::GetCTable(8);
			bufferdepth = 8;
			break;

		case 16:
			bufferdepth = 16;
			break;

		case 24:			// 24 and 32 bit are basically the same
		case 32:
			bufferdepth = 32;
			break;

		default:
			NS_NOTREACHED("Unexpected buffer depth");
			bufferdepth = 32;
			break;
	}


	// allocate the GWorld
	Rect		bounds = {0, 0, 0, 0};
	bounds.right = aWidth;
	bounds.bottom = aHeight;
		
	OSErr		err = AllocateGWorld(bufferdepth, nsnull, bounds, &mImageGWorld);
	if (err != noErr)
	{
		NS_WARNING("GWorld allocation failed");
		return NS_ERROR_OUT_OF_MEMORY;
	}
	
	ClearGWorld(mImageGWorld);
	
	// calculate the pixel data size
	PixMapHandle	thePixMap = ::GetGWorldPixMap(mImageGWorld);

	mRowBytes = (**thePixMap).rowBytes & 0x3FFF;
	mPixelDataSize = mRowBytes * aHeight;
	mBytesPerPixel = (**thePixMap).cmpCount;
		
	if (aMaskRequirements != nsMaskRequirements_kNoMask)
	{
		PRInt16 mAlphaDepth = 0;

		switch (aMaskRequirements)
		{
			case nsMaskRequirements_kNeeds1Bit:
				mAlphaDepth = 1;
				err = AllocateGWorld(mAlphaDepth, nsnull, bounds, &mAlphaGWorld);
				if (err != noErr)
					return NS_ERROR_OUT_OF_MEMORY;
				break;
				
			case nsMaskRequirements_kNeeds8Bit:
				mAlphaDepth = 8;
				
				// make 8-bit grayscale color table
				CTabHandle		grayRamp = nsnull;
				err = MakeGrayscaleColorTable(256, &grayRamp);
				if (err != noErr)
					return NS_ERROR_OUT_OF_MEMORY;				
				
				err = AllocateGWorld(mAlphaDepth, grayRamp, bounds, &mAlphaGWorld);
				if (err != noErr)
					return NS_ERROR_OUT_OF_MEMORY;
					
				::DisposeHandle((Handle)grayRamp);
				break;
				
			default:
				NS_NOTREACHED("Uknown mask depth");
				break;
		}
		
		if (mAlphaGWorld)
		{
			ClearGWorld(mAlphaGWorld);

			// calculate the pixel data size
			PixMapHandle	maskPixMap = GetGWorldPixMap(mAlphaGWorld);

			mARowBytes = (**maskPixMap).rowBytes & 0x3FFF;
		  mAlphaWidth = aWidth;
		  mAlphaHeight = aHeight;
		}		
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
	PixMapHandle	imagePixMap;
	Rect					srcRect, dstRect, maskRect;

	if (!mImageGWorld)
		return NS_ERROR_FAILURE;

	::SetRect(&srcRect, aSX, aSY, aSX + aSWidth, aSY + aSHeight);
	maskRect = srcRect;
	::SetRect(&dstRect, aDX, aDY, aDX + aDWidth, aDY + aDHeight);

	::ForeColor(blackColor);
	::BackColor(whiteColor);
	
	imagePixMap = GetGWorldPixMap(mImageGWorld);
	
	StPixelLocker		pixelLocker(imagePixMap);			// locks the pixels
	
	// get the destination pix map
	nsDrawingSurfaceMac* surface = static_cast<nsDrawingSurfaceMac*>(aSurface);
	CGrafPtr		destPort;
	nsresult		rv = surface->GetGrafPtr((GrafPtr *)&destPort);
	if (NS_FAILED(rv)) return rv;
	
	PixMapHandle		destPixels = GetGWorldPixMap(destPort);
	NS_ASSERTION(destPixels, "No dest pixels!");
	
	// XXX printing???
	
	if(mAlphaGWorld)
	{
		PixMapHandle		maskPixMap = GetGWorldPixMap(mAlphaGWorld);
		StPixelLocker		pixelLocker(maskPixMap);
		
		// 1-bit masks?
		
		::CopyDeepMask((BitMap*)*imagePixMap, (BitMap*)*maskPixMap, (BitMap*)*destPixels, &srcRect, &maskRect, &dstRect, srcCopy, nsnull);
	}
	else
	{
		::CopyBits((BitMap*)*imagePixMap, (BitMap*)*destPixels, &srcRect, &dstRect, ditherCopy, 0L);
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
  if (!mImageGWorld)
    return NS_ERROR_NOT_INITIALIZED;
  
  if (aMaskPixels && !mAlphaGWorld)
  	return NS_ERROR_NOT_INITIALIZED;
  	
	PixMapHandle		thePixMap = ::GetGWorldPixMap(aMaskPixels ? mAlphaGWorld : mImageGWorld);
	::LockPixels(thePixMap);
	return NS_OK;
}

/** ---------------------------------------------------
 *	Unlock the pixels
 */
NS_IMETHODIMP
nsImageMac::UnlockImagePixels(PRBool aMaskPixels)
{
  if (!mImageGWorld)
    return NS_ERROR_NOT_INITIALIZED;
  
  if (aMaskPixels && !mAlphaGWorld)
  	return NS_ERROR_NOT_INITIALIZED;

	PixMapHandle		thePixMap = ::GetGWorldPixMap(aMaskPixels ? mAlphaGWorld : mImageGWorld);
	::UnlockPixels(thePixMap);
	return NS_OK;
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

	// Black the offscreen
	if (LockPixels(thePixels))
	{
		::SetGWorld(theGWorld, nil);

		::BackColor(whiteColor);
		::EraseRect(&theGWorld->portRect);

		::UnlockPixels(thePixels);
	}

	::SetGWorld(curPort, curDev);
}

/** -----------------------------------------------------------------
 *	Allocate a GWorld, trying first in the heap, and then in temp mem.
 */
#define kReserveHeapFreeSpace			(256 * 1024)
#define kReserverHeapContigSpace	(64 * 1024)

OSErr nsImageMac::AllocateGWorld(PRInt16 depth, CTabHandle colorTable, const Rect& bounds, GWorldPtr *outGWorld)
{
	*outGWorld = nsnull;
	
	// Quick and dirty check to make sure there is some memory available.
	// GWorld allocations in temp mem can still fail if the heap is totally
	// full, because some stuff is allocated in the heap
	long	totalSpace, contiguousSpace;
	::PurgeSpace(&totalSpace, &contiguousSpace);		// this does not purge memory!
	
	QDErr		err = noErr;
	
	if (totalSpace > kReserveHeapFreeSpace && contiguousSpace > kReserverHeapContigSpace)
	{
		err = ::NewGWorld(outGWorld, depth, &bounds, colorTable, nsnull, 0);
		if (err == noErr || err != memFullErr) return err;
	}
	
	err = ::NewGWorld(outGWorld, depth, &bounds, colorTable, nsnull, useTempMem);
	return err;
}


/** ----------------------------------------------------------
 *	Make a 256-level grayscale color table, for alpha blending.
 *  Caller must free the color table with ::DisposeHandle((Handle)colorTable)
 *  when done.
 */

OSErr nsImageMac::MakeGrayscaleColorTable(PRInt16 numColors, CTabHandle *outColorTable)
{
	CTabHandle	colorTable = nil;

	colorTable = (CTabHandle)::NewHandleClear (8 * numColors + 8);	/* Allocate memory for the table */
	if (!colorTable) return memFullErr;
						
	(**colorTable).ctSeed = ::GetCTSeed();				// not sure about this one
	(**colorTable).ctFlags = 0;										// not sure about this one
	(**colorTable).ctSize = numColors - 1;					

	RGBColor		tempColor = { 0xFFFF, 0xFFFF, 0xFFFF};			// starts at white
	PRUint16		colorInc = 0xFFFF / numColors;
	
	for (PRInt16 i = 0; i < numColors; i++)
	{
		(**colorTable).ctTable[i].value = i;
				
		(**colorTable).ctTable[i].rgb = tempColor;
		
		tempColor.red -= colorInc;
		tempColor.green -= colorInc;
		tempColor.blue -= colorInc;
	}

	*outColorTable = colorTable;
	return noErr;
}



