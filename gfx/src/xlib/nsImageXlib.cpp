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
 *   Peter Hartshorn <peter@igelaus.com.au>
 *   Stuart Parmenter <pavlov@netscape.com>
 *   Tim Rowley <tor@cs.brown.edu> -- 8bit alpha compositing
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

#include "nsImageXlib.h"
#include "nsDrawingSurfaceXlib.h"
#include "nsDeviceContextXlib.h"
#include "nsRenderingContextXlib.h"
#include "xlibrgb.h"
#include "prlog.h"
#include "nsRect.h"
#include "drawers.h"
#include "imgScaler.h"

#define IsFlagSet(a,b) ((a) & (b))

#ifdef PR_LOGGING 
static PRLogModuleInfo *ImageXlibLM = PR_NewLogModule("ImageXlib");
#endif /* PR_LOGGING */ 

/* XXX we are simply creating a GC and setting its function to Copy.
   we shouldn't be doing this every time this method is called.  this creates
   way more trips to the server than we should be doing so we are creating a
   static one.
*/
static GC s1bitGC = 0;
static GC sXbitGC = 0;

XlibRgbHandle *nsImageXlib::mXlibRgbHandle = nsnull;
Display       *nsImageXlib::mDisplay       = nsnull;

nsImageXlib::nsImageXlib()
{
  PR_LOG(ImageXlibLM, PR_LOG_DEBUG, ("nsImageXlib::nsImageXlib()\n"));
  NS_INIT_REFCNT();
  mImageBits = nsnull;
  mAlphaBits = nsnull;
  mWidth = 0;
  mHeight = 0;
  mDepth = 0;
  mRowBytes = 0;
  mSizeImage = 0;
  mNumBytesPixel = 0;
  mImagePixmap = nsnull;
  mAlphaPixmap = nsnull;
  mAlphaDepth = 0;
  mAlphaRowBytes = 0;
  mAlphaWidth = 0;
  mAlphaHeight = 0;
  mAlphaValid = PR_FALSE;
  mIsSpacer = PR_TRUE;
  mGC = nsnull;
  mNaturalWidth = 0;
  mNaturalHeight = 0;
  mPendingUpdate = PR_FALSE;

  if (!mXlibRgbHandle) {
    mXlibRgbHandle = xxlib_find_handle(XXLIBRGB_DEFAULT_HANDLE);
    mDisplay = xxlib_rgb_get_display(mXlibRgbHandle);
  }  
  
  if (!mXlibRgbHandle || !mDisplay)
    abort();
}

nsImageXlib::~nsImageXlib()
{
  PR_LOG(ImageXlibLM, PR_LOG_DEBUG,("nsImageXlib::nsImageXlib()\n"));
  if (nsnull != mImageBits) {
    delete[] mImageBits;
    mImageBits = nsnull;
  }
  if (nsnull != mAlphaBits) {
    delete[] mAlphaBits;
    mAlphaBits = nsnull;

    if (mAlphaPixmap != nsnull) 
    {
      // The display cant be null.  It gets fetched from the drawing 
      // surface used to create the pixmap.  It gets assigned once
      // in Draw()
      NS_ASSERTION(nsnull != mDisplay,"display is null.");

#ifdef XLIB_PIXMAP_DEBUG
      printf("XFreePixmap(display = %p)\n",mDisplay);
#endif

      XFreePixmap(mDisplay, mAlphaPixmap);

    }
  }

  if (mImagePixmap != 0) 
  {
    NS_ASSERTION(nsnull != mDisplay,"display is null.");

#ifdef XLIB_PIXMAP_DEBUG
    printf("XFreePixmap(display = %p)\n",mDisplay);
#endif

    XFreePixmap(mDisplay, mImagePixmap);
  }

  if(mGC)
  {
    XFreeGC(mDisplay, mGC);
    mGC=nsnull;
  }
  if(sXbitGC && mDisplay) // Sometimes mDisplay is null, let orhers free
  {
    XFreeGC(mDisplay, sXbitGC);
    sXbitGC=nsnull;
  }
  if(s1bitGC && mDisplay) // Sometimes mDisplay is null, let orhers free
  {
    XFreeGC(mDisplay, s1bitGC);
    s1bitGC=nsnull;
  }
  
}

NS_IMPL_ISUPPORTS1(nsImageXlib, nsIImage);

nsresult nsImageXlib::Init(PRInt32 aWidth, PRInt32 aHeight,
                           PRInt32 aDepth, nsMaskRequirements aMaskRequirements)
{
  if ((aWidth == 0) || (aHeight == 0))
    return NS_ERROR_FAILURE;

  if (nsnull != mImageBits) {
    delete[] mImageBits;
    mImageBits = nsnull;
  }
  
  if (nsnull != mAlphaBits) {
    delete[] mAlphaBits;
    mAlphaBits = nsnull;;
  }
  if (nsnull != mAlphaPixmap) {
    XFreePixmap(mDisplay, mAlphaPixmap);
    mAlphaPixmap = nsnull;
  }

  SetDecodedRect(0,0,0,0);
  SetNaturalWidth(0);
  SetNaturalHeight(0);

  if (nsnull != mImagePixmap) {
    XFreePixmap(mDisplay, mImagePixmap);
    mImagePixmap = nsnull;
  }

  if (24 == aDepth) {
    mNumBytesPixel = 3;
  } else {
    NS_ASSERTION(PR_FALSE, "unexpected image depth");
    return NS_ERROR_UNEXPECTED;
  }

  mWidth = aWidth;
  mHeight = aHeight;
  mDepth = aDepth;
  mIsTopToBottom = PR_TRUE;

  // Create the memory for the image
  ComputeMetrics();

  mImageBits = (PRUint8*)new PRUint8[mSizeImage];

  switch(aMaskRequirements) {
    case nsMaskRequirements_kNoMask:
      mAlphaBits = nsnull;
      mAlphaWidth = 0;
      mAlphaHeight = 0;
      break;

    case nsMaskRequirements_kNeeds1Bit:
      mAlphaRowBytes = (aWidth  + 7) / 8;
      mAlphaDepth = 1;

      // 32-bit align each row
      mAlphaRowBytes = (mAlphaRowBytes + 3) & ~0x3;

      mAlphaBits = new unsigned char[mAlphaRowBytes * aHeight];
      mAlphaWidth = aWidth;
      mAlphaHeight = aHeight;
      break;

    case nsMaskRequirements_kNeeds8Bit:
      mAlphaRowBytes = aWidth;
      mAlphaDepth = 8;

      // 32-bit align each row
      mAlphaRowBytes = (mAlphaRowBytes + 3) & ~0x3;
      mAlphaBits = new unsigned char[mAlphaRowBytes * aHeight];
      mAlphaWidth = aWidth;
      mAlphaHeight = aHeight;
      break;
  }
  return NS_OK;
}

//---------------------------------------------------------------------

PRInt32 nsImageXlib::GetHeight()
{
  return mHeight;
}

PRInt32 nsImageXlib::GetWidth()
{
  return mWidth;
}

PRUint8 *nsImageXlib::GetBits()
{
  return mImageBits;
}

void *nsImageXlib::GetBitInfo()
{
  return nsnull;
}

PRInt32 nsImageXlib::GetLineStride()
{
  return mRowBytes;
}

nsColorMap *nsImageXlib::GetColorMap()
{
  return nsnull;
}

PRBool nsImageXlib::IsOptimized()
{
  return PR_TRUE;
}

PRUint8 *nsImageXlib::GetAlphaBits()
{
  return mAlphaBits;
}

PRInt32 nsImageXlib::GetAlphaWidth()
{
  return mAlphaWidth;
}

PRInt32 nsImageXlib::GetAlphaHeight()
{
  return mAlphaHeight;
}

