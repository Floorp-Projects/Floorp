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
#include "nsRegionPool.h"

#include <MacTypes.h>
#include <Quickdraw.h>

#include "nsGfxUtils.h"

#if 0
#if TARGET_CARBON
// useful region debugging code.
static OSStatus PrintRgnRectProc(UInt16 message, RgnHandle rgn, const Rect *inRect, void *refCon)
{
  UInt32*   rectCount = (UInt32*)refCon;
  
  switch (message)
  {
    case kQDRegionToRectsMsgInit:
      printf("Dumping region 0x%X\n", rgn);
      break;
      
    case kQDRegionToRectsMsgParse:
      printf("Rect %d t,l,r,b: %ld, %ld, %ld, %ld\n", *rectCount, inRect->top, inRect->left, inRect->right, inRect->bottom);
      (*rectCount)++;
      break;
      
    case kQDRegionToRectsMsgTerminate:
      printf("\n");
      break;
  }
  
  return noErr;
}

static void PrintRegionOutline(RgnHandle inRgn)
{
  static RegionToRectsUPP sCountRectProc = nsnull;
  if (!sCountRectProc)
    sCountRectProc = NewRegionToRectsUPP(PrintRgnRectProc);
  
  UInt32    rectCount = 0;  
  ::QDRegionToRects(inRgn, kQDParseRegionFromTopLeft, sCountRectProc, &rectCount);
}
#endif // TARGET_CARBON
#endif

#pragma mark -

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
	NS_INIT_ISUPPORTS();
	
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
  
  mBytesPerPixel = (mImagePixmap.pixelSize <= 8) ? 1 : mImagePixmap.pixelSize / 8;

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

	// get the destination pix map
	nsDrawingSurfaceMac* surface = static_cast<nsDrawingSurfaceMac*>(aSurface);
	CGrafPtr		destPort;
	nsresult		rv = surface->GetGrafPtr(&destPort);
	if (NS_FAILED(rv)) return rv;
	
	StPortSetter    destSetter(destPort);
	::ForeColor(blackColor);
	::BackColor(whiteColor);
	
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
        srcRect, maskRect, dstRect, PR_TRUE);
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

#ifdef MOZ_WIDGET_COCOA
  nsGraphicsUtils::SetPortToKnownGoodPort();
#endif

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
      (BitMap*)(destPixels), srcRect, maskRect, dstRect, PR_FALSE);
  
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
	
  if (aMaskPixels)
    mMaskPixmap.baseAddr = nsnull;
  else
    mImagePixmap.baseAddr = nsnull;
    
	return NS_OK;
}

#pragma mark -

/** -----------------------------------------------------------------
 *  Create a PixMap, filling in ioPixMap
 */
OSErr nsImageMac::CreatePixMap( PRInt32 aWidth, PRInt32 aHeight, 
                PRInt32 aDepth, CTabHandle aColorTable, 
                PixMap& ioPixMap, Handle& ioBitsHandle)
{
    return CreatePixMapInternal(aWidth, aHeight, aDepth, aColorTable, 
                                ioPixMap, ioBitsHandle, PR_FALSE);
}


/** -----------------------------------------------------------------
 *	Create a PixMap, filling in ioPixMap
 *  Call the CreatePixMap wrapper instead.
 */
