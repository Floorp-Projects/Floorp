/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 *    Stuart Parmenter <pavlov@netscape.com>
 *    Tim Rowley <tor@cs.brown.edu> -- 8bit alpha compositing
 */
#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include "imgScaler.h"

#include "nsImageGTK.h"
#include "nsRenderingContextGTK.h"

#include "nspr.h"

#define IsFlagSet(a,b) ((a) & (b))

#define NS_GET_BIT(rowptr, x) (rowptr[(x)>>3] &  (1<<(7-(x)&0x7)))
#define NS_SET_BIT(rowptr, x) (rowptr[(x)>>3] |= (1<<(7-(x)&0x7)))

// Defining this will trace the allocation of images.  This includes
// ctor, dtor and update.
//#define TRACE_IMAGE_ALLOCATION

//#define CHEAP_PERFORMANCE_MEASURMENT 1

// Define this to see tiling debug output
//#define DEBUG_TILING

/* XXX we are simply creating a GC and setting its function to Copy.
   we shouldn't be doing this every time this method is called.  this creates
   way more trips to the server than we should be doing so we are creating a
   static one.
*/
static GdkGC *s1bitGC = nsnull;
static GdkGC *sXbitGC = nsnull;

NS_IMPL_ISUPPORTS1(nsImageGTK, nsIImage)

//------------------------------------------------------------

nsImageGTK::nsImageGTK()
{
  NS_INIT_REFCNT();
  mImageBits = nsnull;
  mWidth = 0;
  mHeight = 0;
  mDepth = 0;
  mAlphaBits = mTrueAlphaBits = nsnull;
  mAlphaPixmap = nsnull;
  mImagePixmap = nsnull;
  mAlphaDepth = mTrueAlphaDepth = 0;
  mRowBytes = 0;
  mSizeImage = 0;
  mAlphaHeight = 0;
  mAlphaWidth = 0;
  mNaturalWidth = 0;
  mNaturalHeight = 0;
  mIsSpacer = PR_TRUE;
  mPendingUpdate = PR_FALSE;

#ifdef TRACE_IMAGE_ALLOCATION
  printf("nsImageGTK::nsImageGTK(this=%p)\n",
         this);
#endif
}

//------------------------------------------------------------

nsImageGTK::~nsImageGTK()
{
  if(nsnull != mImageBits) {
    delete[] mImageBits;
    mImageBits = nsnull;
  }

  if (nsnull != mAlphaBits) {
    delete[] mAlphaBits;
    mAlphaBits = nsnull;
  }

  if (nsnull != mTrueAlphaBits) {
    delete[] mTrueAlphaBits;
    mTrueAlphaBits = nsnull;
  }

  if (mAlphaPixmap) {
    gdk_pixmap_unref(mAlphaPixmap);
  }

  if (mImagePixmap) {
    gdk_pixmap_unref(mImagePixmap);
  }

#ifdef TRACE_IMAGE_ALLOCATION
  printf("nsImageGTK::~nsImageGTK(this=%p)\n",
         this);
#endif
}

//------------------------------------------------------------