PRInt32 nsImageXlib::GetAlphaLineStride()
{
  return mAlphaRowBytes;
}

nsIImage *nsImageXlib::DuplicateImage()
{
  return nsnull;
}

void nsImageXlib::SetAlphaLevel(PRInt32 aAlphaLevel)
{
}

PRInt32 nsImageXlib::GetAlphaLevel()
{
  return 0;
}

void nsImageXlib::MoveAlphaMask(PRInt32 aX, PRInt32 aY)
{
}

//-----------------------------------------------------------------------

// Set up the palette to the passed in color array, RGB only in this array
void nsImageXlib::ImageUpdated(nsIDeviceContext *aContext,
                               PRUint8 aFlags,
                               nsRect *aUpdateRect)
{
  mPendingUpdate = PR_TRUE;
  mUpdateRegion.Or(mUpdateRegion, *aUpdateRect);
}

void nsImageXlib::UpdateCachedImage()
{
  nsRegionRectIterator ri(mUpdateRegion);
  const nsRect *rect;

  while (rect = ri.Next()) {

//  fprintf(stderr, "ImageUpdated %p x,y=(%d %d) width,height=(%d %d)\n",
//          this, rect->x, rect->y, rect->width, rect->height);

    unsigned bottom, left, right;
    bottom = rect->y + rect->height;
    left   = rect->x;
    right  = left + rect->width;

    // check if the image has an all-opaque 8-bit alpha mask
    if ((mAlphaDepth==8) && !mAlphaValid) {
      for (unsigned y=rect->y; (y<bottom) && !mAlphaValid; y++) {
        unsigned char *alpha = mAlphaBits + mAlphaRowBytes*y + left;
        for (unsigned x=left; x<right; x++) {
          if (*(alpha++)!=255) {
            mAlphaValid=PR_TRUE;
            break;
          }
        }
      }
    }

    // check if the image is a spacer
    if ((mAlphaDepth==1) && mIsSpacer) {
      // mask of the leading/trailing bits in the update region
      PRUint8  leftmask   = 0xff  >> (left & 0x7);
      PRUint8  rightmask  = 0xff  << (7 - ((right-1) & 0x7));

      // byte where the first/last bits of the update region are located
      PRUint32 leftindex  = left      >> 3;
      PRUint32 rightindex = (right-1) >> 3;

      // first/last bits in the same byte - combine mask into leftmask
      // and fill rightmask so we don't try using it
      if (leftindex == rightindex) {
        leftmask &= rightmask;
        rightmask = 0xff;
      }

      // check the leading bits
      if (leftmask != 0xff) {
        PRUint8 *ptr = mAlphaBits + mAlphaRowBytes * rect->y + leftindex;
        for (unsigned y=rect->y; y<bottom; y++, ptr+=mAlphaRowBytes) {
          if (*ptr & leftmask) {
            mIsSpacer = PR_FALSE;
            break;
          }
        }
        // move to first full byte
        leftindex++;
      }

      // check the trailing bits
      if (mIsSpacer && (rightmask != 0xff)) {
        PRUint8 *ptr = mAlphaBits + mAlphaRowBytes * rect->y + rightindex;
        for (unsigned y=rect->y; y<bottom; y++, ptr+=mAlphaRowBytes) {
          if (*ptr & rightmask) {
            mIsSpacer = PR_FALSE;
            break;
          }
        }
        // move to last full byte
        rightindex--;
      }
    
      // check the middle bytes
      if (mIsSpacer && (leftindex <= rightindex)) {
        for (unsigned y=rect->y; (y<bottom) && mIsSpacer; y++) {
          unsigned char *alpha = mAlphaBits + mAlphaRowBytes*y + leftindex;
          for (unsigned x=left; x<right; x++) {
            if (*(alpha++)!=0) {
              mIsSpacer = PR_FALSE;
              break;
            }
          }
        }
      }
    }

    if (mAlphaValid && mImagePixmap) {
      XFreePixmap(mDisplay, mImagePixmap);
      mImagePixmap = 0;
    }
    
    if (!mAlphaValid) {
      CreateOffscreenPixmap(mWidth, mHeight);

      if (!sXbitGC) {
        XGCValues gcv;
        memset(&gcv, 0, sizeof(XGCValues));
        gcv.function = GXcopy;
        sXbitGC  = XCreateGC(mDisplay, mImagePixmap, GCFunction, &gcv);
      }
      xxlib_draw_rgb_image_dithalign(
                     mXlibRgbHandle,
                     mImagePixmap, sXbitGC,
                     rect->x, rect->y,
                     rect->width, rect->height,
                     XLIB_RGB_DITHER_MAX,
                     mImageBits + mRowBytes * rect->y + 3 * rect->x,
                     mRowBytes,
                     rect->x, rect->y);
    }
  }

  mUpdateRegion.Empty();
  mPendingUpdate = PR_FALSE;
  mFlags = nsImageUpdateFlags_kBitsChanged; // this should be 0'd out by Draw()
}

