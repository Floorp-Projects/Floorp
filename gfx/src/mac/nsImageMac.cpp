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
#include "nsImageMac.h"
#include "nsRenderingContextMac.h"
#include "nsDeviceContextMac.h"
#include "nsCarbonHelpers.h"
#include "nsRegionPool.h"

#include <MacTypes.h>
#include <Quickdraw.h>

#include "nsGfxUtils.h"
#include "imgScaler.h"

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
 *  See documentation in nsImageMac.h
 *  @update 
 */
nsImageMac::nsImageMac()
: mImageGWorld(nsnull)
, mImageBits(nsnull)
, mWidth(0)
, mHeight(0)
, mRowBytes(0)
, mBytesPerPixel(0)   // this value is never initialized; the API that uses it is unused
, mMaskGWorld(nsnull)
, mMaskBits(nsnull)
, mAlphaDepth(0)
, mAlphaRowBytes(0)
, mDecodedX1(PR_INT32_MAX)
, mDecodedY1(PR_INT32_MAX)
, mDecodedX2(0)
, mDecodedY2(0)
{
}

/** ---------------------------------------------------
 *  See documentation in nsImageMac.h
 *  @update 
 */
nsImageMac::~nsImageMac()
{
  if (mImageGWorld)
    ::DisposeGWorld(mImageGWorld);
    
  if (mMaskGWorld)
    ::DisposeGWorld(mMaskGWorld);
  
  if (mImageBits)
    free(mImageBits);

  if (mMaskBits)
    free(mMaskBits);
}

NS_IMPL_ISUPPORTS2(nsImageMac, nsIImage, nsIImageMac)

/** ---------------------------------------------------
 *  See documentation in nsImageMac.h
 *  @update 
 */
PRUint8* 
nsImageMac::GetBits()
{
  NS_ASSERTION(mImageBits, "Getting bits for non-existent image");
  return (PRUint8 *)mImageBits;
}


/** ---------------------------------------------------
 *  See documentation in nsImageMac.h
 *  @update 
 */
PRUint8* 
nsImageMac::GetAlphaBits()
{
  NS_ASSERTION(mMaskBits, "Getting bits for non-existent image mask");
  return (PRUint8 *)mMaskBits;
}


/** ---------------------------------------------------
 *  See documentation in nsImageMac.h
 *  @update 08/03/99 -- cleared out the image pointer - dwc
 */
