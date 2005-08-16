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
#include "nsRegionPool.h"
#include "prmem.h"

// Number of bits for each component in a pixel.
#define BITS_PER_COMPONENT  8
// Number of components per pixel (i.e. as in ARGB).
#define COMPS_PER_PIXEL     4
// Number of bits in a pixel.
#define BITS_PER_PIXEL      BITS_PER_COMPONENT * COMPS_PER_PIXEL

// MacOSX 10.2 supports CGPattern, which does image/pattern tiling for us, and
// is much faster than doing it ourselves.
#define USE_CGPATTERN_TILING

#if 0

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
#endif

#pragma mark -

/**********************************************************
    nsImageMac
 **********************************************************/
nsImageMac::nsImageMac()
: mImageBits(nsnull)
, mImage(nsnull)
, mWidth(0)
, mHeight(0)
, mRowBytes(0)
, mBytesPerPixel(0)   // this value is never initialized; the API that uses it is unused
, mAlphaBits(nsnull)
, mAlphaRowBytes(0)
, mAlphaDepth(0)
, mPendingUpdate(PR_FALSE)
, mOptimized(PR_FALSE)
, mDecodedX1(PR_INT32_MAX)
, mDecodedY1(PR_INT32_MAX)
, mDecodedX2(0)
, mDecodedY2(0)
{
}


nsImageMac::~nsImageMac()
{
  if (mImage)
    ::CGImageRelease(mImage);

  if (mImageBits)
    PR_Free(mImageBits);

  if (mAlphaBits)
    PR_Free(mAlphaBits);
}


NS_IMPL_ISUPPORTS2(nsImageMac, nsIImage, nsIImageMac)