NS_IMETHODIMP
nsImageXlib::DrawScaled(nsIRenderingContext &aContext,
                        nsDrawingSurface aSurface,
                        PRInt32 aSX, PRInt32 aSY,
                        PRInt32 aSWidth, PRInt32 aSHeight,
                        PRInt32 aDX, PRInt32 aDY,
                        PRInt32 aDWidth, PRInt32 aDHeight)
{

  PRInt32 origSHeight = aSHeight, origDHeight = aDHeight;
  PRInt32 origSWidth = aSWidth, origDWidth = aDWidth;

  if (aSWidth < 0 || aDWidth < 0 || aSHeight < 0 || aDHeight < 0)
    return NS_ERROR_FAILURE;

  if (0 == aSWidth || 0 == aDWidth || 0 == aSHeight || 0 == aDHeight)
    return NS_OK;

  // limit the size of the blit to the amount of the image read in
  if (aSX + aSWidth > mDecodedX2) {
    aDWidth -= ((aSX + aSWidth - mDecodedX2)*origDWidth)/origSWidth;
    aSWidth -= (aSX + aSWidth) - mDecodedX2;
  }
  if (aSX < mDecodedX1) {
    aDX += ((mDecodedX1 - aSX)*origDWidth)/origSWidth;
    aSX = mDecodedX1;
  }

  if (aSY + aSHeight > mDecodedY2) {
    aDHeight -= ((aSY + aSHeight - mDecodedY2)*origDHeight)/origSHeight;
    aSHeight -= (aSY + aSHeight) - mDecodedY2;
    }
  if (aSY < mDecodedY1) {
    aDY += ((mDecodedY1 - aSY)*origDHeight)/origSHeight;
    aSY = mDecodedY1;
  }

  if ((aDWidth <= 0 || aDHeight <= 0) || (aSWidth <= 0 || aSHeight <= 0))
    return NS_OK;

  nsIDrawingSurfaceXlib *drawing = NS_STATIC_CAST(nsIDrawingSurfaceXlib *, aSurface);

  if (mAlphaDepth == 1)
    CreateAlphaBitmap(mWidth, mHeight);

  if ((mAlphaDepth == 8) && mAlphaValid) {
    DrawComposited(aContext, aSurface,
        aSX, aSY, aSWidth, aSHeight,
        aDX, aDY, aDWidth, aDHeight);
    return NS_OK;
  }

#ifdef HAVE_XIE
  /* XIE seriosly loses scaling images with alpha */
  if (!mAlphaDepth) {
    /* Draw with XIE */
    PRBool succeeded = PR_FALSE;

    xGC *xiegc = ((nsRenderingContextXlib&)aContext).GetGC();
    Drawable drawable; drawing->GetDrawable(drawable);
    succeeded = DrawScaledImageXIE(mDisplay, drawable,
                                   *xiegc,
                                   mImagePixmap,
                                   mWidth, mHeight,
                                   aSX, aSY,
                                   aSWidth, aSHeight,
                                   aDX, aDY,
                                   aDWidth, aDHeight);
    xiegc->Release();
    if (succeeded)
      return NS_OK;
  }
#endif

  /* the good scaling way, right from GTK */
  GC gc = 0;
  Pixmap pixmap = 0;

  if (mAlphaDepth==1) {
    PRUint32 scaledRowBytes = (aDWidth+7)>>3;   // round to next byte
    PRUint8 *scaledAlpha = (PRUint8 *)nsMemory::Alloc(aDHeight*scaledRowBytes);
    
    // code below attempts to draw the image without the mask if mask
    // creation fails for some reason.  thus no easy-out "return"
    if (scaledAlpha) {
      memset(scaledAlpha, 0, aDHeight*scaledRowBytes);
      RectStretch(aSX, aSY, aSX+aSWidth-1, aSY+aSHeight-1,
          0, 0, aDWidth-1, aDHeight-1,
          mAlphaBits, mAlphaRowBytes, scaledAlpha, scaledRowBytes, 1);

      pixmap = XCreatePixmap(mDisplay, DefaultRootWindow(mDisplay),
                             aDWidth, aDHeight, 1);
      XImage *ximage = 0;
      
      if (pixmap) {
        ximage = XCreateImage(mDisplay, xxlib_rgb_get_visual(mXlibRgbHandle),
                              1, XYPixmap, 0, (char *)scaledAlpha,
                              aDWidth, aDHeight,
                              8, scaledRowBytes);
      }
      if (ximage) {
        ximage->bits_per_pixel=1;
        ximage->bitmap_bit_order=MSBFirst;
        ximage->byte_order = MSBFirst;

        GC tmpGC;
        XGCValues gcv;
        memset(&gcv, 0, sizeof(XGCValues));
        gcv.function = GXcopy;
        tmpGC = XCreateGC(mDisplay, pixmap, GCFunction, &gcv);
        if (tmpGC) {
          XPutImage(mDisplay, pixmap, tmpGC, ximage,
                    0, 0, 0, 0, aDWidth, aDHeight);
          XFreeGC(mDisplay, tmpGC);
        } else {
          // can't write into the clip mask - destroy so we don't use it
          if (pixmap)
             XFreePixmap(mDisplay, pixmap);
          pixmap = 0;
        }

        ximage->data = 0;
        XDestroyImage(ximage);
      }
      nsMemory::Free(scaledAlpha);
    }
  }

  xGC *imageGC = nsnull;

  if (pixmap) {
    XGCValues values;

    memset(&values, 0, sizeof(XGCValues));
    values.clip_x_origin = aDX;
    values.clip_y_origin = aDY;
    values.clip_mask = pixmap;
    Drawable drawable; drawing->GetDrawable(drawable);
    gc = XCreateGC(mDisplay, drawable,
                   GCClipXOrigin | GCClipYOrigin | GCClipMask,
                   &values);
  } else {
    imageGC = ((nsRenderingContextXlib&)aContext).GetGC();
    gc = *imageGC;
  }

  PRUint8 *scaledRGB = (PRUint8 *)nsMemory::Alloc(3*aDWidth*aDHeight);
  if (scaledRGB && gc) {
    RectStretch(aSX, aSY, aSX+aSWidth-1, aSY+aSHeight-1,
                0, 0, aDWidth-1, aDHeight-1,
                mImageBits, mRowBytes, scaledRGB, 3*aDWidth, 24);

    Drawable drawable; drawing->GetDrawable(drawable);
    xxlib_draw_rgb_image(mXlibRgbHandle, drawable, gc,
                         aDX, aDY, aDWidth, aDHeight,
                         XLIB_RGB_DITHER_MAX,
                         scaledRGB, 3*aDWidth);
    nsMemory::Free(scaledRGB);
  }

  if (imageGC)
    imageGC->Release();
  else
    if (gc)
      XFreeGC(mDisplay, gc);
  if (pixmap)
    XFreePixmap(mDisplay, pixmap);

  mFlags = 0;

  return NS_OK;
}

// Draw the bitmap, this method has a source and destination coordinates
NS_IMETHODIMP
nsImageXlib::Draw(nsIRenderingContext &aContext, nsDrawingSurface aSurface,
                  PRInt32 aSX, PRInt32 aSY, PRInt32 aSWidth, PRInt32 aSHeight,
                  PRInt32 aDX, PRInt32 aDY, PRInt32 aDWidth, PRInt32 aDHeight)
{
  if (aSurface == nsnull)
    return NS_ERROR_FAILURE;

  if (mPendingUpdate)
    UpdateCachedImage();

  if ((mAlphaDepth == 1) && mIsSpacer)
    return NS_OK;

  if (aSWidth != aDWidth || aSHeight != aDHeight) {
    return DrawScaled(aContext, aSurface, aSX, aSY, aSWidth, aSHeight,
                      aDX, aDY, aDWidth, aDHeight);
  }

  if (aSWidth <= 0 || aDWidth <= 0 || aSHeight <= 0 || aDHeight <= 0) {
    NS_ASSERTION(aSWidth > 0 && aDWidth > 0 && aSHeight > 0 && aDHeight > 0,
                 "You can't draw an image with a 0 width or height!");
    return NS_OK;
  }

  // limit the size of the blit to the amount of the image read in
  PRInt32 j = aSX + aSWidth;
  PRInt32 z;
  if (j > mDecodedX2) {
    z = j - mDecodedX2;
    aDWidth -= z;
    aSWidth -= z;
  }
  if (aSX < mDecodedX1) {
    aDX += mDecodedX1 - aSX;
    aSX = mDecodedX1;
  }

  j = aSY + aSHeight;
  if (j > mDecodedY2) {
    z = j - mDecodedY2;
    aDHeight -= z;
    aSHeight -= z;
  }
  if (aSY < mDecodedY1) {
    aDY += mDecodedY1 - aSY;
    aSY = mDecodedY1;
  }

  if (aDWidth <= 0 || aDHeight <= 0 || aSWidth <= 0 || aSHeight <= 0)
    return NS_OK;

  if ((mAlphaDepth == 8) && mAlphaValid) {
    DrawComposited(aContext, aSurface,
        aSX, aSY, aSWidth, aSHeight,
        aDX, aDY, aSWidth, aSHeight);
    return NS_OK;
  }

  nsIDrawingSurfaceXlib *drawing = NS_STATIC_CAST(nsIDrawingSurfaceXlib *, aSurface);

  if (mAlphaDepth == 1)
    CreateAlphaBitmap(mWidth, mHeight);

  GC copyGC;
  xGC *gc = ((nsRenderingContextXlib&)aContext).GetGC();

  if (mAlphaPixmap) {
    if (mGC) {                /* reuse GC */
      copyGC = mGC;
      SetupGCForAlpha(copyGC, aDX - aSX, aDY - aSY);
    } else {                  /* make a new one */
      /* this repeats things done in SetupGCForAlpha */
      XGCValues xvalues;
      memset(&xvalues, 0, sizeof(XGCValues));
      unsigned long xvalues_mask = 0;
      xvalues.clip_x_origin = aDX - aSX;
      xvalues.clip_y_origin = aDY - aSY;
      if (IsFlagSet(nsImageUpdateFlags_kBitsChanged, mFlags)) {
        xvalues_mask = GCClipXOrigin | GCClipYOrigin | GCClipMask;
        xvalues.clip_mask = mAlphaPixmap;
      }
      Drawable drawable; drawing->GetDrawable(drawable);
      mGC = XCreateGC(mDisplay, drawable, xvalues_mask , &xvalues);
      copyGC = mGC;
    }
  } else {  /* !mAlphaPixmap */
    copyGC = *gc;
  }

  Drawable drawable; drawing->GetDrawable(drawable);
  XCopyArea(mDisplay, mImagePixmap, drawable,
        copyGC, aSX, aSY, aSWidth, aSHeight, aDX, aDY);

  gc->Release();
  mFlags = 0;
  return NS_OK;
}