nsresult nsImageGTK::Init(PRInt32 aWidth, PRInt32 aHeight,
                          PRInt32 aDepth, nsMaskRequirements aMaskRequirements)
{
  g_return_val_if_fail ((aWidth != 0) || (aHeight != 0), NS_ERROR_FAILURE);

  if (nsnull != mImageBits) {
   delete[] mImageBits;
   mImageBits = nsnull;
  }

  if (nsnull != mAlphaBits) {
    delete[] mAlphaBits;
    mAlphaBits = nsnull;
  }

  if (nsnull != mTrueAlphaBits) {
    delete[] mTrueAlphaBits;
    mTrueAlphaBits = nsnull;
  }

  if (nsnull != mAlphaPixmap) {
    gdk_pixmap_unref(mAlphaPixmap);
    mAlphaPixmap = nsnull;
  }

  SetDecodedRect(0,0,0,0);  //init
  SetNaturalWidth(0);
  SetNaturalHeight(0);

  // mImagePixmap gets created once per unique image bits in Draw()
  // ImageUpdated(nsImageUpdateFlags_kBitsChanged) can cause the
  // image bits to change and mImagePixmap will be unrefed and nulled.
  if (nsnull != mImagePixmap) {
    gdk_pixmap_unref(mImagePixmap);
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

#ifdef TRACE_IMAGE_ALLOCATION
  printf("nsImageGTK::Init(this=%p,%d,%d,%d,%d)\n",
         this,
         aWidth,
         aHeight,
         aDepth,
         aMaskRequirements);
#endif

  // create the memory for the image
  ComputeMetrics();

  mImageBits = (PRUint8*) new PRUint8[mSizeImage];

  switch(aMaskRequirements)
  {
    case nsMaskRequirements_kNoMask:
      mAlphaBits = nsnull;
      mAlphaWidth = 0;
      mAlphaHeight = 0;
      break;

    case nsMaskRequirements_kNeeds8Bit:
      mTrueAlphaRowBytes = aWidth;
      mTrueAlphaDepth = 8;

      // 32-bit align each row
      mTrueAlphaRowBytes = (mTrueAlphaRowBytes + 3) & ~0x3;
      mTrueAlphaBits = new PRUint8[mTrueAlphaRowBytes * aHeight];

      // FALL THROUGH

    case nsMaskRequirements_kNeeds1Bit:
      mAlphaRowBytes = (aWidth + 7) / 8;
      mAlphaDepth = 1;

      // 32-bit align each row
      mAlphaRowBytes = (mAlphaRowBytes + 3) & ~0x3;

      mAlphaBits = new PRUint8[mAlphaRowBytes * aHeight];
      memset(mAlphaBits, 0, mAlphaRowBytes*aHeight);
      mAlphaWidth = aWidth;
      mAlphaHeight = aHeight;
      break;
  }

  if (aMaskRequirements == nsMaskRequirements_kNeeds8Bit)
    mAlphaDepth = 0;

  return NS_OK;
}

//------------------------------------------------------------

PRInt32 nsImageGTK::GetHeight()
{
  return mHeight;
}

PRInt32 nsImageGTK::GetWidth()
{
  return mWidth;
}

PRUint8 *nsImageGTK::GetBits()
{
  return mImageBits;
}

void *nsImageGTK::GetBitInfo()
{
  return nsnull;
}

PRInt32 nsImageGTK::GetLineStride()
{
  return mRowBytes;
}

nsColorMap *nsImageGTK::GetColorMap()
{
  return nsnull;
}

PRBool nsImageGTK::IsOptimized()
{
  return PR_TRUE;
}

PRUint8 *nsImageGTK::GetAlphaBits()
{
  if (mTrueAlphaBits)
    return mTrueAlphaBits;
  else
    return mAlphaBits;
}

PRInt32 nsImageGTK::GetAlphaWidth()
{
  return mAlphaWidth;
}

PRInt32 nsImageGTK::GetAlphaHeight()
{
  return mAlphaHeight;
}

PRInt32
nsImageGTK::GetAlphaLineStride()
{
  if (mTrueAlphaBits)
    return mTrueAlphaRowBytes;
  else
    return mAlphaRowBytes;
}

nsIImage *nsImageGTK::DuplicateImage()
{
  return nsnull;
}

void nsImageGTK::SetAlphaLevel(PRInt32 aAlphaLevel)
{
}

PRInt32 nsImageGTK::GetAlphaLevel()
{
  return 0;
}

void nsImageGTK::MoveAlphaMask(PRInt32 aX, PRInt32 aY)
{
}

void nsImageGTK::ImageUpdated(nsIDeviceContext *aContext,
                              PRUint8 aFlags,
                              nsRect *aUpdateRect)
{
  mPendingUpdate = PR_TRUE;
  mUpdateRegion.Or(mUpdateRegion, *aUpdateRect);
}

void nsImageGTK::UpdateCachedImage()
{
#ifdef TRACE_IMAGE_ALLOCATION
  printf("nsImageGTK::ImageUpdated(this=%p,%d)\n",
         this,
         aFlags);
#endif

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
    if ((mTrueAlphaDepth==8) && (mAlphaDepth<mTrueAlphaDepth)) {
      for (unsigned y=rect->y; 
           (y<bottom) && (mAlphaDepth<mTrueAlphaDepth); 
           y++) {
        unsigned char *alpha = mTrueAlphaBits + mTrueAlphaRowBytes*y + left;
        unsigned char *mask = mAlphaBits + mAlphaRowBytes*y;
        for (unsigned x=left; x<right; x++) {
          switch (*(alpha++)) {
          case 255:
            NS_SET_BIT(mask,x);
            break;
          case 0:
            if (mAlphaDepth != 8)
              mAlphaDepth=1;
            break;
          default:
            mAlphaDepth=8;
            break;
          }
        }
      }
      
      if (mAlphaDepth==8) {
        if (mImagePixmap) {
          gdk_pixmap_unref(mImagePixmap);
          mImagePixmap = 0;
        }
        if (mAlphaPixmap) {
          gdk_pixmap_unref(mAlphaPixmap);
          mAlphaPixmap = 0;
        }
        if (mAlphaBits) {
          delete [] mAlphaBits;
          mAlphaBits = mTrueAlphaBits;
          mAlphaRowBytes = mTrueAlphaRowBytes;
          mTrueAlphaBits = 0;
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

    if (mAlphaDepth != 8) {
      CreateOffscreenPixmap(mWidth, mHeight);
      if (!sXbitGC)
        sXbitGC = gdk_gc_new(mImagePixmap);

      gdk_draw_rgb_image_dithalign(mImagePixmap, sXbitGC, 
                                   rect->x, rect->y,
                                   rect->width, rect->height,
                                   GDK_RGB_DITHER_MAX,
                                   mImageBits + mRowBytes*rect->y + 3*rect->x,
                                   mRowBytes,
                                   rect->x, rect->y);
    }
  }
  
  mUpdateRegion.Empty();
  mPendingUpdate = PR_FALSE;
  mFlags = nsImageUpdateFlags_kBitsChanged; // this should be 0'd out by Draw()
}

#ifdef CHEAP_PERFORMANCE_MEASURMENT
static PRTime gConvertTime, gAlphaTime, gCopyStart, gCopyEnd, gStartTime, gPixmapTime, gEndTime;
#endif


NS_IMETHODIMP
nsImageGTK::DrawScaled(nsIRenderingContext &aContext,
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

  nsDrawingSurfaceGTK *drawing = (nsDrawingSurfaceGTK*)aSurface;

  if (mAlphaDepth == 1) {
    CreateAlphaBitmap(mWidth, mHeight);
  }

  if (mAlphaDepth==8) {
    DrawComposited(aContext, aSurface, 
                   aSX, aSY, aSWidth, aSHeight, 
                   aDX, aDY, aDWidth, aDHeight);
    return NS_OK;
  }

  GdkGC *gc;
  GdkPixmap *pixmap = 0;

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
    
      pixmap = gdk_pixmap_new(nsnull, aDWidth, aDHeight, 1);
      XImage *ximage = 0;

      if (pixmap) {
        ximage = XCreateImage(GDK_WINDOW_XDISPLAY(pixmap),
                              GDK_VISUAL_XVISUAL(gdk_rgb_get_visual()),
                              1, XYPixmap, 0, (char *)scaledAlpha, 
                              aDWidth, aDHeight,
                              8, scaledRowBytes);
      }
      if (ximage) {
        ximage->bits_per_pixel=1;
        ximage->bitmap_bit_order=MSBFirst;
        ximage->byte_order = MSBFirst;
        
        GdkGC *tmpGC = gdk_gc_new(pixmap);
        if (tmpGC) {
          XPutImage(GDK_WINDOW_XDISPLAY(pixmap), GDK_WINDOW_XWINDOW(pixmap),
                    GDK_GC_XGC(tmpGC), ximage,
                    0, 0, 0, 0, aDWidth, aDHeight);
          gdk_gc_unref(tmpGC);
        } else {
          // can't write into the clip mask - destroy so we don't use it
          if (pixmap)
            gdk_pixmap_unref(pixmap);
          pixmap = 0;
        }
        
        ximage->data = 0;
        XDestroyImage(ximage);
      }

      nsMemory::Free(scaledAlpha);
    }
  }

  if (pixmap) {
    gc = gdk_gc_new(drawing->GetDrawable());
    gdk_gc_set_clip_origin(gc, aDX, aDY);
    gdk_gc_set_clip_mask(gc, pixmap);
  } else {
    // don't make a copy... we promise not to change it
    gc = ((nsRenderingContextGTK&)aContext).GetGC();
  }    
  
  PRUint8 *scaledRGB = (PRUint8 *)nsMemory::Alloc(3*aDWidth*aDHeight);
  if (scaledRGB && gc) {
    RectStretch(aSX, aSY, aSX+aSWidth-1, aSY+aSHeight-1,
                0, 0, aDWidth-1, aDHeight-1,
                mImageBits, mRowBytes, scaledRGB, 3*aDWidth, 24);
    
    gdk_draw_rgb_image(drawing->GetDrawable(), gc,
                       aDX, aDY, aDWidth, aDHeight,
                       GDK_RGB_DITHER_MAX,
                       scaledRGB, 3*aDWidth);

    nsMemory::Free(scaledRGB);
  }

  if (gc)
    gdk_gc_unref(gc);
  if (pixmap)
    gdk_pixmap_unref(pixmap);

  mFlags = 0;

  return NS_OK;
}

// Draw the bitmap, this method has a source and destination coordinates
NS_IMETHODIMP
nsImageGTK::Draw(nsIRenderingContext &aContext, nsDrawingSurface aSurface,
                 PRInt32 aSX, PRInt32 aSY, PRInt32 aSWidth, PRInt32 aSHeight,
                 PRInt32 aDX, PRInt32 aDY, PRInt32 aDWidth, PRInt32 aDHeight)
{
  g_return_val_if_fail ((aSurface != nsnull), NS_ERROR_FAILURE);

  if (mPendingUpdate)
    UpdateCachedImage();

  if ((mAlphaDepth==1) && mIsSpacer)
    return NS_OK;

#ifdef TRACE_IMAGE_ALLOCATION
  printf("nsImageGTK::Draw(this=%p) (%d, %d, %d, %d), (%d, %d, %d, %d)\n",
         this,
         aSX, aSY, aSWidth, aSHeight,
         aDX, aDY, aDWidth, aDHeight);
#endif

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

  if (mAlphaDepth==8) {
    DrawComposited(aContext, aSurface, 
                   aSX, aSY, aSWidth, aSHeight, 
                   aDX, aDY, aSWidth, aSHeight);
    return NS_OK;
  }

  nsDrawingSurfaceGTK *drawing = (nsDrawingSurfaceGTK*)aSurface;

  if (mAlphaDepth == 1)
    CreateAlphaBitmap(mWidth, mHeight);

  GdkGC *copyGC;
  if (mAlphaPixmap) {
    copyGC = gdk_gc_new(drawing->GetDrawable());
    GdkGC *gc = ((nsRenderingContextGTK&)aContext).GetGC();
    gdk_gc_copy(copyGC, gc);
    gdk_gc_unref(gc); // unref the one we got
    
    SetupGCForAlpha(copyGC, aDX-aSX, aDY-aSY);
  } else {
    // don't make a copy... we promise not to change it
    copyGC = ((nsRenderingContextGTK&)aContext).GetGC();
  }

  gdk_window_copy_area(drawing->GetDrawable(),      // dest window
                       copyGC,                      // gc
                       aDX,                         // xdest
                       aDY,                         // ydest
                       mImagePixmap,                // source window
                       aSX,                         // xsrc
                       aSY,                         // ysrc
                       aSWidth,                     // width
                       aSHeight);                   // height
 
  gdk_gc_unref(copyGC);
  mFlags = 0;

  return NS_OK;
}

//------------------------------------------------------------
// 8-bit alpha composite drawing...
// Most of this will disappear with gtk+-1.4

// Compositing code consists of these functions:
//  * findIndex32() - helper function to convert mask into bitshift offset
//  * findIndex24() - helper function to convert mask into bitshift offset
//  * nsImageGTK::DrawComposited32() - 32-bit (888) truecolor convert/composite
//  * nsImageGTK::DrawComposited24() - 24-bit (888) truecolor convert/composite
//  * nsImageGTK::DrawComposited16() - 16-bit ([56][56][56]) truecolor 
//                                     convert/composite
//  * nsImageGTK::DrawCompositedGeneral() - convert/composite for any visual
//                                          not handled by the above methods
//  * nsImageGTK::DrawComposited() - compositing master method; does region
//                                   clipping, calls one of the above, then
//                                   writes out the composited image

static unsigned
findIndex32(unsigned mask)
{
  switch (mask) {
  case 0xff:
    return 3;
  case 0xff00:
    return 2;
  case 0xff0000:
    return 1;
  case 0xff000000:
    return 0;
  default:
    return 0;
  }
}

static unsigned
findIndex24(unsigned mask)
{
  switch (mask) {
  case 0xff:
    return 2;
  case 0xff00:
    return 1;
  case 0xff0000:
    return 0;
  default:
    return 0;
  }
}


// 32-bit (888) truecolor convert/composite function
void
nsImageGTK::DrawComposited32(PRBool isLSB, PRBool flipBytes,
                             PRUint8 *imageOrigin, PRUint32 imageStride,
                             PRUint8 *alphaOrigin, PRUint32 alphaStride,
                             unsigned width, unsigned height,
                             XImage *ximage, unsigned char *readData)
{
  GdkVisual *visual   = gdk_rgb_get_visual();
  unsigned redIndex   = findIndex32(visual->red_mask);
  unsigned greenIndex = findIndex32(visual->green_mask);
  unsigned blueIndex  = findIndex32(visual->blue_mask);

  if (flipBytes^isLSB) {
    redIndex   = 3-redIndex;
    greenIndex = 3-greenIndex;
    blueIndex  = 3-blueIndex;
  }

//  fprintf(stderr, "startX=%u startY=%u activeX=%u activeY=%u\n",
//          startX, startY, activeX, activeY);
//  fprintf(stderr, "width=%u height=%u\n", ximage->width, ximage->height);

  for (unsigned y=0; y<height; y++) {
    unsigned char *baseRow   = (unsigned char *)ximage->data 
                                            +y*ximage->bytes_per_line;
    unsigned char *targetRow = readData     +3*(y*ximage->width);
    unsigned char *imageRow  = imageOrigin + y*imageStride;
    unsigned char *alphaRow  = alphaOrigin + y*alphaStride;

    for (unsigned i=0; i<width;
         i++, baseRow+=4, targetRow+=3, imageRow+=3, alphaRow++) {
      unsigned alpha = *alphaRow;
      MOZ_BLEND(targetRow[0], baseRow[redIndex],   imageRow[0], alpha);
      MOZ_BLEND(targetRow[1], baseRow[greenIndex], imageRow[1], alpha);
      MOZ_BLEND(targetRow[2], baseRow[blueIndex],  imageRow[2], alpha);
    }
  }
}

// 24-bit (888) truecolor convert/composite function
void
nsImageGTK::DrawComposited24(PRBool isLSB, PRBool flipBytes,
                             PRUint8 *imageOrigin, PRUint32 imageStride,
                             PRUint8 *alphaOrigin, PRUint32 alphaStride,
                             unsigned width, unsigned height,
                             XImage *ximage, unsigned char *readData)
{
  GdkVisual *visual   = gdk_rgb_get_visual();
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
    unsigned char *imageRow  = imageOrigin + y*imageStride;
    unsigned char *alphaRow  = alphaOrigin + y*alphaStride;

    for (unsigned i=0; i<width;
         i++, baseRow+=3, targetRow+=3, imageRow+=3, alphaRow++) {
      unsigned alpha = *alphaRow;
      MOZ_BLEND(targetRow[0], baseRow[redIndex],   imageRow[0], alpha);
      MOZ_BLEND(targetRow[1], baseRow[greenIndex], imageRow[1], alpha);
      MOZ_BLEND(targetRow[2], baseRow[blueIndex],  imageRow[2], alpha);
    }
  }
}