nsresult 
nsImageMac::Init(PRInt32 aWidth, PRInt32 aHeight, PRInt32 aDepth, nsMaskRequirements aMaskRequirements)
{
  // Assumed: Init only runs once (due to gfxIImageFrame only allowing 1 Init)
  OSErr err = noErr;
  
  mWidth = aWidth;
  mHeight = aHeight;

  err = CreateGWorld(aWidth, aHeight, aDepth, &mImageGWorld, &mImageBits, &mRowBytes);
  if (err != noErr)
  {
    if (err == memFullErr)
      nsMemory::HeapMinimize(PR_FALSE);
    return NS_ERROR_FAILURE;
  }
  
  // this is unused
  //mBytesPerPixel = (mImagePixmap.pixelSize <= 8) ? 1 : mImagePixmap.pixelSize / 8;
        
  if (aMaskRequirements != nsMaskRequirements_kNoMask)
  {
    switch (aMaskRequirements)
    {
      case nsMaskRequirements_kNeeds1Bit:
        mAlphaDepth = 1;
        break;
        
      case nsMaskRequirements_kNeeds8Bit:
        mAlphaDepth = 8;              
        break;

      default:
        break; // avoid compiler warning
    }
    
    err = CreateGWorld(aWidth, aHeight, mAlphaDepth, &mMaskGWorld, &mMaskBits, &mAlphaRowBytes);
    if (err != noErr)
    {
      if (err == memFullErr)
        nsMemory::HeapMinimize(PR_FALSE);
      return NS_ERROR_FAILURE;
    }
  }
  
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsImageMac.h
 *  @update 
 */
void nsImageMac::ImageUpdated(nsIDeviceContext *aContext, PRUint8 aFlags, nsRect *aUpdateRect)
{
  mDecodedX1 = PR_MIN(mDecodedX1, aUpdateRect->x);
  mDecodedY1 = PR_MIN(mDecodedY1, aUpdateRect->y);

  if (aUpdateRect->YMost() > mDecodedY2)
    mDecodedY2 = aUpdateRect->YMost();

  if (aUpdateRect->XMost() > mDecodedX2)
    mDecodedX2 = aUpdateRect->XMost();
}


/** ---------------------------------------------------
 *  See documentation in nsImageMac.h
 *  @update 
 */
NS_IMETHODIMP nsImageMac::Draw(nsIRenderingContext &aContext, nsIDrawingSurface* aSurface, PRInt32 aSX, PRInt32 aSY,
                 PRInt32 aSWidth, PRInt32 aSHeight, PRInt32 aDX, PRInt32 aDY, PRInt32 aDWidth, PRInt32 aDHeight)
{
  Rect                srcRect, dstRect, maskRect;
  nsresult rv = NS_OK;

  if (!mImageGWorld)
    return NS_ERROR_FAILURE;

  if (mDecodedX2 < mDecodedX1 || mDecodedY2 < mDecodedY1)
    return NS_OK;

  PRInt32 srcWidth = aSWidth;
  PRInt32 srcHeight = aSHeight;
  PRInt32 dstWidth = aDWidth;
  PRInt32 dstHeight = aDHeight;

  // Ensure that the source rect is no larger than our decoded rect.
  // If it is, clip the source rect, and scale the difference to adjust
  // the dest rect.

  PRInt32 j = aSX + aSWidth;
  PRInt32 z;
  if (j > mDecodedX2) {
    z = j - mDecodedX2;
    aDWidth -= z*dstWidth/srcWidth;
    aSWidth -= z;
  }
  if (aSX < mDecodedX1) {
    aDX += (mDecodedX1 - aSX)*dstWidth/srcWidth;
    aSX = mDecodedX1;
  }

  j = aSY + aSHeight;
  if (j > mDecodedY2) {
    z = j - mDecodedY2;
    aDHeight -= z*dstHeight/srcHeight;
    aSHeight -= z;
  }
  if (aSY < mDecodedY1) {
    aDY += (mDecodedY1 - aSY)*dstHeight/srcHeight;
    aSY = mDecodedY1;
  }

  if (aDWidth <= 0 || aDHeight <= 0 || aSWidth <= 0 || aSHeight <= 0)
    return NS_OK;

  ::SetRect(&srcRect, aSX, aSY, aSX + aSWidth, aSY + aSHeight);
  maskRect = srcRect;
  ::SetRect(&dstRect, aDX, aDY, aDX + aDWidth, aDY + aDHeight);

  // get the destination pix map
  nsDrawingSurfaceMac* surface = static_cast<nsDrawingSurfaceMac*>(aSurface);
  CGrafPtr    destPort;
  rv = surface->GetGrafPtr(&destPort);
  if (NS_FAILED(rv)) return rv;

  StPortSetter    destSetter(destPort);
  ::ForeColor(blackColor);
  ::BackColor(whiteColor);

  if (RenderingToPrinter(aContext))
  {
    if (!mMaskGWorld)
    {
      ::CopyBits(::GetPortBitMapForCopyBits(mImageGWorld),
                 ::GetPortBitMapForCopyBits(destPort),
                 &srcRect, &dstRect, srcCopy, nsnull);
    }
    else
    {
      // If we are printing, then we need to render everything into a temp
      // GWorld, and then blit that out to the destination.  We do this
      // since Copy{Deep}Mask are not supported for printing.

      GWorldPtr tempGWorld;

      // if we have a mask, blit the transparent image into a new GWorld which is
      // just white, and print that. This is marginally better than printing the
      // image directly, since the transparent pixels come out black.

      PRInt16 pixelDepth = ::GetPixDepth(::GetGWorldPixMap(mImageGWorld));
      if (AllocateGWorld(pixelDepth, nsnull, srcRect, &tempGWorld) == noErr)
      {
        // erase it to white
        ClearGWorld(tempGWorld);

        PixMapHandle    tempPixMap = ::GetGWorldPixMap(tempGWorld);
        if (tempPixMap)
        {
          StPixelLocker   tempPixLocker(tempPixMap);      // locks the pixels

          // Copy everything into tempGWorld
          if (mAlphaDepth > 1)
            ::CopyDeepMask(::GetPortBitMapForCopyBits(mImageGWorld),
                           ::GetPortBitMapForCopyBits(mMaskGWorld),
                           ::GetPortBitMapForCopyBits(tempGWorld),
                           &srcRect, &maskRect, &srcRect, srcCopy, nsnull);
          else
            ::CopyMask(::GetPortBitMapForCopyBits(mImageGWorld),
                       ::GetPortBitMapForCopyBits(mMaskGWorld),
                       ::GetPortBitMapForCopyBits(tempGWorld),
                       &srcRect, &maskRect, &srcRect);

          // now copy tempGWorld bits to destination
          ::CopyBits(::GetPortBitMapForCopyBits(tempGWorld),
                     ::GetPortBitMapForCopyBits(destPort),
                     &srcRect, &dstRect, srcCopy, nsnull);
        }
        
        ::DisposeGWorld(tempGWorld);  // do this after dtor of tempPixLocker!
      }
    }
  }
  else
  {
    // not printing...
    if (mAlphaDepth == 1 && ((aSWidth != aDWidth) || (aSHeight != aDHeight)))
    {
      // If scaling an image that has a 1-bit mask...

      // Bug 195022 - Seems there is a bug in the Copy{Deep}Mask functions
      // where scaling an image that has a 1-bit mask can cause some ugly
      // artifacts to appear on screen.  To work around this issue, we use the
      // functions in imgScaler.cpp to do the actual scaling of the source
      // image and mask.

      GWorldPtr tempSrcGWorld = nsnull, tempMaskGWorld = nsnull;

      // create temporary source GWorld
      char* scaledSrcBits;
      PRInt32 tmpSrcRowBytes;
      PRInt16 pixelDepthSrc = ::GetPixDepth(::GetGWorldPixMap(mImageGWorld));
      OSErr err = CreateGWorld(aDWidth, aDHeight, pixelDepthSrc,
                               &tempSrcGWorld, &scaledSrcBits, &tmpSrcRowBytes);

      if (err != noErr)  return NS_ERROR_FAILURE;

      // create temporary mask GWorld
      char* scaledMaskBits;
      PRInt32 tmpMaskRowBytes;
      err = CreateGWorld(aDWidth, aDHeight, mAlphaDepth, &tempMaskGWorld,
                         &scaledMaskBits, &tmpMaskRowBytes);

      if (err == noErr)
      {
        PixMapHandle srcPixMap = ::GetGWorldPixMap(mImageGWorld);
        PixMapHandle maskPixMap = ::GetGWorldPixMap(mMaskGWorld);
        if (srcPixMap && maskPixMap)
        {
          StPixelLocker srcPixLocker(srcPixMap);      // locks the pixels
          StPixelLocker maskPixLocker(maskPixMap);

          // scale the source
          RectStretch(aSWidth, aSHeight, aDWidth, aDHeight,
                      0, 0, aDWidth - 1, aDHeight - 1,
                      mImageBits, mRowBytes, scaledSrcBits, tmpSrcRowBytes,
                      pixelDepthSrc);

          // scale the mask
          RectStretch(aSWidth, aSHeight, aDWidth, aDHeight,
                      0, 0, aDWidth - 1, aDHeight - 1,
                      mMaskBits, mAlphaRowBytes, scaledMaskBits,
                      tmpMaskRowBytes, mAlphaDepth);

          Rect tmpRect;
          ::SetRect(&tmpRect, 0, 0, aDWidth, aDHeight);

          // copy to screen
          CopyBitsWithMask(::GetPortBitMapForCopyBits(tempSrcGWorld),
                           ::GetPortBitMapForCopyBits(tempMaskGWorld),
                           mAlphaDepth, ::GetPortBitMapForCopyBits(destPort),
                           tmpRect, tmpRect, dstRect, PR_TRUE);
        }

        ::DisposeGWorld(tempMaskGWorld);
        free(scaledMaskBits);
      }
      else
      {
        rv = NS_ERROR_FAILURE;
      }

      ::DisposeGWorld(tempSrcGWorld);
      free(scaledSrcBits);
    }
    else
    {
      // not scaling...
      CopyBitsWithMask(::GetPortBitMapForCopyBits(mImageGWorld),
                       mMaskGWorld ? ::GetPortBitMapForCopyBits(mMaskGWorld) : nsnull,
                       mAlphaDepth, ::GetPortBitMapForCopyBits(destPort),
                       srcRect, maskRect, dstRect, PR_TRUE);
    }
  }

  return rv;
}

/** ---------------------------------------------------
 *  See documentation in nsImageMac.h
 *  @update 
 */
NS_IMETHODIMP nsImageMac::Draw(nsIRenderingContext &aContext, 
                 nsIDrawingSurface* aSurface,
                 PRInt32 aX, PRInt32 aY, 
                 PRInt32 aWidth, PRInt32 aHeight)
{

  return Draw(aContext, aSurface, 0, 0, mWidth, mHeight, aX, aY, aWidth, aHeight);
}
 
/** ---------------------------------------------------
 *  See documentation in nsImageMac.h
 *  @update 
 */
NS_IMETHODIMP nsImageMac::DrawToImage(nsIImage* aDstImage, PRInt32 aDX, PRInt32 aDY, PRInt32 aDWidth, PRInt32 aDHeight)
{
  Rect srcRect, dstRect, maskRect;

  if (!mImageGWorld)
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
  
  GWorldPtr destGWorld;
  dstMacImage->GetGWorldPtr(&destGWorld);
  NS_ASSERTION(destGWorld, "No dest pixels!");

  CopyBitsWithMask(::GetPortBitMapForCopyBits(mImageGWorld),
      mMaskGWorld ? ::GetPortBitMapForCopyBits(mMaskGWorld) : nsnull, mAlphaDepth,
      ::GetPortBitMapForCopyBits(destGWorld), srcRect, maskRect, dstRect, PR_FALSE);
  
  aDstImage->UnlockImagePixels(PR_FALSE);
  aDstImage->UnlockImagePixels(PR_TRUE);
  UnlockImagePixels(PR_FALSE);
  UnlockImagePixels(PR_TRUE);
  
  return NS_OK;
}


/** ---------------------------------------------------
 *  See documentation in nsImageMac.h
 *  @update 
 */
nsresult nsImageMac::Optimize(nsIDeviceContext* aContext)
{
  return NS_OK;
}

#pragma mark -

/** ---------------------------------------------------
 *  Lock down the image pixels
 */
NS_IMETHODIMP
nsImageMac::LockImagePixels(PRBool aMaskPixels)
{
  // nothing to do
  return NS_OK;
}

/** ---------------------------------------------------
 *  Unlock the pixels
 */
NS_IMETHODIMP
nsImageMac::UnlockImagePixels(PRBool aMaskPixels)
{
  // nothing to do
  return NS_OK;
}

#pragma mark -

/*
  Because almost all the images we create are 24-bit images, we can share the same GDevice between
  all of them. So we make a tiny GWorld, and then will use its GDevice for all subsequent
  24-bit GWorlds. This GWorld is never freed.
  
  We only bother caching the device for 24-bit GWorlds. This method returns nsnull
  for other depths.
*/
GDHandle nsImageMac::GetCachedGDeviceForDepth(PRInt32 aDepth)
{
  if (aDepth == 24)
  {
    static GWorldPtr s24BitDeviceGWorld = nsnull;
    
    if (!s24BitDeviceGWorld)
    {
      Rect  bounds = { 0, 0, 16, 16 };
      ::NewGWorld(&s24BitDeviceGWorld, aDepth, &bounds, nsnull, nsnull, 0);
      if (!s24BitDeviceGWorld) return nsnull;
    }
    return ::GetGWorldDevice(s24BitDeviceGWorld);
  }
  
  return nsnull;
}

/** -----------------------------------------------------------------
 *  Create a PixMap, filling in ioPixMap
 */
OSErr nsImageMac::CreateGWorld( PRInt32 aWidth, PRInt32 aHeight, PRInt32 aDepth,
                GWorldPtr* outGWorld, char** outBits, PRInt32* outRowBytes)
{
    return CreateGWorldInternal(aWidth, aHeight, aDepth, outGWorld, outBits, outRowBytes, PR_FALSE);
}


/** -----------------------------------------------------------------
 *  Create a PixMap, filling in ioPixMap
 *  Call the CreatePixMap wrapper instead.
 */
OSErr nsImageMac::CreateGWorldInternal(PRInt32 aWidth, PRInt32 aHeight, PRInt32 aDepth, 
                GWorldPtr* outGWorld, char** outBits, PRInt32* outRowBytes, PRBool aAllow2Bytes)
{
  OSErr   err = noErr;
  
  
  PRInt32 bitsPerPixel = 0;
  CTabHandle  colorTable;
  PRUint32 pixelFormat = GetPixelFormatForDepth(aDepth, bitsPerPixel, &colorTable);

  if (pixelFormat != 0)
  {
    PRInt32   rowBytes = CalculateRowBytesInternal(aWidth, bitsPerPixel, aAllow2Bytes);
    PRInt32   imageSize = rowBytes * aHeight;

    char*     imageBits = (char*)calloc(imageSize, 1);
    if (!imageBits)
      return memFullErr;

    Rect imageRect = {0, 0, 0, 0};
    imageRect.right = aWidth;
    imageRect.bottom = aHeight;
    
    GDHandle    deviceHandle = GetCachedGDeviceForDepth(aDepth);
    GWorldFlags flags = deviceHandle ? noNewDevice : 0;
    
    GWorldPtr imageGWorld;
    err = ::NewGWorldFromPtr(&imageGWorld, pixelFormat, &imageRect, colorTable, deviceHandle, flags, (Ptr)imageBits, rowBytes);
    if (err != noErr)
    {
      NS_ASSERTION(0, "NewGWorldFromPtr failed");
      return err;
    }
    
    *outGWorld = imageGWorld;
    *outBits = imageBits;
    *outRowBytes = rowBytes;
  } 

  return noErr;
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
 *  See <http://developer.apple.com/technotes/qd/qd_15.html>
 */
PRInt32 nsImageMac::CalculateRowBytesInternal(PRUint32 aWidth, PRUint32 aDepth, PRBool aAllow2Bytes)
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
PRInt32 nsImageMac::CalculateRowBytes(PRUint32 aWidth, PRUint32 aDepth)
{
    return CalculateRowBytesInternal(aWidth, aDepth, PR_FALSE);
}


PRUint32 nsImageMac::GetPixelFormatForDepth(PRInt32 inDepth, PRInt32& outBitsPerPixel, CTabHandle* outDefaultColorTable)
{
  PRUint32 pixelFormat = 0;
  PRInt32  bitsPerPixel;
  
  if (outDefaultColorTable)
      *outDefaultColorTable = nsnull;

  // See IM:QuickDraw pp 4-92 for GetCTable params
  switch (inDepth)
  {
    case 1:
      if (outDefaultColorTable)
        *outDefaultColorTable = ::GetCTable(32 + 1);
      bitsPerPixel = 1;
      pixelFormat = k1MonochromePixelFormat;
      break;

    case 2:
      if (outDefaultColorTable)
        *outDefaultColorTable = ::GetCTable(32 + 2);
      bitsPerPixel = 2;
      pixelFormat = k2IndexedPixelFormat;
      break;
      
    case 4:
      if (outDefaultColorTable)
        *outDefaultColorTable = ::GetCTable(32 + 4);
      bitsPerPixel = 4;
      pixelFormat = k4IndexedPixelFormat;
      break;
      
    case 8:
      if (outDefaultColorTable)
        *outDefaultColorTable = ::GetCTable(32 + 8);
      bitsPerPixel = 8;
      pixelFormat = k8IndexedPixelFormat;
      break;
      
    case 16:
      bitsPerPixel = 16;
      pixelFormat = k16BE555PixelFormat;
      break;
      
    case 24:
      // 24-bit images are 8 bits per component; the alpha component is ignored
      bitsPerPixel = 32;
      pixelFormat = k32ARGBPixelFormat;
      break;

    case 32:
      bitsPerPixel = 32;
      pixelFormat = k32ARGBPixelFormat;
      break;
      
    default:
      NS_ASSERTION(0, "Unhandled image depth");
  }
  
  outBitsPerPixel = bitsPerPixel;
  return pixelFormat;
}

#pragma mark -

/** ---------------------------------------------------
 *  Erase the GWorld contents
 */
void nsImageMac::ClearGWorld(GWorldPtr theGWorld)
{
  PixMapHandle  thePixels = ::GetGWorldPixMap(theGWorld);

  StPixelLocker pixelLocker(thePixels);
  StGWorldPortSetter  tilingWorldSetter(theGWorld); 
  
  // White the offscreen
  ::BackColor(whiteColor);

  Rect portRect;
  ::GetPortBounds(theGWorld, &portRect);
  ::EraseRect(&portRect);
}

/** -----------------------------------------------------------------
 *  Allocate a GWorld
 */
OSErr nsImageMac::AllocateGWorld(PRInt16 depth, CTabHandle colorTable,
                                 const Rect& bounds, GWorldPtr *outGWorld)
{
  GWorldPtr newGWorld = NULL;
  // on Mac OS X, there's no reason to use the temp mem flag
  ::NewGWorld(&newGWorld, depth, &bounds, colorTable, nsnull, 0);
  if (!newGWorld)
    return memFullErr;

  *outGWorld = newGWorld;
  return noErr;
}

void nsImageMac::CopyBitsWithMask(const BitMap* srcBits, const BitMap* maskBits,
                                  PRInt16 maskDepth, const BitMap* destBits,
                                  const Rect& srcRect, const Rect& maskRect,
                                  const Rect& destRect, PRBool inDrawingToPort)
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

    ::CopyDeepMask(srcBits, maskBits, destBits, &srcRect, &maskRect,
                   &destRect, ditherCopy, nsnull);

    if (inDrawingToPort)
    {
      ::SetClip(origClipRegion);
    }    
  }
  else
  {
    ::CopyBits(srcBits, destBits, &srcRect, &destRect, ditherCopy, nsnull);
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
nsImageMac::ConvertToPICT(PicHandle* outPicture)
{
  *outPicture = nsnull;

  Rect picFrame  = {0, 0, mHeight, mWidth};
  Rect maskFrame = {0, 0, mHeight, mWidth};
  GWorldPtr tempGWorld;

  PRInt16 pixelDepth = ::GetPixDepth(::GetGWorldPixMap(mImageGWorld));
  // allocate a "normal" GWorld (which owns its own pixels)
  if (AllocateGWorld(pixelDepth, nsnull, picFrame, &tempGWorld) != noErr) 
    return NS_ERROR_FAILURE;

  // erase it to white
  ClearGWorld(tempGWorld);

  PixMapHandle tempPixMap = ::GetGWorldPixMap(tempGWorld);
  if (tempPixMap)
  {
    StPixelLocker tempPixLocker(tempPixMap);      // locks the pixels
  
    // now copy into the picture
    GWorldPtr currPort;
    GDHandle currDev;
    ::GetGWorld(&currPort, &currDev);
    ::SetGWorld(tempGWorld, nsnull);

    ::SetOrigin(0, 0);
    ::ForeColor(blackColor);
    ::BackColor(whiteColor);

    // copy from the destination into our temp GWorld, to get the background
    CopyBitsWithMask(::GetPortBitMapForCopyBits(mImageGWorld),
        mMaskGWorld ? ::GetPortBitMapForCopyBits(mMaskGWorld) : nsnull, mAlphaDepth,
        ::GetPortBitMapForCopyBits(tempGWorld), picFrame, maskFrame, picFrame, PR_FALSE);

    PicHandle thePicture = ::OpenPicture(&picFrame);
    OSErr err = noErr;
    if (thePicture)
    {
      ::CopyBits(::GetPortBitMapForCopyBits(tempGWorld), ::GetPortBitMapForCopyBits(tempGWorld),
                   &picFrame, &picFrame, ditherCopy, nsnull);
    
      ::ClosePicture();
      err = QDError();
    }
    
    ::SetGWorld(currPort, currDev);     // restore to the way things were
    
    if ( err == noErr )       
      *outPicture = thePicture;
  }

  ::DisposeGWorld(tempGWorld);        // do this after dtor of tempPixLocker!
  
  return NS_OK;
} // ConvertToPICT


NS_IMETHODIMP
nsImageMac::ConvertFromPICT(PicHandle inPicture)
{
  return NS_ERROR_FAILURE;
 
} // ConvertFromPICT

NS_IMETHODIMP
nsImageMac::GetGWorldPtr(GWorldPtr* aGWorld)
{
  *aGWorld = mImageGWorld;
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

    NS_ENSURE_ARG_POINTER(aOutIcon);
    NS_ENSURE_ARG_POINTER(aOutIconType);    
    *aOutIcon = nsnull;
    *aOutIconType = nsnull;
    
    if (aIconDepth != 1 && aIconDepth != 2  && aIconDepth != 4 && 
        aIconDepth != 8 && aIconDepth != 24 && aIconDepth != 32)
      return NS_ERROR_INVALID_ARG;
    
    //returns null if there size specified isn't a valid size for an icon
    OSType iconType = MakeIconType(aIconSize, aIconDepth, PR_FALSE);
    if (iconType == nil)
        return NS_ERROR_INVALID_ARG;

    *aOutIconType = iconType;
    
    Rect   srcRect;
    srcRect.top = aSrcRegion.y;
    srcRect.left = aSrcRegion.x;
    srcRect.bottom = aSrcRegion.y + aSrcRegion.height;
    srcRect.right = aSrcRegion.x + aSrcRegion.width;

    Rect  iconRect = { 0, 0, aIconSize, aIconSize};
    return CopyPixMap(srcRect, iconRect, aIconDepth, 
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
    That's how it works, you can't have the mask separately.
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
    
    if (mMaskGWorld)
    {
        //image has an alpha channel, copy into icon mask
    
        //if the image has an 8 bit mask, but the caller asks for a 1 bit
        //mask, or vice versa, it'll simply be converted by CopyPixMap 
        if (aMaskDepth == 8)
        {
            //for 8 bit masks, this is sufficient
            result = CopyPixMap(srcRect, maskRect, aMaskDepth, 
                                        PR_TRUE, &dstHandle, PR_TRUE); 
        }
        else if (aMaskDepth == 1)
        {
            //1 bit masks are tricker, we must create an '#' resource 
            //which inclues both the 1-bit icon and a mask for it (icm#, ics#, ICN# or ich#)
            Handle iconHandle = nsnull, maskHandle = nsnull;
            result = CopyPixMap(srcRect, maskRect, aMaskDepth,
                                        PR_FALSE, &iconHandle, PR_TRUE);
                                    
            if (NS_SUCCEEDED(result)) {
                result = CopyPixMap(srcRect, maskRect, aMaskDepth,
                                            PR_TRUE, &maskHandle, PR_TRUE);                       
                if (NS_SUCCEEDED(result)) {
                    //a '#' resource's data is simply the mask appended to the icon
                    //these icons and masks are small - 128 bytes each
                    result = ConcatBitsHandles(iconHandle, maskHandle, &dstHandle);
                }
            }

            if (iconHandle) ::DisposeHandle(iconHandle);
            if (maskHandle) ::DisposeHandle(maskHandle);    
        }
        else
        {
            NS_ASSERTION(aMaskDepth, "Unregonised icon mask depth");
            result = NS_ERROR_INVALID_ARG;
        }
    }
    else
    {
        //image has no alpha channel, make an entirely black mask with the appropriate depth
        if (aMaskDepth == 8)
        {
            //simply make the mask
            result = MakeOpaqueMask(aMaskSize, aMaskSize, aMaskDepth, &dstHandle);            
        }
        else if (aMaskDepth == 1)
        {
            //make 1 bit icon and mask as above
            Handle iconHandle = nsnull, maskHandle = nsnull;
            result = CopyPixMap(srcRect, maskRect, aMaskDepth, PR_FALSE, &iconHandle, PR_TRUE);
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
        
        }
        else
        {
            NS_ASSERTION(aMaskDepth, "Unregonised icon mask depth");
            result = NS_ERROR_INVALID_ARG;
        }
    }

    if (NS_SUCCEEDED(result)) *aOutMask = dstHandle;
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

/** Call CopyPixMap instead*/
nsresult 
nsImageMac::CopyPixMap(const Rect& aSrcRegion,
                       const Rect& aDestRegion,
                       const PRInt32 aDestDepth,
                       const PRBool aCopyMaskBits,
                       Handle *aDestData,
                       PRBool aAllow2Bytes /* = PR_FALSE */
                      ) 
{
    NS_ENSURE_ARG_POINTER(aDestData);
    *aDestData = nsnull;

    PRInt32     copyMode;
    CTabHandle  destColorTable = nsnull;
    GWorldPtr   srcGWorld = nsnull;

    //are we copying the image data or the mask
    if (aCopyMaskBits)
    {
        if (!mMaskGWorld)
            return NS_ERROR_INVALID_ARG;

        srcGWorld = mMaskGWorld;        
        copyMode = srcCopy;
        if (aDestDepth <= 8)
            destColorTable = GetCTable(32 + aDestDepth);
    }
    else
    {
        if (!mImageGWorld)
            return NS_ERROR_INVALID_ARG;

        srcGWorld = mImageGWorld;        
        copyMode = ditherCopy;
        if (aDestDepth <= 8)
            destColorTable = GetCTable(64 + aDestDepth);
    }

    // create a handle for the bits, and then wrap a GWorld around it
    PRInt32 destRowBytes = CalculateRowBytesInternal(aDestRegion.right - aDestRegion.left, aDestDepth, aAllow2Bytes);
    PRInt32 destSize     = (aDestRegion.bottom - aDestRegion.top) * destRowBytes;
    
    Handle resultData = ::NewHandleClear(destSize);
    if (!resultData) return NS_ERROR_OUT_OF_MEMORY;

    PRInt32  bitsPerPixel;
    PRUint32 pixelFormat = GetPixelFormatForDepth(aDestDepth, bitsPerPixel);

    { // lock scope
        StHandleLocker destBitsLocker(resultData);

        GWorldPtr destGWorld = nsnull;
        OSErr err = ::NewGWorldFromPtr(&destGWorld, pixelFormat, &aDestRegion, destColorTable, nsnull, 0, *resultData, destRowBytes);
        if (err != noErr)
        {
            NS_ASSERTION(0, "NewGWorldFromPtr failed in nsImageMac::CopyPixMap");
            return NS_ERROR_FAILURE;
        }
        
        ::CopyBits( ::GetPortBitMapForCopyBits(srcGWorld),
                    ::GetPortBitMapForCopyBits(destGWorld),
                    &aSrcRegion, &aDestRegion, 
                    copyMode, nsnull);

        // do I need to free the color table explicitly?
        ::DisposeGWorld(destGWorld);
    }
    
    *aDestData = resultData;
    return NS_OK;

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
    NS_ENSURE_ARG_POINTER(aDstData);
    *aDstData = nsnull;

    Handle result = aSrcData1;    

    // clone the first handle
    OSErr err = ::HandToHand(&result);
    if (err != noErr) return NS_ERROR_OUT_OF_MEMORY;
    
    // then append the second
    err = ::HandAndHand(result, aSrcData2);
    if (err != noErr)
    {
      ::DisposeHandle(result);
      return NS_ERROR_OUT_OF_MEMORY;
    }
    
    *aDstData = result;
    return NS_OK;
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
    NS_ENSURE_ARG_POINTER(aMask);
    aMask = nsnull;    

    //mask size =  (width * height * depth)
    PRInt32 size = aHeight * CalculateRowBytesInternal(aWidth, aDepth, PR_TRUE);
    
    Handle resultData = ::NewHandle(size);
    if (!resultData)
        return NS_ERROR_OUT_OF_MEMORY;

    StHandleLocker dstLocker(resultData);
    memset(*resultData, 0xFF, size);
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
                                   nsIDrawingSurface* aSurface,
                                   PRInt32 aSXOffset, PRInt32 aSYOffset,
                                   PRInt32 aPadX, PRInt32 aPadY,
                                   const nsRect &aTileRect)
{
  if (mDecodedX2 < mDecodedX1 || mDecodedY2 < mDecodedY1)
    return NS_OK;

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

  for (PRInt32 y = aY0; y < aY1; y += mHeight + aPadY)
    for (PRInt32 x = aX0; x < aX1; x += mWidth + aPadX)
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
                                   nsIDrawingSurface* aSurface,
                                   PRInt32 aSXOffset, PRInt32 aSYOffset,
                                   const nsRect &aTileRect)
{
  if (!mImageGWorld)
    return NS_ERROR_FAILURE;

  // lock and set up bits handles
  Rect  imageRect;
  imageRect.left = 0;
  imageRect.top = 0;
  imageRect.right = mWidth;
  imageRect.bottom = mHeight;

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
  const PRInt32 kTilingCopyThreshold = 64;
  
  PRInt32 tilingBoundsWidth   = aSXOffset + aTileRect.width;
  PRInt32 tilingBoundsHeight  = aSYOffset + aTileRect.height;

  PRInt32 tiledRows = (tilingBoundsHeight + mHeight - 1) / mHeight;   // round up
  PRInt32 tiledCols = (tilingBoundsWidth + mWidth - 1) / mWidth;      // round up

  PRInt32 numTiles = tiledRows * tiledCols;
  if (numTiles <= kTilingCopyThreshold)
  {
    // the outside bounds of the tiled area
    PRInt32 topY    = aTileRect.y - aSYOffset,
            bottomY = aTileRect.y + aTileRect.height,
            leftX   = aTileRect.x - aSXOffset,
            rightX  = aTileRect.x + aTileRect.width;

    for (PRInt32 y = topY; y < bottomY; y += mHeight)
    {
      for (PRInt32 x = leftX; x < rightX; x += mWidth)
      {
        Rect    imageDestRect = imageRect;
        Rect    imageSrcRect  = imageRect;
        ::OffsetRect(&imageDestRect, x, y);

        if (x > rightX - mWidth) {
          imageDestRect.right = PR_MIN(imageDestRect.right, rightX);
          imageSrcRect.right = imageRect.left + (imageDestRect.right - imageDestRect.left);
        }
        
        if (y > bottomY - mHeight) {
          imageDestRect.bottom = PR_MIN(imageDestRect.bottom, bottomY);
          imageSrcRect.bottom = imageRect.top + (imageDestRect.bottom - imageDestRect.top);
        }
        
        // CopyBits will do the truncation for us at the edges
        CopyBitsWithMask(::GetPortBitMapForCopyBits(mImageGWorld),
            mMaskGWorld ? ::GetPortBitMapForCopyBits(mMaskGWorld) : nsnull, mAlphaDepth,
            ::GetPortBitMapForCopyBits(destPort), imageSrcRect, imageSrcRect, imageDestRect, PR_TRUE);
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

  PRInt16 pixelDepth = ::GetPixDepth(::GetGWorldPixMap(mImageGWorld));

  GWorldPtr   tilingGWorld = nsnull;
  OSErr err = AllocateGWorld(pixelDepth, nsnull, tileRect, &tilingGWorld);
  if (err != noErr) return NS_ERROR_OUT_OF_MEMORY;
  
  GWorldPtr   maskingGWorld = nsnull;
  if (mMaskGWorld)
  {
    err = AllocateGWorld(pixelDepth, nsnull, tileRect, &maskingGWorld);
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

    const BitMap* destBitMap    = ::GetPortBitMapForCopyBits(destPort);

    const BitMap* imageBitmap   = ::GetPortBitMapForCopyBits(mImageGWorld);
    const BitMap* maskBitMap    = mMaskGWorld ? ::GetPortBitMapForCopyBits(mMaskGWorld) : nsnull;

    const BitMap* tilingBitMap  = ::GetPortBitMapForCopyBits(tilingGWorld);
    const BitMap* maskingBitMap = maskingGWorld ? ::GetPortBitMapForCopyBits(maskingGWorld) : nsnull;

    
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
    
    ::CopyBits(imageBitmap, tilingBitMap, &offsetTileSrc, &offsetTileDest, srcCopy, nsnull);
    if (maskBitMap)
      ::CopyBits(maskBitMap, maskingBitMap, &offsetTileSrc, &offsetTileDest, srcCopy, nsnull);

    // top right of offset tile (Z)
    if (aSXOffset > 0)
    {
      offsetTileSrc = tilePartRect;
      offsetTileSrc.right = aSXOffset;
      offsetTileSrc.top   = aSYOffset;
      
      offsetTileDest = tilePartRect;
      offsetTileDest.left   = tilePartRect.right - aSXOffset;
      offsetTileDest.bottom = tilePartRect.bottom - aSYOffset;

      ::CopyBits(imageBitmap, tilingBitMap, &offsetTileSrc, &offsetTileDest, srcCopy, nsnull);
      if (maskBitMap)
        ::CopyBits(maskBitMap, maskingBitMap, &offsetTileSrc, &offsetTileDest, srcCopy, nsnull);    
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

      ::CopyBits(imageBitmap, tilingBitMap, &offsetTileSrc, &offsetTileDest, srcCopy, nsnull);
      if (maskBitMap)
        ::CopyBits(maskBitMap, maskingBitMap, &offsetTileSrc, &offsetTileDest, srcCopy, nsnull);      
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

      ::CopyBits(imageBitmap, tilingBitMap, &offsetTileSrc, &offsetTileDest, srcCopy, nsnull);
      if (maskBitMap)
        ::CopyBits(maskBitMap, maskingBitMap, &offsetTileSrc, &offsetTileDest, srcCopy, nsnull);
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
        
        ::CopyBits(tilingBitMap, tilingBitMap, &srcRect, &dstRect, srcCopy, nsnull);
        if (maskingPixels)
          ::CopyBits(maskingBitMap, maskingBitMap, &srcRect, &dstRect, srcCopy, nsnull);

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
        
        ::CopyBits(tilingBitMap, tilingBitMap, &srcRect, &dstRect, srcCopy, nsnull);
        if (maskingPixels)
          ::CopyBits(maskingBitMap, maskingBitMap, &srcRect, &dstRect, srcCopy, nsnull);
      
        tileHeight *= 2;
      }
      else
        doneHeight = PR_TRUE;
    }
    
    // We could optimize this a little more by making the temp GWorld 1/4 the size of the dest, 
    // and doing 4 final blits directly to the destination.
    
    // finally, copy to the destination
    CopyBitsWithMask(tilingBitMap,
        maskingBitMap ? maskingBitMap : nsnull, mAlphaDepth,
        destBitMap, tileRect, tileRect, tileDestRect, PR_TRUE);

  } // scope for locks

  // clean up  
  ::DisposeGWorld(tilingGWorld);
  if (maskingGWorld)
    ::DisposeGWorld(maskingGWorld);

  return NS_OK;
}


NS_IMETHODIMP nsImageMac::DrawTile(nsIRenderingContext &aContext,
                                   nsIDrawingSurface* aSurface,
                                   PRInt32 aSXOffset, PRInt32 aSYOffset,
                                   PRInt32 aPadX, PRInt32 aPadY,
                                   const nsRect &aTileRect)
{
  nsresult rv = NS_ERROR_FAILURE;
  PRBool padded = (aPadX || aPadY);
  
  if (!RenderingToPrinter(aContext) && !padded)
    rv = DrawTileQuickly(aContext, aSurface, aSXOffset, aSYOffset, aTileRect);

  if (NS_FAILED(rv))
    rv = SlowTile(aContext, aSurface, aSXOffset, aSYOffset, aPadX, aPadY, aTileRect);
    
  return rv;
}