// -----------------------------------------------------------------
// 8-bit alpha composite drawing

static unsigned
findIndex32(unsigned mask)
{
  switch (mask)
  {
    case 0xff:
      return 3;
    case 0xff00:
      return 2;
    case 0xff0000:
      return 1;
    default:
      return 0;
  }
}

static unsigned
findIndex24(unsigned mask)
{
  switch(mask)
  {
    case 0xff:
      return 2;
    case 0xff00:
      return 1;
    default:
      return 0;
  }
}


// 32-bit (888) truecolor convert/composite function
void nsImageXlib::DrawComposited32(PRBool isLSB, PRBool flipBytes,
                                   PRUint8 *imageOrigin, PRUint32 imageStride,
                                   PRUint8 *alphaOrigin, PRUint32 alphaStride,
                                   unsigned width, unsigned height,
                                   XImage *ximage, unsigned char *readData)
{
  Visual *visual = xxlib_rgb_get_visual(mXlibRgbHandle);
  unsigned redIndex   = findIndex32(visual->red_mask);
  unsigned greenIndex = findIndex32(visual->green_mask);
  unsigned blueIndex  = findIndex32(visual->blue_mask);

  if (flipBytes^isLSB)
  {
    redIndex   = 3-redIndex;
    greenIndex = 3-greenIndex;
    blueIndex  = 3-blueIndex;
  }

  for (unsigned y=0; y<height; y++)
  {
    unsigned char *baseRow   = (unsigned char *)ximage->data
                                            +y*ximage->bytes_per_line;
    unsigned char *targetRow = readData     +3*(y*ximage->width);
    unsigned char *imageRow  = imageOrigin  +y*imageStride;
    unsigned char *alphaRow  = alphaOrigin  +y*alphaStride;

    for (unsigned i=0; i<width;
         i++, baseRow+=4, targetRow+=3, imageRow+=3, alphaRow++)
    {
      unsigned alpha = *alphaRow;
      MOZ_BLEND(targetRow[0], baseRow[redIndex],   imageRow[0], alpha);
      MOZ_BLEND(targetRow[1], baseRow[greenIndex], imageRow[1], alpha);
      MOZ_BLEND(targetRow[2], baseRow[blueIndex],  imageRow[2], alpha);
    }
  }
}

// 24-bit (888) truecolor convert/composite function
void
nsImageXlib::DrawComposited24(PRBool isLSB, PRBool flipBytes,
                             PRUint8 *imageOrigin, PRUint32 imageStride,
                             PRUint8 *alphaOrigin, PRUint32 alphaStride,
                             unsigned width, unsigned height,
                             XImage *ximage, unsigned char *readData)
{
  Visual *visual      = xxlib_rgb_get_visual(mXlibRgbHandle);
  unsigned redIndex   = findIndex24(visual->red_mask);
  unsigned greenIndex = findIndex24(visual->green_mask);
  unsigned blueIndex  = findIndex24(visual->blue_mask);

  if (flipBytes^isLSB) {
    redIndex   = 2-redIndex;
    greenIndex = 2-greenIndex;
    blueIndex  = 2-blueIndex;
  }

  for (unsigned y=0; y<height; y++) {
    unsigned char *baseRow   = (unsigned char *)ximage->data
                                            +y*ximage->bytes_per_line;
    unsigned char *targetRow = readData     +3*(y*ximage->width);
    unsigned char *imageRow  = imageOrigin  +y*imageStride;
    unsigned char *alphaRow  = alphaOrigin  +y*alphaStride;

    for (unsigned i=0; i<width;
         i++, baseRow+=3, targetRow+=3, imageRow+=3, alphaRow++) {
      unsigned alpha = *alphaRow;
      MOZ_BLEND(targetRow[0], baseRow[redIndex],   imageRow[0], alpha);
      MOZ_BLEND(targetRow[1], baseRow[greenIndex], imageRow[1], alpha);
      MOZ_BLEND(targetRow[2], baseRow[blueIndex],  imageRow[2], alpha);
    }
  }
}

unsigned nsImageXlib::scaled6[1<<6] = {
  3,   7,  11,  15,  19,  23,  27,  31,  35,  39,  43,  47,  51,  55,  59,  63,
 67,  71,  75,  79,  83,  87,  91,  95,  99, 103, 107, 111, 115, 119, 123, 127,
131, 135, 139, 143, 147, 151, 155, 159, 163, 167, 171, 175, 179, 183, 187, 191,
195, 199, 203, 207, 211, 215, 219, 223, 227, 231, 235, 239, 243, 247, 251, 255
};

unsigned nsImageXlib::scaled5[1<<5] = {
  7,  15,  23,  31,  39,  47,  55,  63,  71,  79,  87,  95, 103, 111, 119, 127,
135, 143, 151, 159, 167, 175, 183, 191, 199, 207, 215, 223, 231, 239, 247, 255
};

// 16-bit ([56][56][56]) truecolor convert/composite function
void
nsImageXlib::DrawComposited16(PRBool isLSB, PRBool flipBytes,
                             PRUint8 *imageOrigin, PRUint32 imageStride,
                             PRUint8 *alphaOrigin, PRUint32 alphaStride,
                             unsigned width, unsigned height,
                             XImage *ximage, unsigned char *readData)
{
  Visual *visual = xxlib_rgb_get_visual(mXlibRgbHandle);

  unsigned *redScale   = (xxlib_get_prec_from_mask(visual->red_mask)   == 5)
                          ? scaled5 : scaled6;
  unsigned *greenScale = (xxlib_get_prec_from_mask(visual->green_mask) == 5)
                          ? scaled5 : scaled6;
  unsigned *blueScale  = (xxlib_get_prec_from_mask(visual->blue_mask)  == 5)
                          ? scaled5 : scaled6;

  unsigned long redShift   = xxlib_get_shift_from_mask(visual->red_mask);
  unsigned long greenShift = xxlib_get_shift_from_mask(visual->green_mask);
  unsigned long blueShift  = xxlib_get_shift_from_mask(visual->blue_mask);

  for (unsigned y=0; y<height; y++) {
    unsigned char *baseRow   = (unsigned char *)ximage->data
                                            +y*ximage->bytes_per_line;
    unsigned char *targetRow = readData     +3*(y*ximage->width);
    unsigned char *imageRow  = imageOrigin  +y*imageStride;
    unsigned char *alphaRow  = alphaOrigin  +y*alphaStride;
    for (unsigned i=0; i<width;
         i++, baseRow+=2, targetRow+=3, imageRow+=3, alphaRow++) {
      unsigned pix;
      if (flipBytes) {
        unsigned char tmp[2];
        tmp[0] = baseRow[1];
        tmp[1] = baseRow[0];
        pix = *((short *)tmp);
      } else
        pix = *((short *)baseRow);
      unsigned alpha = *alphaRow;
      MOZ_BLEND(targetRow[0],
                redScale[(pix&visual->red_mask) >> redShift], 
                imageRow[0], alpha);
      MOZ_BLEND(targetRow[1],
                greenScale[(pix&visual->green_mask) >> greenShift], 
                imageRow[1], alpha);
      MOZ_BLEND(targetRow[2],
                blueScale[(pix&visual->blue_mask) >> blueShift], 
                imageRow[2], alpha);
    }
  }
}