unsigned nsImageGTK::scaled6[1<<6] = {
  3,   7,  11,  15,  19,  23,  27,  31,  35,  39,  43,  47,  51,  55,  59,  63,
 67,  71,  75,  79,  83,  87,  91,  95,  99, 103, 107, 111, 115, 119, 123, 127,
131, 135, 139, 143, 147, 151, 155, 159, 163, 167, 171, 175, 179, 183, 187, 191,
195, 199, 203, 207, 211, 215, 219, 223, 227, 231, 235, 239, 243, 247, 251, 255
};

unsigned nsImageGTK::scaled5[1<<5] = {
  7,  15,  23,  31,  39,  47,  55,  63,  71,  79,  87,  95, 103, 111, 119, 127,
135, 143, 151, 159, 167, 175, 183, 191, 199, 207, 215, 223, 231, 239, 247, 255
};

// 16-bit ([56][56][56]) truecolor convert/composite function
void
nsImageGTK::DrawComposited16(PRBool isLSB, PRBool flipBytes,
                             PRUint8 *imageOrigin, PRUint32 imageStride,
                             PRUint8 *alphaOrigin, PRUint32 alphaStride,
                             unsigned width, unsigned height,
                             XImage *ximage, unsigned char *readData)
{
  GdkVisual *visual   = gdk_rgb_get_visual();

  unsigned *redScale   = (visual->red_prec   == 5) ? scaled5 : scaled6;
  unsigned *greenScale = (visual->green_prec == 5) ? scaled5 : scaled6;
  unsigned *blueScale  = (visual->blue_prec  == 5) ? scaled5 : scaled6;

  for (unsigned y=0; y<height; y++) {
    unsigned char *baseRow   = (unsigned char *)ximage->data 
                                            +y*ximage->bytes_per_line;
    unsigned char *targetRow = readData     +3*(y*ximage->width);
    unsigned char *imageRow  = imageOrigin + y*imageStride;
    unsigned char *alphaRow  = alphaOrigin + y*alphaStride;

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
                redScale[(pix&visual->red_mask)>>visual->red_shift], 
                imageRow[0], alpha);
      MOZ_BLEND(targetRow[1],
                greenScale[(pix&visual->green_mask)>>visual->green_shift], 
                imageRow[1], alpha);
      MOZ_BLEND(targetRow[2],
                blueScale[(pix&visual->blue_mask)>>visual->blue_shift], 
                imageRow[2], alpha);
    }
  }
}