OSErr nsImageMac::CreatePixMapInternal(	PRInt32 aWidth, PRInt32 aHeight, 
								PRInt32 aDepth, CTabHandle aColorTable, 
								PixMap& ioPixMap, Handle& ioBitsHandle, PRBool aAllow2Bytes)
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
      // default to black & white colortable
      ioPixMap.pmTable = aColorTable ? aColorTable : GetCTable(32 + 1);		
      ioPixMap.pixelSize = 1;
      break;

    case 2:
      ioPixMap.pixelType = 0;
      ioPixMap.cmpCount = 1;
      ioPixMap.cmpSize = 2;
      // Black, 33% gray, 66% gray, white
      ioPixMap.pmTable = aColorTable ? aColorTable : GetCTable(32 + 2);
      bufferdepth = 2;
      break;
      
    case 4:
      ioPixMap.pixelType = 0;
      ioPixMap.cmpCount = 1;
      ioPixMap.cmpSize = 4;
      // Black, 14 shades of gray, white
      ioPixMap.pmTable = aColorTable ? aColorTable : GetCTable(32 + 4);
      bufferdepth = 4;
      break;
      
    case 8:
      ioPixMap.pixelType = 0;
      ioPixMap.cmpCount = 1;
      ioPixMap.cmpSize = 8;
      // default to gray ramp colortable
      ioPixMap.pmTable = aColorTable ? aColorTable : GetCTable(32 + 8);
      ioPixMap.pixelSize = 8;
      break;
      
    case 16:
      ioPixMap.pixelType = RGBDirect;
      ioPixMap.cmpCount = 3;
      ioPixMap.cmpSize = 5;
      ioPixMap.pmTable = nsnull;
      ioPixMap.pixelSize = 16;
      break;
      
    case 24:      // 24 and 32 bit are basically the same
      ioPixMap.pixelType = RGBDirect;
      ioPixMap.cmpCount = 3;
      ioPixMap.cmpSize = 8;
      ioPixMap.pmTable = nsnull;
      ioPixMap.pixelSize = 32;      // ??
      break;

    case 32:
      ioPixMap.pixelType = RGBDirect;
#if TARGET_CARBON
      ioPixMap.cmpCount = 4;
#else
      ioPixMap.cmpCount = 3;