// Generic convert/composite function
void
nsImageXlib::DrawCompositedGeneral(PRBool isLSB, PRBool flipBytes,
                                  PRUint8 *imageOrigin, PRUint32 imageStride,
                                  PRUint8 *alphaOrigin, PRUint32 alphaStride,
                                  unsigned width, unsigned height,
                                  XImage *ximage, unsigned char *readData)
{
  Visual *visual = xxlib_rgb_get_visual(mXlibRgbHandle);

  unsigned char *target = readData;

  // flip bytes
  if (flipBytes && (ximage->bits_per_pixel>=16)) {
    for (int row=0; row<ximage->height; row++) {
      unsigned char *ptr =
        (unsigned char*)ximage->data + row*ximage->bytes_per_line;
      if (ximage->bits_per_pixel==24) {  // Aurgh....
        for (int col=0;
             col<ximage->bytes_per_line;
             col+=(ximage->bits_per_pixel/8)) {
          unsigned char tmp;
          tmp = *ptr;
          *ptr = *(ptr+2);
          *(ptr+2) = tmp;
          ptr+=3;
        }
        continue;
      }

      for (int col=0;
               col<ximage->bytes_per_line;
               col+=(ximage->bits_per_pixel/8)) {
        unsigned char tmp;
        switch (ximage->bits_per_pixel) {
        case 16:
          tmp = *ptr;
          *ptr = *(ptr+1);
          *(ptr+1) = tmp;
          ptr+=2;
          break;
        case 32:
          tmp = *ptr;
          *ptr = *(ptr+3);
          *(ptr+3) = tmp;
          tmp = *(ptr+1);
          *(ptr+1) = *(ptr+2);
          *(ptr+2) = tmp;
          ptr+=4;
          break;
        }
      }
    }
  }

  unsigned redScale   = 8 - xxlib_get_prec_from_mask(visual->red_mask);
  unsigned greenScale = 8 - xxlib_get_prec_from_mask(visual->green_mask);
  unsigned blueScale  = 8 - xxlib_get_prec_from_mask(visual->blue_mask);
  unsigned redFill    = 0xff >> xxlib_get_prec_from_mask(visual->red_mask);
  unsigned greenFill  = 0xff >> xxlib_get_prec_from_mask(visual->green_mask);
  unsigned blueFill   = 0xff >> xxlib_get_prec_from_mask(visual->blue_mask);

  unsigned long redShift   = xxlib_get_shift_from_mask(visual->red_mask);
  unsigned long greenShift = xxlib_get_shift_from_mask(visual->green_mask);
  unsigned long blueShift  = xxlib_get_shift_from_mask(visual->blue_mask);

  for (int row=0; row<ximage->height; row++) {
    unsigned char *ptr =
      (unsigned char *)ximage->data + row*ximage->bytes_per_line;
    for (int col=0; col<ximage->width; col++) {
      unsigned pix = 0;
      switch (ximage->bits_per_pixel) {
      case 1:
        pix = (*ptr>>(col%8))&1;
        if ((col%8)==7)
          ptr++;
        break;
      case 4:
        pix = (col&1)?(*ptr>>4):(*ptr&0xf);
        if (col&1)
          ptr++;
        break;
      case 8:
        pix = *ptr++;
        break;
      case 16:
        pix = *((short *)ptr);
        ptr+=2;
        break;
      case 24:
        if (isLSB)
          pix = (*(ptr+2)<<16) | (*(ptr+1)<<8) | *ptr;
        else
          pix = (*ptr<<16) | (*(ptr+1)<<8) | *(ptr+2);
        ptr+=3;
        break;
      case 32:
        pix = *((unsigned *)ptr);
        ptr+=4;
        break;
      }

      *target++ =
        redFill|((pix&visual->red_mask) >> redShift)<<redScale;
      *target++ =
        greenFill|((pix&visual->green_mask) >> greenShift)<<greenScale;
      *target++ =
        blueFill|((pix&visual->blue_mask) >> blueShift)<<blueScale;
    }
  }

  // now composite
  for (unsigned y=0; y<height; y++) {
    unsigned char *targetRow = readData+3*y*width;
    unsigned char *imageRow  = imageOrigin  +y*imageStride;
    unsigned char *alphaRow  = alphaOrigin  +y*alphaStride;
    for (unsigned i=0; i<width; i++) {
      unsigned alpha = alphaRow[i];
      MOZ_BLEND(targetRow[3*i],   targetRow[3*i],   imageRow[3*i],   alpha);
      MOZ_BLEND(targetRow[3*i+1], targetRow[3*i+1], imageRow[3*i+1], alpha);
      MOZ_BLEND(targetRow[3*i+2], targetRow[3*i+2], imageRow[3*i+2], alpha);
    }
  }
}