// Generic convert/composite function
void
nsImageGTK::DrawCompositedGeneral(PRBool isLSB, PRBool flipBytes,
                                  PRUint8 *imageOrigin, PRUint32 imageStride,
                                  PRUint8 *alphaOrigin, PRUint32 alphaStride,
                                  unsigned width, unsigned height,
                                  XImage *ximage, unsigned char *readData)
{
  GdkVisual *visual     = gdk_rgb_get_visual();
  GdkColormap *colormap = gdk_rgb_get_cmap();

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

  unsigned redScale, greenScale, blueScale, redFill, greenFill, blueFill;
  redScale =   8-visual->red_prec;
  greenScale = 8-visual->green_prec;
  blueScale =  8-visual->blue_prec;
  redFill =   0xff>>visual->red_prec;
  greenFill = 0xff>>visual->green_prec;
  blueFill =  0xff>>visual->blue_prec;

  for (int row=0; row<ximage->height; row++) {
    unsigned char *ptr = 
      (unsigned char *)ximage->data + row*ximage->bytes_per_line;
    for (int col=0; col<ximage->width; col++) {
      unsigned pix;
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
      switch (visual->type) {
      case GDK_VISUAL_STATIC_GRAY:
      case GDK_VISUAL_GRAYSCALE:
      case GDK_VISUAL_STATIC_COLOR:
      case GDK_VISUAL_PSEUDO_COLOR:
        *target++ = colormap->colors[pix].red   >>8;
        *target++ = colormap->colors[pix].green >>8;
        *target++ = colormap->colors[pix].blue  >>8;
        break;
        
      case GDK_VISUAL_DIRECT_COLOR:
        *target++ = 
          colormap->colors[(pix&visual->red_mask)>>visual->red_shift].red       >> 8;
        *target++ = 
          colormap->colors[(pix&visual->green_mask)>>visual->green_shift].green >> 8;
        *target++ =
          colormap->colors[(pix&visual->blue_mask)>>visual->blue_shift].blue    >> 8;
        break;
        
      case GDK_VISUAL_TRUE_COLOR:
        *target++ = 
          redFill|((pix&visual->red_mask)>>visual->red_shift)<<redScale;
        *target++ = 
          greenFill|((pix&visual->green_mask)>>visual->green_shift)<<greenScale;
        *target++ = 
          blueFill|((pix&visual->blue_mask)>>visual->blue_shift)<<blueScale;
        break;
      }
    }
  }

  // now composite
  for (unsigned y=0; y<height; y++) {
    unsigned char *targetRow = readData+3*y*width;
    unsigned char *imageRow  = imageOrigin + y*imageStride;
    unsigned char *alphaRow  = alphaOrigin + y*alphaStride;
    
    for (unsigned i=0; i<width; i++) {
      unsigned alpha = alphaRow[i];
      MOZ_BLEND(targetRow[3*i],   targetRow[3*i],   imageRow[3*i],   alpha);
      MOZ_BLEND(targetRow[3*i+1], targetRow[3*i+1], imageRow[3*i+1], alpha);
      MOZ_BLEND(targetRow[3*i+2], targetRow[3*i+2], imageRow[3*i+2], alpha);
    }
  }
}