nsresult
nsImageMac::Init(PRInt32 aWidth, PRInt32 aHeight, PRInt32 aDepth,
                 nsMaskRequirements aMaskRequirements)
{
  // Assumed: Init only runs once (due to gfxIImageFrame only allowing 1 Init)

  mWidth = aWidth;
  mHeight = aHeight;

  switch (aMaskRequirements)
  {
    case nsMaskRequirements_kNeeds1Bit:
      mAlphaDepth = 1;
      break;

    case nsMaskRequirements_kNeeds8Bit:
      mAlphaDepth = 8;
      break;

    case nsMaskRequirements_kNoMask:
    default:
      break; // avoid compiler warning
  }

  // create the memory for the image
  // 24-bit images are 8 bits per component; the alpha component is ignored
  mRowBytes = CalculateRowBytes(aWidth, aDepth == 24 ? 32 : aDepth);
  mImageBits = (PRUint8*) PR_Malloc(mHeight * mRowBytes * sizeof(PRUint8));
  if (!mImageBits)
    return NS_ERROR_OUT_OF_MEMORY;

  if (mAlphaDepth)
  {
    mAlphaRowBytes = CalculateRowBytes(aWidth, mAlphaDepth);
    mAlphaBits = (PRUint8*) PR_Malloc(mHeight * mAlphaRowBytes * sizeof(PRUint8));
    if (!mAlphaBits) {
      PR_Free(mImageBits);
      mImageBits = nsnull;
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  return NS_OK;
}


// The image bits (mImageBits & mAlphaBits) have been updated, so set the
// mPendingUpdate flag to force the recreation of CGImageRef later.  This is a
// lazy update, as ImageUpdated() can be called several times in a row, but
// we only recreate the CGImageRef when needed.
void
nsImageMac::ImageUpdated(nsIDeviceContext *aContext, PRUint8 aFlags,
                         nsRect *aUpdateRect)
{
  mPendingUpdate = PR_TRUE;

  mDecodedX1 = PR_MIN(mDecodedX1, aUpdateRect->x);
  mDecodedY1 = PR_MIN(mDecodedY1, aUpdateRect->y);

  if (aUpdateRect->YMost() > mDecodedY2)
    mDecodedY2 = aUpdateRect->YMost();

  if (aUpdateRect->XMost() > mDecodedX2)
    mDecodedX2 = aUpdateRect->XMost();
}

/** ---------------------------------------------------
 *  See documentation in nsIImage.h
 */
PRBool nsImageMac::GetIsImageComplete() {
  return mDecodedX1 == 0 &&
         mDecodedY1 == 0 &&
         mDecodedX2 == mWidth &&
         mDecodedY2 == mHeight;
}

void DataProviderReleaseFunc(void *info, const void *data, size_t size)
{
  PR_Free(NS_CONST_CAST(void*, data));
}


// Create CGImageRef from image bits. Merge alpha bit into mImageBits, which
// contains "place holder" for the alpha information.  Then, create the
// CGImageRef for use in drawing.
nsresult
nsImageMac::EnsureCachedImage()
{
  // Only create cached image if mPendingUpdate is set.
  if (!mPendingUpdate)
    return NS_OK;

  if (mImage) {
    ::CGImageRelease(mImage);
    mImage = NULL;
  }

  PRUint8* imageData = NULL;  // data from which to create CGImage
  CGImageAlphaInfo alphaInfo; // alpha info for CGImage
  void (*releaseFunc)(void *info, const void *data, size_t size) = NULL;

  switch (mAlphaDepth)
  {
    case 8:
    {
      // For 8-bit alpha, we create our own storage, since we premultiply the
      // alpha info into the image bits, but we still want to keep the original
      // image bits (mImageBits).
      imageData = (PRUint8*) PR_Malloc(mWidth * mHeight * COMPS_PER_PIXEL);
      if (!imageData)
        return NS_ERROR_OUT_OF_MEMORY;
      PRUint8* tmp = imageData;

      for (PRInt32 y = 0; y < mHeight; y++)
      {
        PRInt32 rowStart = mRowBytes * y;
        PRInt32 alphaRowStart = mAlphaRowBytes * y;
        for (PRInt32 x = 0; x < mWidth; x++)
        {
          // Here we combine the alpha information with each pixel component,
          // creating an image with 'premultiplied alpha'.
          PRUint8 alpha = mAlphaBits[alphaRowStart + x];
          *tmp++ = alpha;
          PRUint32 offset = rowStart + COMPS_PER_PIXEL * x;
          FAST_DIVIDE_BY_255(*tmp++, mImageBits[offset + 1] * alpha);
          FAST_DIVIDE_BY_255(*tmp++, mImageBits[offset + 2] * alpha);
          FAST_DIVIDE_BY_255(*tmp++, mImageBits[offset + 3] * alpha);
        }
      }

      // The memory that we pass to the CGDataProvider needs to stick around as
      // long as the provider does, and the provider doesn't get destroyed until
      // the CGImage is destroyed.  So we have this small function which is
      // called when the provider is destroyed.
      releaseFunc = DataProviderReleaseFunc;
      alphaInfo = kCGImageAlphaPremultipliedFirst;
      break;
    }

    case 1:
    {
      for (PRInt32 y = 0; y < mHeight; y++)
      {
        PRInt32 rowStart = mRowBytes * y;
        PRUint8* alphaRow = mAlphaBits + mAlphaRowBytes * y;
        for (PRInt32 x = 0; x < mWidth; x++)
        {
          // Copy the alpha information into the place holder in the image data.
          mImageBits[rowStart + COMPS_PER_PIXEL * x] =
                                            GetAlphaBit(alphaRow, x) ? 255 : 0;
        }
      }

      alphaInfo = kCGImageAlphaPremultipliedFirst;
      imageData = mImageBits;
      break;
    }

    case 0:
    default:
    {
      alphaInfo = kCGImageAlphaNoneSkipFirst;
      imageData = mImageBits;
      break;
    }
  }

  CGColorSpaceRef cs = ::CGColorSpaceCreateDeviceRGB();
  StColorSpaceReleaser csReleaser(cs);
  CGDataProviderRef prov = ::CGDataProviderCreateWithData(NULL, imageData,
                                                          mRowBytes * mHeight,
                                                          releaseFunc);
  mImage = ::CGImageCreate(mWidth, mHeight, BITS_PER_COMPONENT, BITS_PER_PIXEL,
                           mRowBytes, cs, alphaInfo, prov, NULL, TRUE,
                           kCGRenderingIntentDefault);
  ::CGDataProviderRelease(prov);

  mPendingUpdate = PR_FALSE;
  return NS_OK;
}


NS_IMETHODIMP
nsImageMac::Draw(nsIRenderingContext &aContext, nsIDrawingSurface* aSurface,
                 PRInt32 aSX, PRInt32 aSY, PRInt32 aSWidth, PRInt32 aSHeight,
                 PRInt32 aDX, PRInt32 aDY, PRInt32 aDWidth, PRInt32 aDHeight)
{
  if (mDecodedX2 < mDecodedX1 || mDecodedY2 < mDecodedY1)
    return NS_OK;

  nsresult rv = EnsureCachedImage();
  NS_ENSURE_SUCCESS(rv, rv);

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

  nsDrawingSurfaceMac* surface = static_cast<nsDrawingSurfaceMac*>(aSurface);
  CGContextRef context = surface->StartQuartzDrawing();

  CGRect srcRect = ::CGRectMake(aSX, aSY, aSWidth, aSHeight);
  CGRect destRect = ::CGRectMake(aDX, aDY, aDWidth, aDHeight);

  CGRect drawRect = ::CGRectMake(0, 0, mWidth, mHeight);
  if (!::CGRectEqualToRect(srcRect, destRect))
  {
    // If drawing a portion of the image, change the drawRect accordingly.
    float sx = ::CGRectGetWidth(destRect) / ::CGRectGetWidth(srcRect);
    float sy = ::CGRectGetHeight(destRect) / ::CGRectGetHeight(srcRect);
    float dx = ::CGRectGetMinX(destRect) - (::CGRectGetMinX(srcRect) * sx);
    float dy = ::CGRectGetMinY(destRect) - (::CGRectGetMinY(srcRect) * sy);
    drawRect = ::CGRectMake(dx, dy, mWidth * sx, mHeight * sy);
  }

  ::CGContextClipToRect(context, destRect);
  ::CGContextDrawImage(context, drawRect, mImage);
  surface->EndQuartzDrawing(context);

  return NS_OK;
}


NS_IMETHODIMP
nsImageMac::Draw(nsIRenderingContext &aContext, 
                 nsIDrawingSurface* aSurface,
                 PRInt32 aX, PRInt32 aY, 
                 PRInt32 aWidth, PRInt32 aHeight)
{

  return Draw(aContext, aSurface, 0, 0, mWidth, mHeight, aX, aY, aWidth, aHeight);
}


NS_IMETHODIMP
nsImageMac::DrawToImage(nsIImage* aDstImage, PRInt32 aDX, PRInt32 aDY,
                        PRInt32 aDWidth, PRInt32 aDHeight)
{
  nsImageMac* dest = NS_STATIC_CAST(nsImageMac*, aDstImage);

  nsresult rv = EnsureCachedImage();
  NS_ENSURE_SUCCESS(rv, rv);
  rv = dest->EnsureCachedImage();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = NS_ERROR_FAILURE;

  // Create image storage.  The storage is 'owned' by the CGImageRef created
  // below as 'newImageRef'; that is, the storage cannot be deleted until the
  // CGImageRef is released.
  PRInt32 width = dest->GetWidth();
  PRInt32 height = dest->GetHeight();
  PRInt32 bytesPerRow = dest->GetLineStride();
  PRInt32 totalBytes = height * bytesPerRow;
  PRUint8* bitmap = (PRUint8*) PR_Malloc(totalBytes);
  if (!bitmap)
    return NS_ERROR_OUT_OF_MEMORY;

  CGColorSpaceRef cs = ::CGColorSpaceCreateDeviceRGB();
  StColorSpaceReleaser csReleaser(cs);
  CGContextRef bitmapContext =
                  ::CGBitmapContextCreate(bitmap, width, height,
                                          BITS_PER_COMPONENT, bytesPerRow,
                                          cs, kCGImageAlphaPremultipliedFirst);

  if (bitmapContext)
  {
    CGRect destRect = ::CGRectMake(0, 0, width, height);
    CGRect drawRect = ::CGRectMake(aDX, aDY, aDWidth, aDHeight);

    // clear the bitmap context
    ::CGContextClearRect(bitmapContext, destRect);

    // draw destination and then this image into bitmap
    ::CGContextDrawImage(bitmapContext, destRect, dest->mImage);
    ::CGContextDrawImage(bitmapContext, drawRect, mImage);

    ::CGContextRelease(bitmapContext);

    CGImageAlphaInfo alphaInfo = ::CGImageGetAlphaInfo(dest->mImage);

    // create a new image from the combined bitmap
    CGDataProviderRef prov = ::CGDataProviderCreateWithData(NULL, bitmap,
                                                            totalBytes, NULL);
    CGImageRef newImageRef = ::CGImageCreate(width, height, BITS_PER_COMPONENT,
                                             BITS_PER_PIXEL, bytesPerRow, cs,
                                             alphaInfo, prov, NULL, TRUE,
                                             kCGRenderingIntentDefault);
    if (newImageRef)
    {
      // set new image in destination
      dest->AdoptImage(newImageRef, bitmap);
      ::CGImageRelease(newImageRef);
      rv = NS_OK;
    }

    ::CGDataProviderRelease(prov);
  }

  return rv;
}

void
nsImageMac::AdoptImage(CGImageRef aNewImage, PRUint8* aNewBitmap)
{
  NS_PRECONDITION(aNewImage != nsnull && aNewBitmap != nsnull,
                  "null ptr");
  if (!aNewImage || !aNewBitmap)
    return;

  // Free exising image and image bits.
  ::CGImageRelease(mImage);
  if (mImageBits)
    PR_Free(mImageBits);

  // Adopt given image and image bits.
  mImage = aNewImage;
  ::CGImageRetain(mImage);
  mImageBits = aNewBitmap;
}


#pragma mark -

// Frees up memory from any unneeded structures.
nsresult
nsImageMac::Optimize(nsIDeviceContext* aContext)
{
  nsresult rv = EnsureCachedImage();
  NS_ENSURE_SUCCESS(rv, rv);

  // We cannot delete the bitmap data from which mImage was created;  the data
  // needs to stick around at least as long as the mImage is valid.
  // mImageBits is used by all images, except those that have mAlphaDepth == 8
  // (for which we created a separate data store in EnsureCachedImage).
  if (mImageBits && mAlphaDepth == 8) {
    PR_Free(mImageBits);
    mImageBits = NULL;
  }

  // mAlphaBits is not really used any more after EnsureCachedImage is called,
  // since it's data is merged into mImageBits.
  if (mAlphaBits) {
    PR_Free(mAlphaBits);
    mAlphaBits = NULL;
  }

  mOptimized = PR_TRUE;

  return NS_OK;
}


NS_IMETHODIMP
nsImageMac::LockImagePixels(PRBool aMaskPixels)
{
  if (!mOptimized)
    return NS_OK;

  // Need to recreate mAlphaBits and/or mImageBits.  We do so by drawing the
  // CGImage into a bitmap context.  Afterwards, 'imageBits' contains the
  // image data with premultiplied alpha (if applicable).
  PRUint8* imageBits = (PRUint8*) PR_Malloc(mHeight * mRowBytes);
  if (!imageBits)
    return NS_ERROR_OUT_OF_MEMORY;
  CGColorSpaceRef cs = ::CGColorSpaceCreateDeviceRGB();
  StColorSpaceReleaser csReleaser(cs);
  CGContextRef bitmapContext =
      ::CGBitmapContextCreate(imageBits, mWidth, mHeight, BITS_PER_COMPONENT,
                              mRowBytes, cs, kCGImageAlphaPremultipliedFirst);
  if (!bitmapContext) {
    PR_Free(imageBits);
    return NS_ERROR_FAILURE;
  }

  // clear the bitmap context & draw mImage into it
  CGRect drawRect = ::CGRectMake(0, 0, mWidth, mHeight);
  ::CGContextClearRect(bitmapContext, drawRect);
  ::CGContextDrawImage(bitmapContext, drawRect, mImage);
  ::CGContextRelease(bitmapContext);

  // 'imageBits' now contains the image and (possibly) alpha bits for image.
  // Now we need to separate them out.

  if (mAlphaDepth) {
    // Only need to worry about alpha for mAlphaDepth == 1 or 8.
    mAlphaBits = (PRUint8*) PR_Malloc(mHeight * mAlphaRowBytes *
                                      sizeof(PRUint8));
    if (!mAlphaBits) {
      PR_Free(imageBits);
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  switch (mAlphaDepth)
  {
    case 8:
    {
      // Need to recreate mImageBits for mAlphaDepth == 8 only.
      // See comments nsImageMac::Optimize().
      PRUint8* tmp = imageBits;
      mImageBits = (PRUint8*) PR_Malloc(mHeight * mRowBytes * sizeof(PRUint8));
      if (!mImageBits) {
        PR_Free(mAlphaBits);
        mAlphaBits = nsnull;
        PR_Free(imageBits);
        return NS_ERROR_OUT_OF_MEMORY;
      }

      // split the alpha bits and image bits into their own structure
      for (PRInt32 y = 0; y < mHeight; y++)
      {
        PRInt32 rowStart = mRowBytes * y;
        PRInt32 alphaRowStart = mAlphaRowBytes * y;
        for (PRInt32 x = 0; x < mWidth; x++)
        {
          PRUint8 alpha = *tmp++;
          mAlphaBits[alphaRowStart + x] = alpha;
          PRUint32 offset = rowStart + COMPS_PER_PIXEL * x;
          mImageBits[offset + 1] = ((PRUint32) *tmp++) * 255 / alpha;
          mImageBits[offset + 2] = ((PRUint32) *tmp++) * 255 / alpha;
          mImageBits[offset + 3] = ((PRUint32) *tmp++) * 255 / alpha;
        }
      }
      break;
    }

    // mImageBits still exists for mAlphaDepth == 1 or mAlphaDepth == 0,
    // since the bits are owned by the CGImageRef.  So the only thing to
    // do is to recreate the alpha bits for mAlphaDepth == 1.

    case 1:
    {
      // recreate the alpha bits structure
      for (PRInt32 y = 0; y < mHeight; y++)
      {
        PRInt32 rowStart = mRowBytes * y;
        PRUint8* alphaRow = mAlphaBits + mAlphaRowBytes * y;
        for (PRInt32 x = 0; x < mWidth; x++)
        {
          if (imageBits[rowStart + COMPS_PER_PIXEL * x])
            SetAlphaBit(alphaRow, x);
          else
            ClearAlphaBit(alphaRow, x);
        }
      }
    }

    case 0:
    default:
      break;
  }

  PR_Free(imageBits);
  return NS_OK;
}


NS_IMETHODIMP
nsImageMac::UnlockImagePixels(PRBool aMaskPixels)
{
  if (mOptimized)
    Optimize(nsnull);

  return NS_OK;
}


#pragma mark -

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
// Draw the image into a temporary GWorld, and then blit it into the picture.
//
// XXX TODO In the future, clipboard operations should be converted to using
// the Pasteboard Manager.  Then we could simply write our image to a PDF
// context and put that on the clipboard.
//
NS_IMETHODIMP
nsImageMac::ConvertToPICT(PicHandle* outPicture)
{
  *outPicture = nsnull;
  Rect picFrame  = {0, 0, mHeight, mWidth};

  PRUint32 pixelDepth = ::CGImageGetBitsPerPixel(mImage);

  // allocate a "normal" GWorld (which owns its own pixels)
  GWorldPtr tempGWorld = NULL;
  ::NewGWorld(&tempGWorld, pixelDepth, &picFrame, nsnull, nsnull, 0);
  if (!tempGWorld)
    return NS_ERROR_FAILURE;

  PixMapHandle tempPixMap = ::GetGWorldPixMap(tempGWorld);
  if (tempPixMap)
  {
    StPixelLocker tempPixLocker(tempPixMap);      // locks the pixels

    // erase it to white
    {
      StGWorldPortSetter setter(tempGWorld);
      ::BackColor(whiteColor);
      ::EraseRect(&picFrame);
    }

    // set as current GWorld
    GWorldPtr currPort;
    GDHandle currDev;
    ::GetGWorld(&currPort, &currDev);
    ::SetGWorld(tempGWorld, nsnull);

    ::SetOrigin(0, 0);
    ::ForeColor(blackColor);
    ::BackColor(whiteColor);

    // Get the storage addr for the GWorld and create a bitmap context from it.
    // Then we simply call CGContextDrawImage to draw our image into the GWorld.
    // NOTE: The width of the created context must be a multiple of 4.
    PRUint8* bitmap = (PRUint8*) ::GetPixBaseAddr(tempPixMap);

    PRUint32 bytesPerPixel = ::CGImageGetBitsPerPixel(mImage) / 8;
    PRUint32 bitmapWidth = (mWidth + 4) & ~0x3;
    PRUint32 bitmapRowBytes = bitmapWidth * bytesPerPixel;

    CGColorSpaceRef cs = ::CGColorSpaceCreateDeviceRGB();
    StColorSpaceReleaser csReleaser(cs);
    CGContextRef bitmapContext =
                    ::CGBitmapContextCreate(bitmap, bitmapWidth, mHeight,
                                            BITS_PER_COMPONENT,
                                            bitmapRowBytes, cs,
                                            kCGImageAlphaPremultipliedFirst);

    NS_ASSERTION(bitmapContext, "Failed to create bitmap context");

    if (bitmapContext)
    {
      // Translate to QuickDraw coordinate system
      ::CGContextTranslateCTM(bitmapContext, 0, mHeight);
      ::CGContextScaleCTM(bitmapContext, 1, -1);

      // Draw image into GWorld
      CGRect drawRect = ::CGRectMake(0, 0, mWidth, mHeight);
      ::CGContextDrawImage(bitmapContext, drawRect, mImage);
      ::CGContextRelease(bitmapContext);

      PicHandle thePicture = ::OpenPicture(&picFrame);
      if (thePicture)
      {
        // blit image from GWorld into Picture
        ::CopyBits(::GetPortBitMapForCopyBits(tempGWorld),
                   ::GetPortBitMapForCopyBits(tempGWorld),
                   &picFrame, &picFrame, ditherCopy, nsnull);

        ::ClosePicture();
        if (QDError() == noErr)
          *outPicture = thePicture;
      }
    }

    ::SetGWorld(currPort, currDev);     // restore to the way things were
  }

  ::DisposeGWorld(tempGWorld);        // do this after dtor of tempPixLocker!

  return *outPicture ? NS_OK : NS_ERROR_FAILURE;
}


NS_IMETHODIMP
nsImageMac::ConvertFromPICT(PicHandle inPicture)
{
  NS_WARNING("ConvertFromPICT is not implemented.");
  return NS_ERROR_NOT_IMPLEMENTED;
}


#pragma mark -

nsresult
nsImageMac::SlowTile(nsIRenderingContext &aContext, nsIDrawingSurface* aSurface,
                     PRInt32 aSXOffset, PRInt32 aSYOffset,
                     PRInt32 aPadX, PRInt32 aPadY, const nsRect &aTileRect)
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

  for (PRInt32 y = aY0; y < aY1; y += mHeight + aPadY)
    for (PRInt32 x = aX0; x < aX1; x += mWidth + aPadX)
    {
      Draw(aContext, aSurface,
           0, 0, PR_MIN(validWidth, aX1-x), PR_MIN(validHeight, aY1-y),     // src coords
           x, y, PR_MIN(validWidth, aX1-x), PR_MIN(validHeight, aY1-y));    // dest coords
    }

  return NS_OK;
}


nsresult
nsImageMac::DrawTileQuickly(nsIRenderingContext &aContext,
                            nsIDrawingSurface* aSurface,
                            PRInt32 aSXOffset, PRInt32 aSYOffset,
                            const nsRect &aTileRect)
{
  CGColorSpaceRef cs = ::CGColorSpaceCreateDeviceRGB();
  StColorSpaceReleaser csReleaser(cs);

  PRUint32 tiledCols = (aTileRect.width + aSXOffset + mWidth - 1) / mWidth;
  PRUint32 tiledRows = (aTileRect.height + aSYOffset + mHeight - 1) / mHeight;

  // XXX note that this code tiles the entire image, even when we just
  // need a small portion, which can get expensive
  PRUint32 bitmapWidth = tiledCols * mWidth;
  PRUint32 bitmapHeight = tiledRows * mHeight;
  PRUint32 bitmapRowBytes = tiledCols * mRowBytes;
  PRUint32 totalBytes = bitmapHeight * bitmapRowBytes;
  PRUint8* bitmap = (PRUint8*) PR_Malloc(totalBytes);
  if (!bitmap)
    return NS_ERROR_OUT_OF_MEMORY;

  CGContextRef bitmapContext;
  bitmapContext = ::CGBitmapContextCreate(bitmap, bitmapWidth, bitmapHeight,
                                          BITS_PER_COMPONENT, bitmapRowBytes,
                                          cs, kCGImageAlphaPremultipliedFirst);

  if (bitmapContext != NULL)
  {
    // clear the bitmap context
    ::CGContextClearRect(bitmapContext,
                         ::CGRectMake(0, 0, bitmapWidth, bitmapHeight));
  
    // prime bitmap with initial tile draw
    // Draw image into 'bottom' of bitmap, to make the calculations in the
    // following for loops somewhat simpler.
    CGRect drawRect = ::CGRectMake(0, bitmapHeight - mHeight, mWidth, mHeight);
    ::CGContextDrawImage(bitmapContext, drawRect, mImage);
    ::CGContextRelease(bitmapContext);

    // Manually blit image, doubling each time.
    // Quartz does not provide any functions for blitting from a context to a
    // context, so rather than create a CFImageRef each time through the loop
    // in order to blit it to the context, we just do it manually.
    PRUint32 tileHeight = mHeight;
    for (PRUint32 destCol = 1; destCol < tiledCols; destCol *= 2)
    {
      PRUint8* srcLine = bitmap;
      PRUint32 bytesToCopy = destCol * mRowBytes;
      PRUint8* destLine = srcLine + bytesToCopy;
      if (destCol * 2 > tiledCols)
      {
        bytesToCopy = (tiledCols - destCol) * mRowBytes;
      }
      for (PRUint32 row = 0; row < tileHeight; row++)
      {
        memcpy(destLine, srcLine, bytesToCopy);
        srcLine += bitmapRowBytes;
        destLine += bitmapRowBytes;
      }
    }

    for (PRUint32 destRow = 1; destRow < tiledRows; destRow *= 2)
    {
      PRUint32 tileRowBytes = mHeight * bitmapRowBytes;
      PRUint32 bytesToCopy = destRow * tileRowBytes;
      PRUint8* dest = bitmap + bytesToCopy;
      if (destRow * 2 > tiledRows)
      {
        bytesToCopy = (tiledRows - destRow) * tileRowBytes;
      }
      memcpy(dest, bitmap, bytesToCopy);
    }

    // Create final tiled image from bitmap
    CGDataProviderRef prov = ::CGDataProviderCreateWithData(NULL, bitmap,
                                                            totalBytes, NULL);
    CGImageRef tiledImage = ::CGImageCreate(bitmapWidth, bitmapHeight,
                                            BITS_PER_COMPONENT, BITS_PER_PIXEL,
                                            bitmapRowBytes, cs,
                                            kCGImageAlphaPremultipliedFirst,
                                            prov, NULL, TRUE,
                                            kCGRenderingIntentDefault);
    ::CGDataProviderRelease(prov);

    nsDrawingSurfaceMac* surface = static_cast<nsDrawingSurfaceMac*>(aSurface);
    CGContextRef context = surface->StartQuartzDrawing();

    CGRect srcRect = ::CGRectMake(aSXOffset, aSYOffset, aTileRect.width,
                                  aTileRect.height);
    CGRect destRect = ::CGRectMake(aTileRect.x, aTileRect.y, aTileRect.width,
                                   aTileRect.height);

    drawRect = ::CGRectMake(0, 0, bitmapWidth, bitmapHeight);
    if (!::CGRectEqualToRect(srcRect, destRect))
    {
      // If drawing a portion of the image, change the drawRect accordingly.
      float sx = ::CGRectGetWidth(destRect) / ::CGRectGetWidth(srcRect);
      float sy = ::CGRectGetHeight(destRect) / ::CGRectGetHeight(srcRect);
      float dx = ::CGRectGetMinX(destRect) - (::CGRectGetMinX(srcRect) * sx);
      float dy = ::CGRectGetMinY(destRect) - (::CGRectGetMinY(srcRect) * sy);
      drawRect = ::CGRectMake(dx, dy, bitmapWidth * sx, bitmapHeight * sy);
    }

    ::CGContextClipToRect(context, destRect);
    ::CGContextDrawImage(context, drawRect, tiledImage);

    ::CGImageRelease(tiledImage);
    surface->EndQuartzDrawing(context);
  }

  PR_Free(bitmap);

  return NS_OK;
}

void
DrawTileAsPattern(void *aInfo, CGContextRef aContext)
{
  CGImageRef image = static_cast<CGImageRef> (aInfo);

  float width = ::CGImageGetWidth(image);
  float height = ::CGImageGetHeight(image);
  CGRect drawRect = ::CGRectMake(0, 0, width, height);
  ::CGContextDrawImage(aContext, drawRect, image);
}

nsresult
nsImageMac::DrawTileWithQuartz(nsIDrawingSurface* aSurface,
                               PRInt32 aSXOffset, PRInt32 aSYOffset,
                               PRInt32 aPadX, PRInt32 aPadY,
                               const nsRect &aTileRect)
{
  nsDrawingSurfaceMac* surface = static_cast<nsDrawingSurfaceMac*>(aSurface);
  CGContextRef context = surface->StartQuartzDrawing();
  ::CGContextSaveGState(context);

  static const CGPatternCallbacks callbacks = {0, &DrawTileAsPattern, NULL};

  // get the current transform from the context
  CGAffineTransform patternTrans = CGContextGetCTM(context);
  patternTrans = CGAffineTransformTranslate(patternTrans, aTileRect.x, aTileRect.y);

  CGPatternRef pattern;
  pattern = ::CGPatternCreate(mImage, ::CGRectMake(0, 0, mWidth, mHeight),
                              patternTrans, mWidth + aPadX, mHeight + aPadY,
                              kCGPatternTilingConstantSpacing,
                              TRUE, &callbacks);

  CGColorSpaceRef patternSpace = ::CGColorSpaceCreatePattern(NULL);
  ::CGContextSetFillColorSpace(context, patternSpace);
  ::CGColorSpaceRelease(patternSpace);

  float alpha = 1.0f;
  ::CGContextSetFillPattern(context, pattern, &alpha);
  ::CGPatternRelease(pattern);
  
  // set the pattern phase (really -ve x and y offsets, but we negate
  // the y offset again to take flipping into account)
  ::CGContextSetPatternPhase(context, CGSizeMake(-aSXOffset, aSYOffset));

  CGRect tileRect = ::CGRectMake(aTileRect.x,
                                 aTileRect.y,
                                 aTileRect.width,
                                 aTileRect.height);

  ::CGContextFillRect(context, tileRect);
  
  ::CGContextRestoreGState(context);
  surface->EndQuartzDrawing(context);
  return NS_OK;
}


NS_IMETHODIMP nsImageMac::DrawTile(nsIRenderingContext &aContext,
                                   nsIDrawingSurface* aSurface,
                                   PRInt32 aSXOffset, PRInt32 aSYOffset,
                                   PRInt32 aPadX, PRInt32 aPadY,
                                   const nsRect &aTileRect)
{
  nsresult rv = EnsureCachedImage();
  NS_ENSURE_SUCCESS(rv, rv);

  if (mDecodedX2 < mDecodedX1 || mDecodedY2 < mDecodedY1)
    return NS_OK;

  PRUint32 tiledCols = (aTileRect.width + aSXOffset + mWidth - 1) / mWidth;
  PRUint32 tiledRows = (aTileRect.height + aSYOffset + mHeight - 1) / mHeight;

  // The tiling that we do below can be expensive for large background
  // images, for which we have few tiles.  Plus, the SlowTile call eventually
  // calls the Quartz drawing functions, which can take advantage of hardware
  // acceleration.  So if we have few tiles, we just call SlowTile.
#ifdef USE_CGPATTERN_TILING
  const PRUint32 kTilingCopyThreshold = 16;   // CG tiling is so much more efficient
#else
  const PRUint32 kTilingCopyThreshold = 64;
#endif
  if (tiledCols * tiledRows < kTilingCopyThreshold)
    return SlowTile(aContext, aSurface, aSXOffset, aSYOffset, 0, 0, aTileRect);
  
#ifdef USE_CGPATTERN_TILING
  rv = DrawTileWithQuartz(aSurface, aSXOffset, aSYOffset, aPadX, aPadY, aTileRect);
#else
  // use the manual methods of tiling
  if (!aPadX && !aPadY)
    rv = DrawTileQuickly(aContext, aSurface, aSXOffset, aSYOffset, aTileRect);
  else
    rv = SlowTile(aContext, aSurface, aSXOffset, aSYOffset, aPadX, aPadY, aTileRect);
#endif /* USE_CGPATTERN_TILING */

  return rv;
}