void
nsImageXlib::DrawComposited(nsIRenderingContext &aContext,
                            nsDrawingSurface aSurface,
                            PRInt32 aSX, PRInt32 aSY,
                            PRInt32 aSWidth, PRInt32 aSHeight,
                            PRInt32 aDX, PRInt32 aDY,
                            PRInt32 aDWidth, PRInt32 aDHeight)
{
  if ((aDWidth==0) || (aDHeight==0))
    return;

  nsIDrawingSurfaceXlib *drawing = NS_STATIC_CAST(nsIDrawingSurfaceXlib *, aSurface);
  Drawable drawable; drawing->GetDrawable(drawable);
  Visual  *visual   = xxlib_rgb_get_visual(mXlibRgbHandle);

  // I hate clipping... too!
  PRUint32 surfaceWidth, surfaceHeight;
  drawing->GetDimensions(&surfaceWidth, &surfaceHeight);

  int readX, readY;
  unsigned readWidth, readHeight, destX, destY;

  if ((aDY >= (int)surfaceHeight) || (aDX >= (int)surfaceWidth) ||
      (aDY + aDHeight <= 0) || (aDX + aDWidth <= 0)) {
    // This should never happen if the layout engine is sane,
    // as it means we're trying to draw an image which is outside
    // the drawing surface.  Bulletproof gfx for now...
    return;
  }

  if (aDX < 0) {
    readX = 0;   readWidth = aDWidth + aDX;    destX = aSX - aDX;
  } else {
    readX = aDX;  readWidth = aDWidth;       destX = aSX;
  }
  if (aDY < 0) {
    readY = 0;   readHeight = aDHeight + aDY;  destY = aSY - aDY;
  } else { 
    readY = aDY;  readHeight = aDHeight;     destY = aSY;
  }

  if (readX+readWidth > surfaceWidth)
  readWidth = surfaceWidth-readX;                                             
  if (readY+readHeight > surfaceHeight)
    readHeight = surfaceHeight-readY;

  if ((readHeight <= 0) || (readWidth <= 0))
    return;

  //  fprintf(stderr, "aX=%d aY=%d, aWidth=%u aHeight=%u\n", aX, aY, aWidth, aHeight);
  //  fprintf(stderr, "surfaceWidth=%u surfaceHeight=%u\n", surfaceWidth, surfaceHeight);
  //  fprintf(stderr, "readX=%u readY=%u readWidth=%u readHeight=%u destX=%u destY=%u\n\n",
  //          readX, readY, readWidth, readHeight, destX, destY);

  XImage *ximage = XGetImage(mDisplay, drawable,
                             readX, readY, readWidth, readHeight,
                             AllPlanes, ZPixmap);

  NS_ASSERTION((ximage != NULL), "XGetImage() failed");
  if (!ximage)
    return;

  unsigned char *readData = 
    (unsigned char *)nsMemory::Alloc(3*readWidth*readHeight);

  PRUint8 *scaledImage = 0;
  PRUint8 *scaledAlpha = 0;
  PRUint8 *imageOrigin, *alphaOrigin;
  PRUint32 imageStride, alphaStride;

  /* image needs to be scaled */
  if ((aSWidth!=aDWidth) || (aSHeight!=aDHeight)) {
    PRUint32 x1, y1, x2, y2;
    x1 = (destX*aSWidth)/aDWidth;
    y1 = (destY*aSHeight)/aDHeight;
    x2 = ((destX+readWidth)*aSWidth)/aDWidth;
    y2 = ((destY+readHeight)*aSHeight)/aDHeight;

    scaledImage = (PRUint8 *)nsMemory::Alloc(3*aDWidth*aDHeight);
    scaledAlpha = (PRUint8 *)nsMemory::Alloc(aDWidth*aDHeight);
    if (!scaledImage || !scaledAlpha) {
      XDestroyImage(ximage);
      nsMemory::Free(readData);
      if (scaledImage)
        nsMemory::Free(scaledImage);
      if (scaledAlpha)
        nsMemory::Free(scaledAlpha);
      return;
    }
    RectStretch(x1, y1, x2-1, y2-1,
                0, 0, readWidth-1, readHeight-1,
                mImageBits, mRowBytes, scaledImage, 3*readWidth, 24);
    RectStretch(x1, y1, x2-1, y2-1,
                0, 0, readWidth-1, readHeight-1,
                mAlphaBits, mAlphaRowBytes, scaledAlpha, readWidth, 8);
    imageOrigin = scaledImage;
    imageStride = 3*readWidth;
    alphaOrigin = scaledAlpha;
    alphaStride = readWidth;
  } else {
    imageOrigin = mImageBits + destY*mRowBytes + 3*destX;
    imageStride = mRowBytes;
    alphaOrigin = mAlphaBits + destY*mAlphaRowBytes + destX;
    alphaStride = mAlphaRowBytes;
  }

  PRBool isLSB;
  unsigned int test = 1;
  isLSB = (((char *)&test)[0]) ? 1 : 0;
  int red_prec   = xxlib_get_prec_from_mask(visual->red_mask);
  int green_prec = xxlib_get_prec_from_mask(visual->green_mask);
  int blue_prec  = xxlib_get_prec_from_mask(visual->blue_mask);
  

  PRBool flipBytes =
    ( isLSB && ximage->byte_order != LSBFirst) ||
    (!isLSB && ximage->byte_order == LSBFirst);

  if ((ximage->bits_per_pixel==32) &&
      (red_prec == 8) &&
      (green_prec == 8) &&
      (blue_prec == 8))
    DrawComposited32(isLSB, flipBytes, 
                     imageOrigin, imageStride,
                     alphaOrigin, alphaStride, 
                     readWidth, readHeight, ximage, readData);
  else if ((ximage->bits_per_pixel==24) &&
      (red_prec == 8) &&
      (green_prec == 8) &&
      (blue_prec == 8))
    DrawComposited24(isLSB, flipBytes, 
                     imageOrigin, imageStride,
                     alphaOrigin, alphaStride, 
                     readWidth, readHeight, ximage, readData);
  else if ((ximage->bits_per_pixel==16) &&
           ((red_prec == 5)   || (red_prec == 6)) &&
           ((green_prec == 5) || (green_prec == 6)) &&
           ((blue_prec == 5)  || (blue_prec == 6)))
    DrawComposited16(isLSB, flipBytes,
                     imageOrigin, imageStride,
                     alphaOrigin, alphaStride, 
                     readWidth, readHeight, ximage, readData);
  else
    DrawCompositedGeneral(isLSB, flipBytes,
                     imageOrigin, imageStride,
                     alphaOrigin, alphaStride, 
                     readWidth, readHeight, ximage, readData);

  xGC *imageGC = ((nsRenderingContextXlib&)aContext).GetGC();
  xxlib_draw_rgb_image(mXlibRgbHandle, drawable, *imageGC,
                       readX, readY, readWidth, readHeight,
                       XLIB_RGB_DITHER_MAX,
                       readData, 3*readWidth);
  XDestroyImage(ximage);
  imageGC->Release();
  nsMemory::Free(readData);
  if (scaledImage)
    nsMemory::Free(scaledImage);
  if (scaledAlpha)
    nsMemory::Free(scaledAlpha);
  mFlags = 0;
}

void nsImageXlib::CreateAlphaBitmap(PRInt32 aWidth, PRInt32 aHeight)
{
  XImage *x_image = nsnull;
  XGCValues gcv;

  /* Create gc clip-mask on demand */
  if (mAlphaBits && IsFlagSet(nsImageUpdateFlags_kBitsChanged, mFlags)) {

    if (!mAlphaPixmap)
      mAlphaPixmap = XCreatePixmap(mDisplay, DefaultRootWindow(mDisplay),
                                   aWidth, aHeight, 1);

    // Make an image out of the alpha-bits created by the image library
    x_image = XCreateImage(mDisplay, xxlib_rgb_get_visual(mXlibRgbHandle),
                           1, /* visual depth...1 for bitmaps */
                           XYPixmap,
                           0, /* x offset, XXX fix this */
                           (char *)mAlphaBits,  /* cast away our sign. */
                           aWidth,
                           aHeight,
                           32, /* bitmap pad */
                           mAlphaRowBytes); /* bytes per line */

    x_image->bits_per_pixel=1;

    /* Image library always places pixels left-to-right MSB to LSB */
    x_image->bitmap_bit_order = MSBFirst;

    /* This definition doesn't depend on client byte ordering
       because the image library ensures that the bytes in
       bitmask data are arranged left to right on the screen,
       low to high address in memory. */
    x_image->byte_order = MSBFirst;
#if defined(IS_LITTLE_ENDIAN)
    // no, it's still MSB XXX check on this!!
    //      x_image->byte_order = LSBFirst;
#elif defined (IS_BIG_ENDIAN)
    x_image->byte_order = MSBFirst;
#else
#error ERROR! Endianness is unknown;
#endif

    /* Copy the XImage to mAlphaPixmap */
    if (!s1bitGC) {
      memset(&gcv, 0, sizeof(XGCValues));
      gcv.function = GXcopy;
      s1bitGC = XCreateGC(mDisplay, mAlphaPixmap, GCFunction, &gcv);
    }

    XPutImage(mDisplay, mAlphaPixmap, s1bitGC, x_image, 0, 0, 0, 0,
              aWidth, aHeight);

    /* Now we are done with the temporary image */
    x_image->data = 0;          /* Don't free the IL_Pixmap's bits. */
    XDestroyImage(x_image);
  }
}

void nsImageXlib::CreateOffscreenPixmap(PRInt32 aWidth, PRInt32 aHeight)
{
  if (mImagePixmap == nsnull) {
    mImagePixmap = XCreatePixmap(mDisplay, XDefaultRootWindow(mDisplay),
                                 aWidth, aHeight,
                                 xxlib_rgb_get_depth(mXlibRgbHandle));
  }
}

void nsImageXlib::SetupGCForAlpha(GC aGC, PRInt32 aX, PRInt32 aY)
{
  if (mAlphaPixmap)
  {
    XGCValues xvalues;
    memset(&xvalues, 0, sizeof(XGCValues));
    unsigned long xvalues_mask = 0;
    xvalues.clip_x_origin = aX;
    xvalues.clip_y_origin = aY;
    xvalues_mask = GCClipXOrigin | GCClipYOrigin | GCClipMask;
    xvalues.function = GXcopy;
    xvalues.clip_mask = mAlphaPixmap;

    XChangeGC(mDisplay, aGC, xvalues_mask, &xvalues);
  }
}