void
nsImageGTK::DrawComposited(nsIRenderingContext &aContext,
                           nsDrawingSurface aSurface,
                           PRInt32 aSX, PRInt32 aSY,
                           PRInt32 aSWidth, PRInt32 aSHeight,
                           PRInt32 aDX, PRInt32 aDY,
                           PRInt32 aDWidth, PRInt32 aDHeight)
{
  if ((aDWidth==0) || (aDHeight==0))
    return;

  nsDrawingSurfaceGTK* drawing = (nsDrawingSurfaceGTK*) aSurface;
  GdkVisual *visual = gdk_rgb_get_visual();
    
  Display *dpy = GDK_WINDOW_XDISPLAY(drawing->GetDrawable());
  Drawable drawable = GDK_WINDOW_XWINDOW(drawing->GetDrawable());

  // I hate clipping...
  PRUint32 surfaceWidth, surfaceHeight;
  drawing->GetDimensions(&surfaceWidth, &surfaceHeight);
  
  int readX, readY;
  unsigned readWidth, readHeight, destX, destY;

  if ((aDY>=(int)surfaceHeight) || (aDX>=(int)surfaceWidth) ||
      (aDY+aDHeight<=0) || (aDX+aDWidth<=0)) {
    // This should never happen if the layout engine is sane,
    // as it means we're trying to draw an image which is outside
    // the drawing surface.  Bulletproof gfx for now...
    return;
  }

  if (aDX<0) {
    readX = 0;   readWidth = aDWidth+aDX;    destX = aSX-aDX;
  } else {
    readX = aDX;  readWidth = aDWidth;       destX = aSX;
  }
  if (aDY<0) {
    readY = 0;   readHeight = aDHeight+aDY;  destY = aSY-aDY;
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

  XImage *ximage = XGetImage(dpy, drawable,
                             readX, readY, readWidth, readHeight, 
                             AllPlanes, ZPixmap);

  NS_ASSERTION((ximage!=NULL), "XGetImage() failed");
  if (!ximage)
    return;

  unsigned char *readData = 
    (unsigned char *)nsMemory::Alloc(3*readWidth*readHeight);

  PRUint8 *scaledImage = 0;
  PRUint8 *scaledAlpha = 0;
  PRUint8 *imageOrigin, *alphaOrigin;
  PRUint32 imageStride, alphaStride;
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
  unsigned test = 1;
  isLSB = (((char *)&test)[0]) ? 1 : 0;

  PRBool flipBytes = 
    ( isLSB && ximage->byte_order != LSBFirst) ||
    (!isLSB && ximage->byte_order == LSBFirst);

  if ((ximage->bits_per_pixel==32) &&
      (visual->red_prec == 8) &&
      (visual->green_prec == 8) &&
      (visual->blue_prec == 8))
    DrawComposited32(isLSB, flipBytes, 
                     imageOrigin, imageStride,
                     alphaOrigin, alphaStride, 
                     readWidth, readHeight, ximage, readData);
  else if ((ximage->bits_per_pixel==24) &&
           (visual->red_prec == 8) && 
           (visual->green_prec == 8) &&
           (visual->blue_prec == 8))
    DrawComposited24(isLSB, flipBytes, 
                     imageOrigin, imageStride,
                     alphaOrigin, alphaStride, 
                     readWidth, readHeight, ximage, readData);
  else if ((ximage->bits_per_pixel==16) &&
           ((visual->red_prec == 5)   || (visual->red_prec == 6)) &&
           ((visual->green_prec == 5) || (visual->green_prec == 6)) &&
           ((visual->blue_prec == 5)  || (visual->blue_prec == 6)))
    DrawComposited16(isLSB, flipBytes,
                     imageOrigin, imageStride,
                     alphaOrigin, alphaStride, 
                     readWidth, readHeight, ximage, readData);
  else
    DrawCompositedGeneral(isLSB, flipBytes,
                     imageOrigin, imageStride,
                     alphaOrigin, alphaStride, 
                     readWidth, readHeight, ximage, readData);

  GdkGC *imageGC = ((nsRenderingContextGTK&)aContext).GetGC();
  gdk_draw_rgb_image(drawing->GetDrawable(), imageGC,
                     readX, readY, readWidth, readHeight,
                     GDK_RGB_DITHER_MAX,
                     readData, 3*readWidth);
  gdk_gc_unref(imageGC);

  XDestroyImage(ximage);
  nsMemory::Free(readData);
  if (scaledImage)
    nsMemory::Free(scaledImage);
  if (scaledAlpha)
    nsMemory::Free(scaledAlpha);
  mFlags = 0;
}

void nsImageGTK::CreateAlphaBitmap(PRInt32 aWidth, PRInt32 aHeight)
{
  XImage *x_image = nsnull;
  Pixmap pixmap = 0;
  Display *dpy = nsnull;
  Visual *visual = nsnull;

  // Create gc clip-mask on demand
  if (mAlphaBits && IsFlagSet(nsImageUpdateFlags_kBitsChanged, mFlags)) {

#if 1
    if (!mAlphaPixmap) {
      mAlphaPixmap = gdk_pixmap_new(nsnull, aWidth, aHeight, 1);
    }

    /* get the X primitives */
    dpy = GDK_WINDOW_XDISPLAY(mAlphaPixmap);

    /* this is the depth of the pixmap that we are going to draw to.
       It's always a bitmap.  We're doing alpha here folks. */
    visual = GDK_VISUAL_XVISUAL(gdk_rgb_get_visual());

    // Make an image out of the alpha-bits created by the image library
    x_image = XCreateImage(dpy, visual,
                           1, /* visual depth...1 for bitmaps */
                           XYPixmap,
                           0, /* x offset, XXX fix this */
                           (char *)mAlphaBits,  /* cast away our sign. */
                           aWidth,
                           aHeight,
                           32,/* bitmap pad */
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

    // Write into the pixemap that is underneath gdk's mAlphaPixmap
    // the image we just created.
    pixmap = GDK_WINDOW_XWINDOW(mAlphaPixmap);

    if (!s1bitGC) {
      s1bitGC = gdk_gc_new(mAlphaPixmap);
    }

    XPutImage(dpy, pixmap, GDK_GC_XGC(s1bitGC), x_image, 0, 0, 0, 0,
              aWidth, aHeight);

    // Now we are done with the temporary image
    x_image->data = 0;          /* Don't free the IL_Pixmap's bits. */
    XDestroyImage(x_image);
#else
    mAlphaPixmap = gdk_bitmap_create_from_data(mImagePixmap, mAlphaBits, mAlphaWidth, mAlphaHeight);
#endif
  }

}

void nsImageGTK::CreateOffscreenPixmap(PRInt32 aWidth, PRInt32 aHeight)
{
  // Render unique image bits onto an off screen pixmap only once
  // The image bits can change as a result of ImageUpdated() - for
  // example: animated GIFs.
  if (!mImagePixmap) {
#ifdef TRACE_IMAGE_ALLOCATION
    printf("nsImageGTK::Draw(this=%p) gdk_pixmap_new(nsnull,width=%d,height=%d,depth=%d)\n",
           this,
           aWidth, aHeight,
           mDepth);
#endif

    // Create an off screen pixmap to hold the image bits.
    mImagePixmap = gdk_pixmap_new(nsnull, aWidth, aHeight,
                                  gdk_rgb_get_visual()->depth);
  }
}


void nsImageGTK::SetupGCForAlpha(GdkGC *aGC, PRInt32 aX, PRInt32 aY)
{
  // XXX should use (different?) GC cache here
  if (mAlphaPixmap) {
    // Setup gc to use the given alpha-pixmap for clipping
    XGCValues xvalues;
    memset(&xvalues, 0, sizeof(XGCValues));
    unsigned long xvalues_mask = 0;
    xvalues.clip_x_origin = aX;
    xvalues.clip_y_origin = aY;
    xvalues_mask = GCClipXOrigin | GCClipYOrigin;

    xvalues.clip_mask = GDK_WINDOW_XWINDOW(mAlphaPixmap);
    xvalues_mask |= GCClipMask;

    XChangeGC(GDK_DISPLAY(), GDK_GC_XGC(aGC), xvalues_mask, &xvalues);
  }
}

// Draw the bitmap, this draw just has destination coordinates
NS_IMETHODIMP
nsImageGTK::Draw(nsIRenderingContext &aContext,
                 nsDrawingSurface aSurface,
                 PRInt32 aX, PRInt32 aY,
                 PRInt32 aWidth, PRInt32 aHeight)
{
  g_return_val_if_fail ((aSurface != nsnull), NS_ERROR_FAILURE);

  if (mPendingUpdate)
    UpdateCachedImage();

  if ((mAlphaDepth==1) && mIsSpacer)
    return NS_OK;

  if (mAlphaDepth==8) {
    DrawComposited(aContext, aSurface, 0, 0, aWidth, aHeight, aX, aY, aWidth, aHeight);
    return NS_OK;
  }

  // XXX kipp: this is temporary code until we eliminate the
  // width/height arguments from the draw method.
  if ((aWidth != mWidth) || (aHeight != mHeight)) {
    aWidth = mWidth;
    aHeight = mHeight;
  }


#ifdef TRACE_IMAGE_ALLOCATION
  printf("nsImageGTK::Draw(this=%p,x=%d,y=%d,width=%d,height=%d)\n",
         this,
         aX, aY,
         aWidth, aHeight);
#endif

  nsDrawingSurfaceGTK* drawing = (nsDrawingSurfaceGTK*) aSurface;

#ifdef CHEAP_PERFORMANCE_MEASURMENT
  gStartTime = PR_Now();
#endif

  CreateAlphaBitmap(aWidth, aHeight);

#ifdef CHEAP_PERFORMANCE_MEASURMENT
  gAlphaTime = PR_Now();
#endif



  PRInt32
    validX = 0,
    validY = 0,
    validWidth  = aWidth,
    validHeight = aHeight;

  // limit the image rectangle to the size of the image data which
  // has been validated.
  if (mDecodedY2 < aHeight) {
    validHeight = mDecodedY2 - mDecodedY1;
  }
  if (mDecodedX2 < aWidth) {
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

#ifdef CHEAP_PERFORMANCE_MEASURMENT
  gPixmapTime = PR_Now();
#endif

  GdkGC *copyGC;
  if (mAlphaPixmap) {
    copyGC = gdk_gc_new(drawing->GetDrawable());
    GdkGC *gc = ((nsRenderingContextGTK&)aContext).GetGC();
    gdk_gc_copy(copyGC, gc);
    gdk_gc_unref(gc); // unref the one we got
    
    SetupGCForAlpha(copyGC, aX, aY);
  } else {
    // don't make a copy... we promise not to change it
    copyGC = ((nsRenderingContextGTK&)aContext).GetGC();
  }
#ifdef TRACE_IMAGE_ALLOCATION
  printf("nsImageGTK::Draw(this=%p) gdk_draw_pixmap(x=%d,y=%d,width=%d,height=%d)\n",
         this,
         validX+aX,
         validY+aY,
         validWidth,                  
         validHeight);
#endif

#ifdef CHEAP_PERFORMANCE_MEASURMENT
  gCopyStart = PR_Now();
#endif
  // copy our offscreen pixmap onto the window.
  gdk_window_copy_area(drawing->GetDrawable(),      // dest window
                       copyGC,                      // gc
                       validX+aX,                   // xdest
                       validY+aY,                   // ydest
                       mImagePixmap,                // source window
                       validX,                      // xsrc
                       validY,                      // ysrc
                       validWidth,                  // width
                       validHeight);                // height
#ifdef CHEAP_PERFORMANCE_MEASURMENT
  gCopyEnd = PR_Now();
#endif

  gdk_gc_unref(copyGC);


#ifdef CHEAP_PERFORMANCE_MEASURMENT
  gEndTime = PR_Now();
  printf("nsImageGTK::Draw(this=%p,w=%d,h=%d) total=%lld pixmap=%lld, alpha=%lld, copy=%lld\n",
         this,
         aWidth, aHeight,
         gEndTime - gStartTime,
         gPixmapTime - gAlphaTime,
         gAlphaTime - gStartTime,
         gCopyEnd - gCopyStart);
#endif

  mFlags = 0;

  return NS_OK;
}

/* inline */
void nsImageGTK::TilePixmap(GdkPixmap *src, GdkPixmap *dest, 
                            PRInt32 aSXOffset, PRInt32 aSYOffset,
                            const nsRect &destRect, 
                            const nsRect &clipRect, PRBool useClip)
{
  GdkGC *gc;
  GdkGCValues values;
  GdkGCValuesMask valuesMask;
  memset(&values, 0, sizeof(GdkGCValues));
  values.fill = GDK_TILED;
  values.tile = src;
  values.ts_x_origin = destRect.x - aSXOffset;
  values.ts_y_origin = destRect.y - aSYOffset;
  valuesMask = GdkGCValuesMask(GDK_GC_FILL | GDK_GC_TILE | GDK_GC_TS_X_ORIGIN | GDK_GC_TS_Y_ORIGIN);
  gc = gdk_gc_new_with_values(src, &values, valuesMask);

  if (useClip) {
    GdkRectangle gdkrect = {clipRect.x, clipRect.y, clipRect.width, clipRect.height};
    gdk_gc_set_clip_rectangle(gc, &gdkrect);
  }

  // draw to destination window
  #ifdef DEBUG_TILING
  printf("nsImageGTK::TilePixmap(..., %d, %d, %d, %d)\n",
         destRect.x, destRect.y, 
         destRect.width, destRect.height);
  #endif

  gdk_draw_rectangle(dest, gc, PR_TRUE,
                     destRect.x, destRect.y,
                     destRect.width, destRect.height);

  gdk_gc_unref(gc);
}


NS_IMETHODIMP nsImageGTK::DrawTile(nsIRenderingContext &aContext,
                                   nsDrawingSurface aSurface,
                                   PRInt32 aSXOffset, PRInt32 aSYOffset,
                                   const nsRect &aTileRect)
{
#ifdef DEBUG_TILING
  printf("nsImageGTK::DrawTile: mWidth=%d, mHeight=%d\n", mWidth, mHeight);
  printf("nsImageGTK::DrawTile((src: %d, %d), (tile: %d,%d, %d, %d) %p\n", aSXOffset, aSYOffset,
         aTileRect.x, aTileRect.y,
         aTileRect.width, aTileRect.height, this);
#endif

  if (mPendingUpdate)
    UpdateCachedImage();

  if (mPendingUpdate)
    UpdateCachedImage();

  if ((mAlphaDepth==1) && mIsSpacer)
    return NS_OK;

  nsDrawingSurfaceGTK *drawing = (nsDrawingSurfaceGTK*)aSurface;
  PRBool partial = PR_FALSE;

  PRInt32
    validX = 0,
    validY = 0,
    validWidth  = mWidth,
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

  if (aTileRect.width == 0 || aTileRect.height == 0 ||
      validWidth == 0 || validHeight == 0) {
    return NS_OK;
  }

  if (partial || (mAlphaDepth == 8)) {
#ifdef DEBUG_TILING
    printf("Warning: using slow tiling\n");
#endif
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
        Draw(aContext,aSurface, x,y,
             PR_MIN(validWidth, aX1-x),
             PR_MIN(validHeight, aY1-y));

    aContext.PopState(clipState);

    return NS_OK;
  }

  if (mAlphaDepth == 1) {
    GdkPixmap *tileImg;
    GdkPixmap *tileMask;

    CreateAlphaBitmap(validWidth, validHeight);

    nsRect tmpRect(0,0,aTileRect.width, aTileRect.height);

    tileImg = gdk_pixmap_new(mImagePixmap, aTileRect.width, 
                             aTileRect.height, drawing->GetDepth());
    TilePixmap(mImagePixmap, tileImg, aSXOffset, aSYOffset, tmpRect,
               tmpRect, PR_FALSE);


    // tile alpha mask
    tileMask = gdk_pixmap_new(mAlphaPixmap, aTileRect.width, aTileRect.height, mAlphaDepth);
    TilePixmap(mAlphaPixmap, tileMask, aSXOffset, aSYOffset, tmpRect,
               tmpRect, PR_FALSE);

    GdkGC *fgc = gdk_gc_new(drawing->GetDrawable());
    gdk_gc_set_clip_mask(fgc, (GdkBitmap*)tileMask);
    gdk_gc_set_clip_origin(fgc, aTileRect.x, aTileRect.y);

    // and copy it back
    gdk_window_copy_area(drawing->GetDrawable(), fgc, aTileRect.x,
                         aTileRect.y, tileImg, 0, 0,
                         aTileRect.width, aTileRect.height);
    gdk_gc_unref(fgc);

    gdk_pixmap_unref(tileImg);
    gdk_pixmap_unref(tileMask);

  } else {

    // In the non-alpha case, gdk can tile for us

    nsRect clipRect;
    PRBool isValid;
    aContext.GetClipRect(clipRect, isValid);

    TilePixmap(mImagePixmap, drawing->GetDrawable(), aSXOffset, aSYOffset,
               aTileRect, clipRect, PR_FALSE);
  }

  mFlags = 0;
  return NS_OK;
}



//------------------------------------------------------------

nsresult nsImageGTK::Optimize(nsIDeviceContext* aContext)
{
  return NS_OK;
}

//------------------------------------------------------------
// lock the image pixels. nothing to do on gtk
NS_IMETHODIMP
nsImageGTK::LockImagePixels(PRBool aMaskPixels)
{
  return NS_OK;
}

//------------------------------------------------------------
// unlock the image pixels. nothing to do on gtk
NS_IMETHODIMP
nsImageGTK::UnlockImagePixels(PRBool aMaskPixels)
{
  return NS_OK;
} 

// ---------------------------------------------------
//	Set the decoded dimens of the image
//
NS_IMETHODIMP
nsImageGTK::SetDecodedRect(PRInt32 x1, PRInt32 y1, PRInt32 x2, PRInt32 y2 )
{
    
  mDecodedX1 = x1; 
  mDecodedY1 = y1; 
  mDecodedX2 = x2; 
  mDecodedY2 = y2; 


  return NS_OK;
}

#ifdef USE_IMG2
NS_IMETHODIMP nsImageGTK::DrawToImage(nsIImage* aDstImage,
                                      nscoord aDX, nscoord aDY,
                                      nscoord aDWidth, nscoord aDHeight)
{
  nsImageGTK *dest = NS_STATIC_CAST(nsImageGTK *, aDstImage);

  if (!dest)
    return NS_ERROR_FAILURE;

  if (mPendingUpdate)
    UpdateCachedImage();
  
  if (!dest->mImagePixmap) {
    dest->CreateOffscreenPixmap(dest->mWidth, dest->mHeight);
  }
  
  if (!dest->mImagePixmap) {
    return NS_ERROR_FAILURE;
  }

  if (!mImagePixmap)
    return NS_ERROR_FAILURE;

  GdkGC *gc = gdk_gc_new(dest->mImagePixmap);

  if (mAlphaDepth == 1)
    CreateAlphaBitmap(mWidth, mHeight);
  
  if (mAlphaPixmap) {
    SetupGCForAlpha(gc, aDX, aDY);
  }

  gdk_window_copy_area(dest->mImagePixmap, gc,
                       aDX, aDY,
                       mImagePixmap,
                       0, 0, mWidth, mHeight);

  gdk_gc_unref(gc);

  if ((mAlphaDepth==1) && (dest->mAlphaPixmap)) {
    GdkGCValues values;
    GdkGCValuesMask vmask;

    memset(&values, 0, sizeof(GdkGCValues));
    values.function = GDK_OR;
    vmask = GDK_GC_FUNCTION;
    gc = gdk_gc_new_with_values(dest->mAlphaPixmap, &values, vmask);
    gdk_window_copy_area(dest->mAlphaPixmap, gc,
                         aDX, aDY,
                         mAlphaPixmap,
                         0, 0, mWidth, mHeight);
    gdk_gc_unref(gc);
  }

  if (!mIsSpacer || !mAlphaDepth)
    dest->mIsSpacer = PR_FALSE;

  // need to copy the mImageBits in case we're rendered scaled
  PRUint8 *scaledImage = 0, *scaledAlpha = 0;
  PRUint8 *rgbPtr=0, *alphaPtr=0;
  PRUint32 rgbStride, alphaStride;

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
        // if this pixel is opaque then copy into the destination image
        if (NS_GET_BIT(alpha, x)) {
          dst[0] = src[0];
          dst[1] = src[1];
          dst[2] = src[2];
          NS_SET_BIT(dstAlpha, aDX+x);
        }
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
#endif