#endif
      ioPixMap.cmpSize = 8;
      ioPixMap.pmTable = nsnull;
      ioPixMap.pixelSize = 32;
      break;
      
    default:
      NS_ASSERTION(0, "Unhandled image depth");
      return paramErr;
  }
  
  if (ioPixMap.cmpCount)
  {
    PRInt32   imageSize;
    PRInt32   rowBytes = CalculateRowBytesInternal(aWidth, ioPixMap.pixelSize, aAllow2Bytes);

    if (rowBytes >= 0x4000)
    {
      NS_ASSERTION(0, "PixMap too big for QuickDraw");
      return paramErr;
    }
      
    imageSize = rowBytes * aHeight;
    
    err = AllocateBitsHandle(imageSize, &ioBitsHandle);
    if (err != noErr)
			return err;
	
	// We can only set this after locking the pixels handle
    ioPixMap.baseAddr = nsnull;     
	// set the high bit to tell CopyBits that this is a PixMap
    ioPixMap.rowBytes = rowBytes | 0x8000;    
    ioPixMap.bounds.top = 0;
    ioPixMap.bounds.left = 0;
    ioPixMap.bounds.bottom = aHeight;
    ioPixMap.bounds.right = aWidth;
    ioPixMap.packType = 0;
    ioPixMap.packSize = 0;
    ioPixMap.hRes = 72 << 16;     // 72 dpi as Fixed
    ioPixMap.vRes = 72 << 16;     // 72 dpi as Fixed
#if TARGET_CARBON
    ioPixMap.pixelFormat = 0;
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
 *  a multiple of 4. ( 32 / 4 == 8). If you pass true for 
 *  aAllow2Bytes then, for small images (aWidth * aDepth =< 24), you 
 *  get a  result which can be a multiple of 2 instead. 
 *  Some parts of the toolbox require this, notably the native icon creation 
 *  code. This is worth experimenting with if you deal with masks of 16x16 icons
 *  which are frequently 1 bit and thus are considered "small" by this function.
 *
 *  CAUTION: MacOS X is extremely intolerant of divisible by 2 widths. You should never
 *  pass a PixMap to the OS for drawing with PixMap.rowBytes set to anything other than
 *  a multiple of 4 on Mac OS X. (CopyBits seems to be OK with divisible by 2 rowbytes, 
 *  at least for the icon code in this class). That's why this function is private and 
 *  wrapped.
 *
 *	See <http://developer.apple.com/technotes/qd/qd_15.html>
 */
PRInt32  nsImageMac::CalculateRowBytesInternal(PRUint32 aWidth, PRUint32 aDepth, PRBool aAllow2Bytes)
{
    PRInt32 rowBits = aWidth * aDepth;
    //if bits per row is 24 or less, may need 3 bytes or less
    return (rowBits > 24 || !aAllow2Bytes) ?
        ((aWidth * aDepth + 31) / 32) * 4 :
        ((aWidth * aDepth + 15) / 16) * 2;
}

/** Protected CalculateRowBytes. Most functions should call this
    Requires rowBytes to be a multiple of 4
    @see CalculateRowBytesInternal
  */
PRInt32  nsImageMac::CalculateRowBytes(PRUint32 aWidth, PRUint32 aDepth)
{
    return CalculateRowBytesInternal(aWidth, aDepth, PR_FALSE);
}

#pragma mark -

/** ---------------------------------------------------
 *	Erase the GWorld contents
 */
void nsImageMac::ClearGWorld(GWorldPtr theGWorld)
{
	PixMapHandle	thePixels;

	thePixels = ::GetGWorldPixMap(theGWorld);

	StPixelLocker	pixelLocker(thePixels);
  StGWorldPortSetter  tilingWorldSetter(theGWorld);	
	
	// White the offscreen
	::BackColor(whiteColor);

  Rect portRect;
  ::GetPortBounds(theGWorld, &portRect);
	::EraseRect(&portRect);
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

void nsImageMac::CopyBitsWithMask(const BitMap* srcBits, const BitMap* maskBits, PRInt16 maskDepth, const BitMap* destBits,
        const Rect& srcRect, const Rect& maskRect, const Rect& destRect, PRBool inDrawingToPort)
{
  if (maskBits)
  {
    StRegionFromPool    origClipRegion;
    
    if (inDrawingToPort)
    {
      // We need to pass in the clip region, even if it doesn't intersect the image, to avoid a bug
      // on Mac OS X that causes bad image drawing (see bug 137295).
      ::GetClip(origClipRegion);
      
      // There is a bug in the OS that causes bad image drawing if the clip region in
      // the destination port is complex (has holes in??), which hits us on pages with iframes.
      // To work around this, temporarily set the clip to the intersection of the clip 
      // and this image (which, most of the time, will be rectangular). See bug 137295.
      
      StRegionFromPool newClip;
      ::RectRgn(newClip, &destRect);
      ::SectRgn(newClip, origClipRegion, newClip);
      ::SetClip(newClip);
    }

    ::CopyDeepMask(srcBits, maskBits, destBits, &srcRect, &maskRect, &destRect, srcCopy, nsnull);

    if (inDrawingToPort)
    {
      ::SetClip(origClipRegion);
    }    
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
          (BitMap*)(*tempPixMap), picFrame, maskFrame, picFrame, PR_FALSE);
      
  		// now copy into the picture
  		GWorldPtr currPort;
  		GDHandle currDev;
      ::GetGWorld(&currPort, &currDev);
      ::SetGWorld(tempGWorld, nsnull);    
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

/** Create a Macintosh native icon from a region of this image.
    After creating an Icon, you probably want to add it to either
    an IconSuite, an IconFamily or perhaps just write it as a resource. 
    See <Icons.h>
    The caller of the function owns the memory allocated for the resulting icon.

    The "type" of the icon is implicit in the size and depth and mask 
    parameters, 
    e.g.
    size 32, depth 1 -> 'ICON' resource
    size 16, depth 4 -> 'ics4' resource
    size 48, depth 32 -> 'ih32' valid only inside an 'icns' resource (IconFamily)
    
    n.b. you cannout create any of the 'XXX#' (icm#, ics#, ICN# or ich#) resources 
    using this method. Use ConvertAlphaToIconMask for this task.

    CopyBits is used to scale and dither, so the bit depth and size of the requested
    icon do not have to match those of this image.

    @param the region of this image to make into an icon. 
    @param aIconDepth the depth of the icon, must be one of 1, 4, 8 or 32
    @param aIconSize the size of the icon. Traditionally 12, 16, 32 or 48
    @param aOutIcon a handle the icon, caller owns the memory
    @param aOutIconType the type code for the icon requested (see MakeIconType)
    @return error an error code -
                                NS_OK if the data was produced, 
                                NS_ERROR_INVALID_ARG if the depth is wrong, 
                                NS_ERROR_FAILURE if a general error occurs.
*/
    
    

NS_IMETHODIMP
nsImageMac::ConvertToIcon(  const nsRect& aSrcRegion, 
                            const PRInt16 aIconDepth, 
                            const PRInt16 aIconSize,
                            Handle* aOutIcon,
                            OSType* aOutIconType) 
{
                            
    Rect            srcRect;
    Rect            iconRect = { 0, 0, aIconSize, aIconSize};

    NS_ENSURE_ARG_POINTER(aOutIcon);
    NS_ENSURE_ARG_POINTER(aOutIconType);    
    *aOutIcon = nsnull;
    *aOutIconType = nsnull;
    
    if (    aIconDepth != 1 && aIconDepth != 2  && aIconDepth != 4 && 
            aIconDepth != 8 && aIconDepth != 24 && aIconDepth != 32) {        
            return NS_ERROR_INVALID_ARG;
    }
    
    //returns null if there size specified isn't a valid size for an icon
    OSType iconType = MakeIconType(aIconSize, aIconDepth, PR_FALSE);
    if (iconType == nil) {
        return NS_ERROR_INVALID_ARG;
    } 
    *aOutIconType = iconType;
    
    srcRect.top = aSrcRegion.y;
    srcRect.left = aSrcRegion.x;
    srcRect.bottom = aSrcRegion.y + aSrcRegion.height;
    srcRect.right = aSrcRegion.x + aSrcRegion.width;

    return CopyPixMapInternal(    srcRect, iconRect, aIconDepth, 
                                  PR_FALSE, aOutIcon, PR_TRUE);     
} // ConvertToIcon

/** Create an Icon mask from a specified region of the the alpha channel 
    in this image.
    The caller owns the memory allocated for the mask.
    
    If the image has no alpha channel, a fully opaque mask of the
    requested size and depth is generated and returned.
    If the image has an alpha channel which is at a different depth
    from the requested mask, the channel is converted.
    
    If an 8 bit masks is requested, one is simply returned. 
    As with icons, the size and determines the exact type, however
    8 bit masks are ONLY valid inside an IconFamily ('icns' resource)
    
    size 16 -> s8mk, size 32 -> l8mk, size 48 -> h8mk. 
    (no mini 8 bit masks exist)
    
    1 bit masks are trickier. These are older and work for both IconFamilies and
    IconSuites. Actually, a 1 bit masks is required for every size that you set
    in an IconFamily or Icon Suite.
    
    'XXX#' resources (icm#, ics#, ICN# or ich#) contain a 1 bit icon of the 
    indicated
    size, followed immediate by a 1 bit mask of the same size. 
    That's how it works, you can't have the mask seperately.
    This mask is used for all icons of that size in a suite or family.
    
    So if you want to use a 256 colour 32x32 icon you will typically call
    CreateToIcon to make an icl8 resource then this function to make an ICN# 
    resource. Then you store the result in an suite or a family. 
    Repeat for other sizes and depths as desired. 
    For more details, see Inside Macintosh: Icon Utilities, Icon Services and 
    <Icons.h>
    
    size 12 -> icm#, size 16 -> ics#, size 32-> ICN#, size 48-> ich#
    (constrast ICON above with ICN#, the later has both the icon and the mask)
    
    @param the region of this image's alpha channel to make into an icon. 
    @param aIconDepth the depth of the icon, must be 1 or 8
    @param aIconSize the size of the icon. Traditionally 12, 16, 32 or 48
                        See above for restictions.
    @param aOutIcon a handle the mask, caller owns the memory
    @param aOutIconType the type code for the icon mask requested (see MakeIconType)
    @return error an error code -
                                NS_OK if the data was produced, 
                                NS_ERROR_INVALID_ARG if the depth is wrong, 
                                NS_ERROR_FAILURE if a general error occurs.
    
    
*/    
NS_IMETHODIMP
nsImageMac::ConvertAlphaToIconMask(  const nsRect& aSrcRegion, 
                                     const PRInt16 aMaskDepth, 
                                     const PRInt16 aMaskSize,
                                     Handle* aOutMask,
                                     OSType* aOutIconType) 
{                            
    Handle          dstHandle = nsnull;
    Rect            srcRect;
    nsresult        result;      
    Rect            maskRect = { 0, 0, aMaskSize, aMaskSize};
    
    srcRect.top = aSrcRegion.y;
    srcRect.left = aSrcRegion.x;
    srcRect.bottom = aSrcRegion.y + aSrcRegion.height;
    srcRect.right = aSrcRegion.x + aSrcRegion.width;
    
    NS_ENSURE_ARG_POINTER(aOutMask);
    NS_ENSURE_ARG_POINTER(aOutIconType);
    *aOutMask = nsnull;
    *aOutIconType = nsnull;
    
    //returns null if there size specified isn't a valid size for an icon
    OSType iconType = MakeIconType(aMaskSize, aMaskDepth, PR_TRUE);
    if (iconType == nil) {
        return NS_ERROR_INVALID_ARG;
    } 
    *aOutIconType = iconType;
    
    if (mMaskBitsHandle) {
        //image has an alpha channel, copy into icon mask
    
        //if the image has an 8 bit mask, but the caller asks for a 1 bit
        //mask, or vice versa, it'll simply be converted by CopyPixMap 
        if (aMaskDepth == 8) {
            //for 8 bit masks, this is sufficient
            result = CopyPixMapInternal(srcRect, maskRect, aMaskDepth, 
                                        PR_TRUE, &dstHandle, PR_TRUE); 
        } else if (aMaskDepth == 1) {
            //1 bit masks are tricker, we must create an '#' resource 
            //which inclues both the 1-bit icon and a mask for it (icm#, ics#, ICN# or ich#)
            Handle iconHandle = nsnull, maskHandle = nsnull;
            result = CopyPixMapInternal(srcRect, maskRect, aMaskDepth,
                                        PR_FALSE, &iconHandle, PR_TRUE);
                                    
            if (NS_SUCCEEDED(result)) {
                result = CopyPixMapInternal(srcRect, maskRect, aMaskDepth,
                                            PR_TRUE, &maskHandle, PR_TRUE);                       
                if (NS_SUCCEEDED(result)) {
                    //a '#' resource's data is simply the mask appended to the icon
                    //these icons and masks are small - 128 bytes each
                    result = ConcatBitsHandles(iconHandle, maskHandle, &dstHandle);
                }
            }

            if (iconHandle) ::DisposeHandle(iconHandle);
            if (maskHandle) ::DisposeHandle(maskHandle);    
        } else {
            NS_ASSERTION(aMaskDepth, "Unregonised icon mask depth");
            result = NS_ERROR_INVALID_ARG;
        }
    } else {
        //image has no alpha channel, make an entirely black mask with the appropriate depth
        if (aMaskDepth == 8) {
            //simply make the mask
            result = MakeOpaqueMask(aMaskSize, aMaskSize, aMaskDepth, &dstHandle);            
        } else if (aMaskDepth == 1) {
            //make 1 bit icon and mask as above
            Handle iconHandle = nsnull, maskHandle = nsnull;
            result = CopyPixMapInternal(    srcRect, maskRect, aMaskDepth, PR_FALSE, &iconHandle, PR_TRUE);
            if (NS_SUCCEEDED(result)) {
                result = MakeOpaqueMask(aMaskSize, aMaskSize, aMaskDepth, &maskHandle);                    
                if (NS_SUCCEEDED(result)) {
                    //a '#' resource's data is simply the mask appended to the icon
                    //these icons and masks are small - 128 bytes each
                    result = ConcatBitsHandles(iconHandle, maskHandle, &dstHandle);
                }
            }

            if (iconHandle) ::DisposeHandle(iconHandle);
            if (maskHandle) ::DisposeHandle(maskHandle); 
        
        } else {
            NS_ASSERTION(aMaskDepth, "Unregonised icon mask depth");
            result = NS_ERROR_INVALID_ARG;
        }
        
        
    }
    if(NS_SUCCEEDED(result)) *aOutMask = dstHandle;
    return result;
} // ConvertAlphaToIconMask

#pragma mark -

/** Create a new PixMap with the specified size and depth,
    then copy either the image bits or the mask bits from this
    image into a handle. CopyBits is used to dither and scale
    the indicated source region from this image into the resulting
    handle. For indexed colour image depths, a standard Mac colour table
    is used. For masks, a standard Mac greyscale table is used.
    
    @param aSrcregion the part of the image to copy
    @param aDestRegion the size of the destination image bits to create
    @param aDestDepth the depth of the destination image bits
    @param aCopyMaskBits if true, the alpha bits are copied, otherwise the
                            image bits are copied. You must check that the
                            image has an alpha channel before calling with this
                            parameter set to true.
    @param aDestData the result bits are copied into this handle, the caller
                            is responsible for disposing of them    
*/
nsresult 
nsImageMac::CopyPixMap(Rect& aSrcRegion,
                       Rect& aDestRegion,
                       const PRInt32 aDestDepth,
                       const PRBool aCopyMaskBits,
                       Handle *aDestData
                      )
{

    return CopyPixMapInternal(aSrcRegion,
                              aDestRegion,
                              aDestDepth,
                              aCopyMaskBits,
                              aDestData,
                              PR_FALSE);
}


/** Call CopyPixMap instead*/
nsresult 
nsImageMac::CopyPixMapInternal(Rect& aSrcRegion,
                               Rect& aDestRegion,
                               const PRInt32 aDestDepth,
                               const PRBool aCopyMaskBits,
                               Handle *aDestData,
                               PRBool aAllow2Bytes
                              ) 
{
    OSStatus err;
    PRBool pixelsNeedLocking = PR_FALSE;
    PixMap destPixmap;
    PixMapPtr srcPixmap;
    Handle *srcData;
    Handle resultData = nsnull;
    PRInt32 copyMode;
    CTabHandle destColorTable = nsnull;
    
    NS_ENSURE_ARG_POINTER(aDestData);
    *aDestData = nsnull;
    
    //are we copying the image data or the mask
    if (aCopyMaskBits) {
        if (!mMaskBitsHandle) {
            return NS_ERROR_INVALID_ARG;
        } else {
            srcPixmap = &mMaskPixmap;        
            srcData = &mMaskBitsHandle;
            copyMode = srcCopy;
            if (aDestDepth <= 8)
                destColorTable = GetCTable(32 + aDestDepth);
        }        
    } else {
        if (!mImageBitsHandle) {
            return NS_ERROR_INVALID_ARG;
        } else {
            srcPixmap = &mImagePixmap;        
            srcData = &mImageBitsHandle;
            copyMode = srcCopy | ditherCopy;
            if (aDestDepth <= 8)
                destColorTable = GetCTable(64 + aDestDepth);
        }
    }

    // allocate the PixMap structure, then fill it out
    err = CreatePixMapInternal( aDestRegion.right - aDestRegion.left,
                                aDestRegion.bottom - aDestRegion.top,
                                aDestDepth, 
                                destColorTable, 
                                destPixmap, 
                                resultData,
                                aAllow2Bytes);   
    if(err) {
        return NS_ERROR_FAILURE;
    }
    
    // otherwise lock and set up bits handles
    StHandleLocker  srcBitsLocker(*srcData);
    StHandleLocker  destBitsLocker(resultData); 
    
    srcPixmap->baseAddr = **srcData;
    
    //CreatePixMap does not set the baseAdr as the bits handle must 
    //be locked first, but it does initialise dstHandle for us so we 
    //set up baseAddr to use aDestHandle
    destPixmap.baseAddr = *resultData;

    //no need to lock either set of pixels as they're not in offscreen GWorlds.

    ::CopyBits( (BitMapPtr) srcPixmap,  
                (BitMapPtr) &destPixmap,
                &aSrcRegion, &aDestRegion, 
                copyMode, nsnull);
    err = QDError();
    NS_ASSERTION(err == noErr, "Error from QuickDraw copying PixMap Data");

    //Clean up PixMap
    if (destPixmap.pmTable) ::DisposeCTable(destPixmap.pmTable);
    
    if (err == noErr) {
      *aDestData = resultData;
      return NS_OK;
    } else {
      //deallocate handle data
      if (resultData) ::DisposeHandle(resultData);
      return NS_ERROR_FAILURE;
    }
} //CopyPixMap

/** Concantenate the data supplied in the given handles,
    the caller is responsible for disposing the result.
    This uses AllocateBitsHandle to allocate the new handle, to take
    advantage of spillover allocation into TempMemory
    @param aSrcData1 first piece of source data
    @param aSrcData2 second piece of src data
    @param astData on exit, contains a copy of aSrcData1 followed by
        a copy of aSrcData2
    @return nsresult an error code
*/
nsresult 
nsImageMac::ConcatBitsHandles( Handle aSrcData1, 
                               Handle aSrcData2,
                               Handle *aDstData)
{
    PRInt32 src1Size = ::GetHandleSize(aSrcData1);
    PRInt32 src2Size = ::GetHandleSize(aSrcData2);
    Handle result = nsnull;
    OSStatus err;
    
    NS_ENSURE_ARG_POINTER(aDstData);
    *aDstData = nsnull;

    err = AllocateBitsHandle(src1Size + src2Size, &result);
    if (err) {
        NS_ASSERTION(err == noErr, "Problem allocating handle for bits");
        return NS_ERROR_OUT_OF_MEMORY;
    } else {
        StHandleLocker dstLocker(*aDstData);                    
        ::BlockMoveData(*aSrcData1, *result, src1Size);
        ::BlockMoveData(*aSrcData2, *result + src1Size, src2Size);
        err = MemError();
        NS_ASSERTION(err == noErr, "Mem error copying icon mask data");
        if (err==noErr) {
            *aDstData = result; 
            return NS_OK;
        } else {
            return NS_ERROR_FAILURE;
        }
    }        
} // ConcatBitsHandles

/** Make a completely opaque mask for an Icon of the specified size and depth.
    @param aWidth the width of the desired mask
    @param aHeight the height of the desired mask
    @param aDepth the bit depth of the desired mask
    @param aMask the mask
    @return nsresult an error code
*/
nsresult 
nsImageMac::MakeOpaqueMask(    const PRInt32 aWidth,
                               const PRInt32 aHeight,
                               const PRInt32 aDepth,
                               Handle *aMask)
{
    //mask size =  (width * height * depth)
    PRInt32 size = aHeight * CalculateRowBytesInternal(aWidth, aDepth, PR_TRUE);
    OSStatus err;
    Handle resultData = nsnull;

    NS_ENSURE_ARG_POINTER(aMask);
    aMask = nsnull;    
    
    err = AllocateBitsHandle(size, &resultData);
    if (err != noErr)
        return NS_ERROR_OUT_OF_MEMORY;

    StHandleLocker dstLocker(resultData);
    ::memset(*resultData, 0xFF, size);
    *aMask = resultData;
    return NS_OK;
} // MakeOpaqueMask


/** Make icon types (OSTypes) from depth and size arguments
    Valid depths for icons: 1, 4, 8 and then 16, 24 or 32 are treat as 32. 
    Valid masks for icons: 1 and 8.
    Valid sizes for icons: 
      16x12 - mini  (pretty obsolete)
      16x16 - small 
      32x32 - large
      48x48 - huge
      128x128 - thumbnail
    
    Exact mapping table (see note above about 1 bit masks being generally inseperable from 1 bit icons)
    
    Icon
        depth height  width   type
        1     32      32      ICON    (one bit icon without mask)
        4     12      16      icm4      
        4     16      16      ics4
        4     32      32      icl4
        4     48      48      ich4
        8     12      16      icm8
        8     16      16      ics8  
        8     32      32      icl8
        8     48      48      ich8
        32    16      16      is32
        32    32      32      il32
        32    48      48      ih32
        32    128     128     it32
    Mask
        1     16      12      icm#
        1     16      16      ics#
        1     32      32      ICN#    (one bit icon and mask - you probably want one of these, not an 'ICON')
        1     48      48      ich#
        8     16      16      s8mk
        8     32      32      l8mk
        8     48      48      h8mk
        8     16      12      t8mk
    
    16 and 24 bit depths will be promoted to 32 bit.
    Any other combination not in the above table gives nil
          
    @param aHeight the height of the icon or mask    
    @param aDepth the depth of the icon or mask 
    @param aMask pass true for masks, false for icons
    @return the correct OSType as defined above
  */      
OSType
nsImageMac::MakeIconType(PRInt32 aHeight, PRInt32 aDepth, PRBool aMask) 
{
    switch(aHeight) {
      case 12:
        switch(aDepth) {
          case 1:
            return 'icm#';
          case 4:
            return 'icm4';
          case 8:
            return 'icm8';
          default:
            return nil;          
        }
      case 16:
        switch(aDepth) {
          case 1:
            return 'ics#';
          case 4:
            return 'ics4';
          case 8:
            if(aMask)
              return 's8mk';
            else
              return 'ics8';
          case 16:
          case 24:
          case 32:
            return 'is32';
          default:
            return nil;           
        }
      case 32:
        switch(aDepth) {
          case 1:
            if(aMask)
              return 'ICN#';
            else
              return 'ICON';
          case 4:
            return 'icl4';
          case 8:
            if(aMask)
              return 'l8mk';
            else
              return 'icl8';
          case 16:
          case 24:
          case 32:
            return 'il32'; 
          default:
            return nil;           
        }
      case 48:
        switch(aDepth) {
          case 1:
            return 'ich#';
          case 4:
            return 'ich4';
          case 8:
            if(aMask)
              return 'h8mk';
            else
              return 'ich8';
          case 16:
          case 24:
          case 32:
            return 'ih32';            
          default:
            return nil;
        }
      case 128:
        if(aMask)
          return 't8mk';
        else
          switch (aDepth) {
            case 16:
            case 24:
            case 32:
              return 'it32';
            default:
              return nil;
          }                  
      default:
        return nil;
    } //switch(aHeight)
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
	if (!mImageBitsHandle)
		return NS_ERROR_FAILURE;

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

  // this code assumes that, if we have a mask, it's the same size as the image
  NS_ASSERTION((mMaskBitsHandle == nsnull) || (mAlphaWidth == mWidth && mAlphaHeight == mHeight), "Mask should be same dimensions as image");
  
  // get the destination pix map
  nsDrawingSurfaceMac* destSurface = static_cast<nsDrawingSurfaceMac*>(aSurface);
  
  CGrafPtr    destPort;
  nsresult    rv = destSurface->GetGrafPtr(&destPort);
  if (NS_FAILED(rv)) return rv;
  
  StPortSetter    destSetter(destPort);
  ::ForeColor(blackColor);
  ::BackColor(whiteColor);
  
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
	  		Rect    imageSrcRect  = imageRect;
	  		::OffsetRect(&imageDestRect, x, y);

	  		if (x == aX1 - mWidth) {
	  		  imageDestRect.right = PR_MIN(imageDestRect.right, aX1);
	  		  imageSrcRect.right = imageRect.left + (imageDestRect.right - imageDestRect.left);
	  		}
        
	  		if (y == aY1 - mHeight) {
	  		  imageDestRect.bottom = PR_MIN(imageDestRect.bottom, aY1);
	  		  imageSrcRect.bottom = imageRect.top + (imageDestRect.bottom - imageDestRect.top);
	  		}
	  		
	  		// CopyBits will do the truncation for us at the edges
        CopyBitsWithMask((BitMap*)(&mImagePixmap),
            mMaskBitsHandle ? (BitMap*)(&mMaskPixmap) : nsnull, mAlphaDepth,
            (BitMap*)(*destPixels), imageSrcRect, imageSrcRect, imageDestRect, PR_TRUE);
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
        (BitMap*)(*destPixels), tileRect, tileRect, tileDestRect, PR_TRUE);
    
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