// Draw the bitmap. This draw just has destination coordinates
NS_IMETHODIMP
nsImageXlib::Draw(nsIRenderingContext &aContext,
                  nsDrawingSurface aSurface,
                  PRInt32 aX, PRInt32 aY,
                  PRInt32 aWidth, PRInt32 aHeight)
{
  if (mPendingUpdate)
    UpdateCachedImage();

  if ((mAlphaDepth == 1) && mIsSpacer)
    return NS_OK;

  if (aSurface == nsnull)
    return NS_ERROR_FAILURE;

  if ((mAlphaDepth == 8) && mAlphaValid) {
    DrawComposited(aContext, aSurface,
        0, 0, aWidth, aHeight,
        aX, aY, aWidth, aHeight);
    return NS_OK;
  }

  // XXX it is said that this is temporary code
  if ((aWidth != mWidth) || (aHeight != mHeight)) {
    aWidth = mWidth;
    aHeight = mHeight;
  }

  nsIDrawingSurfaceXlib *drawing = NS_STATIC_CAST(nsIDrawingSurfaceXlib *, aSurface);
  
  PRInt32
    validX = 0,
    validY = 0,
    validWidth = aWidth,
    validHeight = aHeight;

  if ((mDecodedY2 < aHeight)) {
    validHeight = mDecodedY2 - mDecodedY1;
  }
  if ((mDecodedX2 < aWidth)) {
    validWidth = mDecodedX2 - mDecodedX1;
  }
  if ((mDecodedY1 > 0)) {
    validHeight -= mDecodedY1;
    validY = mDecodedY1;
  }
  if ((mDecodedX1 > 0)) {
    validHeight -= mDecodedX1;
    validX = mDecodedX1;
  }

  CreateAlphaBitmap(aWidth, aHeight);

  GC copyGC;
  xGC *gc = ((nsRenderingContextXlib&)aContext).GetGC();

  if (mAlphaPixmap) {
    if (mGC) {                /* reuse GC */
      copyGC = mGC;
    SetupGCForAlpha(copyGC, aX, aY);
    } else {                  /* make a new one */
      /* this repeats things done in SetupGCForAlpha */
      XGCValues xvalues;
      memset(&xvalues, 0, sizeof(XGCValues));
      unsigned long xvalues_mask = 0;
      xvalues.clip_x_origin = aX;
      xvalues.clip_y_origin = aY;
      if (IsFlagSet(nsImageUpdateFlags_kBitsChanged, mFlags)) {
        xvalues_mask = GCClipXOrigin | GCClipYOrigin | GCClipMask;
        xvalues.clip_mask = mAlphaPixmap;
      }
      Drawable drawable; drawing->GetDrawable(drawable);
      mGC = XCreateGC(mDisplay, drawable, xvalues_mask , &xvalues);
      copyGC = mGC;
    }
  } else {  /* !mAlphaPixmap */
    copyGC = *gc;
  }

  Drawable drawable; drawing->GetDrawable(drawable);
  XCopyArea(mDisplay, mImagePixmap, drawable,
            copyGC, validX, validY,
            validWidth, validHeight,
            validX + aX, validY + aY);

  gc->Release();

  mFlags = 0;
  return NS_OK;
}

void nsImageXlib::TilePixmap(Pixmap src, Pixmap dest, PRInt32 aSXOffset,
                             PRInt32 aSYOffset, const nsRect &destRect,
                             const nsRect &clipRect, PRBool useClip)
{
  GC gc;
  XGCValues values;
  unsigned long valuesMask;
  memset(&values, 0, sizeof(XGCValues));
  values.fill_style = FillTiled;
  values.tile = src;
  values.ts_x_origin = destRect.x - aSXOffset;
  values.ts_y_origin = destRect.y - aSYOffset;
  valuesMask = GCTile | GCTileStipXOrigin | GCTileStipYOrigin | GCFillStyle;
  gc = XCreateGC(mDisplay, src, valuesMask, &values);

  if (useClip) {
    XRectangle xrectangle;
    xrectangle.x = clipRect.x;
    xrectangle.y = clipRect.y;
    xrectangle.width = clipRect.width;
    xrectangle.height = clipRect.height;
    XSetClipRectangles(mDisplay, gc, 0, 0, &xrectangle, 1, Unsorted);
  }

  XFillRectangle(mDisplay, dest, gc, destRect.x, destRect.y,
                 destRect.width, destRect.height);

  XFreeGC(mDisplay, gc);
}

NS_IMETHODIMP nsImageXlib::DrawTile(nsIRenderingContext &aContext,
                                    nsDrawingSurface aSurface,
                                    PRInt32 aSXOffset, PRInt32 aSYOffset,
                                    const nsRect &aTileRect)
{
  if (mPendingUpdate)
    UpdateCachedImage();

  if ((mAlphaDepth == 1) && mIsSpacer)
    return NS_OK;

  if (aTileRect.width <= 0 || aTileRect.height <= 0) {
    return NS_OK;
  }
  
  nsIDrawingSurfaceXlib *drawing = NS_STATIC_CAST(nsIDrawingSurfaceXlib *, aSurface);

  PRBool partial = PR_FALSE;

  PRInt32
    validX = 0,
    validY = 0,
    validWidth = mWidth,
    validHeight = mHeight;

  // limit the image rectangle to the size of the image data which
  // has been validated.
  if (mDecodedY2 < mHeight) {
    validHeight = mDecodedY2 - mDecodedY1;
    partial = PR_TRUE;
  }
  if (mDecodedX2 < mWidth) {
    validWidth = mDecodedX2 - mDecodedX1;
    partial = PR_TRUE;
  }
  if (mDecodedY1 > 0) {   
    validHeight -= mDecodedY1;
    validY = mDecodedY1;
    partial = PR_TRUE;
  }
  if (mDecodedX1 > 0) {
    validWidth -= mDecodedX1;
    validX = mDecodedX1;
    partial = PR_TRUE;
  }
  
  if (validWidth == 0 || validHeight == 0) {
    return NS_OK;
  }
   
  if (partial || ((mAlphaDepth == 8) && mAlphaValid)) {
    PRInt32 aY0 = aTileRect.y - aSYOffset,
            aX0 = aTileRect.x - aSXOffset,
            aY1 = aTileRect.y + aTileRect.height,
            aX1 = aTileRect.x + aTileRect.width;

    // Set up clipping and call Draw().
    PRBool clipState;
    aContext.PushState();
    aContext.SetClipRect(aTileRect, nsClipCombine_kIntersect,
                         clipState);

    for (PRInt32 y = aY0; y < aY1; y += mHeight)
      for (PRInt32 x = aX0; x < aX1; x += mWidth)
        Draw(aContext,aSurface, x, y,
             PR_MIN(validWidth, aX1 - x),
             PR_MIN(validHeight, aY1 - y));

    aContext.PopState(clipState);

    return NS_OK;
  }

  CreateOffscreenPixmap(mWidth, mHeight);

  if (mAlphaDepth == 1) {
    Pixmap tileImg;
    Pixmap tileMask;

    CreateAlphaBitmap(validWidth, validHeight);

    nsRect tmpRect(0,0,aTileRect.width, aTileRect.height);

    XlibRgbHandle *drawingXHandle; 
    drawing->GetXlibRgbHandle(drawingXHandle);
    tileImg = XCreatePixmap(mDisplay, mImagePixmap,
                            aTileRect.width, aTileRect.height,
                            xxlib_rgb_get_depth(drawingXHandle));
    TilePixmap(mImagePixmap, tileImg, aSXOffset, aSYOffset, tmpRect,
               tmpRect, PR_FALSE);

    // tile alpha mask
    tileMask = XCreatePixmap(mDisplay, mAlphaPixmap,
                             aTileRect.width, aTileRect.height, mAlphaDepth);
    TilePixmap(mAlphaPixmap, tileMask, aSXOffset, aSYOffset, tmpRect,
               tmpRect, PR_FALSE);

    GC fgc;
    XGCValues values;
    unsigned long valuesMask;

    Drawable drawable; drawing->GetDrawable(drawable);
    memset(&values, 0, sizeof(XGCValues));
    values.clip_mask = tileMask;
    values.clip_x_origin = aTileRect.x;
    values.clip_y_origin = aTileRect.y;
    valuesMask = GCClipXOrigin | GCClipYOrigin | GCClipMask;
    fgc = XCreateGC(mDisplay, drawable, valuesMask, &values);

    XCopyArea(mDisplay, tileImg, drawable,
              fgc, 0,0,
              aTileRect.width, aTileRect.height,
              aTileRect.x, aTileRect.y);

    XFreePixmap(mDisplay, tileImg);
    XFreePixmap(mDisplay, tileMask);
    XFreeGC(mDisplay, fgc);

  } else {
    // In the non-alpha case, xlib can tile for us
    nsRect clipRect;
    PRBool isValid;

    aContext.GetClipRect(clipRect, isValid);

    Drawable drawable; drawing->GetDrawable(drawable);
    TilePixmap(mImagePixmap, drawable, aSXOffset, aSYOffset,
               aTileRect, clipRect, PR_FALSE);
  }

  mFlags = 0;
  return NS_OK;
}


//----------------------------------------------------------------------
nsresult nsImageXlib::Optimize(nsIDeviceContext *aContext)
{
  return NS_OK;
}

//----------------------------------------------------------------------
// Lock the image pixels.
NS_IMETHODIMP
nsImageXlib::LockImagePixels(PRBool aMaskPixels)
{
  return NS_OK;
}

//---------------------------------------------------------------------
// unlock the image pixels. Implement this if you need it.
NS_IMETHODIMP
nsImageXlib::UnlockImagePixels(PRBool aMaskPixels)
{
  return NS_OK;
}

//---------------------------------------------------------------------
// Set the decoded dimens of the image
NS_IMETHODIMP
nsImageXlib::SetDecodedRect(PRInt32 x1, PRInt32 y1, PRInt32 x2, PRInt32 y2)
{
  mDecodedX1 = x1;
  mDecodedY1 = y1;
  mDecodedX2 = x2;
  mDecodedY2 = y2;

  return NS_OK;
}

NS_IMETHODIMP nsImageXlib::DrawToImage(nsIImage* aDstImage,
                                       nscoord aDX, nscoord aDY,
                                       nscoord aDWidth, nscoord aDHeight)
{
  nsImageXlib *dest = NS_STATIC_CAST(nsImageXlib *, aDstImage);
  if (!dest)
    return NS_ERROR_FAILURE;

  if (mPendingUpdate)
    UpdateCachedImage();

  if (!dest->mImagePixmap)
    dest->CreateOffscreenPixmap(dest->mWidth, dest->mHeight);
  
  if (!dest->mImagePixmap || !mImagePixmap)
    return NS_ERROR_FAILURE;

  GC gc = XCreateGC(mDisplay, dest->mImagePixmap, 0, NULL);

  if (mAlphaDepth == 1)
    CreateAlphaBitmap(mWidth, mHeight);
  
  if (mAlphaPixmap)
    SetupGCForAlpha(gc, aDX, aDY);

  XCopyArea(dest->mDisplay, mImagePixmap, dest->mImagePixmap, gc,
            0, 0, mWidth, mHeight, aDX, aDY);

  XFreeGC(mDisplay, gc);

  if (!mIsSpacer || !mAlphaDepth)
    dest->mIsSpacer = PR_FALSE;

  // need to copy the mImageBits in case we're rendered scaled
  PRUint8 *scaledImage = 0, *scaledAlpha = 0;
  PRUint8 *rgbPtr=0, *alphaPtr=0;
  PRUint32 rgbStride, alphaStride = 0;

  if ((aDWidth != mWidth) || (aDHeight != mHeight)) {
    // scale factor in DrawTo... start scaling
    scaledImage = (PRUint8 *)nsMemory::Alloc(3*aDWidth*aDHeight);
    if (!scaledImage)
      return NS_ERROR_OUT_OF_MEMORY;

    RectStretch(0, 0, mWidth-1, mHeight-1, 0, 0, aDWidth-1, aDHeight-1,
                mImageBits, mRowBytes, scaledImage, 3*aDWidth, 24);

    if (mAlphaDepth) {
      if (mAlphaDepth==1)
        alphaStride = (aDWidth+7)>>3;    // round to next byte
      else
        alphaStride = aDWidth;

      scaledAlpha = (PRUint8 *)nsMemory::Alloc(alphaStride*aDHeight);
      if (!scaledAlpha) {
        nsMemory::Free(scaledImage);
        return NS_ERROR_OUT_OF_MEMORY;
      }

      RectStretch(0, 0, mWidth-1, mHeight-1, 0, 0, aDWidth-1, aDHeight-1,
                  mAlphaBits, mAlphaRowBytes, scaledAlpha, alphaStride,
                  mAlphaDepth);
    }
    rgbPtr = scaledImage;
    rgbStride = 3*aDWidth;
    alphaPtr = scaledAlpha;
  } else {
    rgbPtr = mImageBits;
    rgbStride = mRowBytes;
    alphaPtr = mAlphaBits;
    alphaStride = mAlphaRowBytes;
  }

  PRInt32 y;
  // now composite the two images together
  switch (mAlphaDepth) {
  case 1:
    for (y=0; y<aDHeight; y++) {
      PRUint8 *dst = dest->mImageBits + (y+aDY)*dest->mRowBytes + 3*aDX;
      PRUint8 *dstAlpha = dest->mAlphaBits + (y+aDY)*dest->mAlphaRowBytes;
      PRUint8 *src = rgbPtr + y*rgbStride; 
      PRUint8 *alpha = alphaPtr + y*alphaStride;
      for (int x=0; x<aDWidth; x++, dst+=3, src+=3) {
#define NS_GET_BIT(rowptr, x) (rowptr[(x)>>3] &  (1<<(7-(x)&0x7)))
#define NS_SET_BIT(rowptr, x) (rowptr[(x)>>3] |= (1<<(7-(x)&0x7)))

        // if this pixel is opaque then copy into the destination image
        if (NS_GET_BIT(alpha, x)) {
          dst[0] = src[0];
          dst[1] = src[1];
          dst[2] = src[2];
          NS_SET_BIT(dstAlpha, aDX+x);
        }

#undef NS_GET_BIT
#undef NS_SET_BIT
      }
    }
    break;
  case 8:
    for (y=0; y<aDHeight; y++) {
      PRUint8 *dst = dest->mImageBits + (y+aDY)*dest->mRowBytes + 3*aDX;
      PRUint8 *dstAlpha = 
        dest->mAlphaBits + (y+aDY)*dest->mAlphaRowBytes + aDX;
      PRUint8 *src = rgbPtr + y*rgbStride; 
      PRUint8 *alpha = alphaPtr + y*alphaStride;
      for (int x=0; x<aDWidth; x++, dst+=3, dstAlpha++, src+=3, alpha++) {

        // blend this pixel over the destination image
        unsigned val = *alpha;
        MOZ_BLEND(dst[0], dst[0], src[0], val);
        MOZ_BLEND(dst[1], dst[1], src[1], val);
        MOZ_BLEND(dst[2], dst[2], src[2], val);
        MOZ_BLEND(*dstAlpha, *dstAlpha, val, val);
      }
    }
    break;
  case 0:
  default:
    for (y=0; y<aDHeight; y++)
      memcpy(dest->mImageBits + (y+aDY)*dest->mRowBytes + 3*aDX, 
             rgbPtr + y*rgbStride,
             3*aDWidth);
  }
  if (scaledAlpha)
    nsMemory::Free(scaledAlpha);
  if (scaledImage)
    nsMemory::Free(scaledImage);

  return NS_OK;
}


