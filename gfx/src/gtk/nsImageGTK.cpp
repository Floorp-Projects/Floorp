/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <pavlov@netscape.com>
 *   Tim Rowley <tor@cs.brown.edu>
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
#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include "imgScaler.h"

#include "nsImageGTK.h"
#include "nsRenderingContextGTK.h"

#include "nspr.h"

#define IsFlagSet(a,b) ((a) & (b))

#define NS_GET_BIT(rowptr, x) (rowptr[(x)>>3] &  (1<<(7-(x)&0x7)))
#define NS_SET_BIT(rowptr, x) (rowptr[(x)>>3] |= (1<<(7-(x)&0x7)))
#define NS_CLEAR_BIT(rowptr, x) (rowptr[(x)>>3] &= ~(1<<(7-(x)&0x7)))

// Defining this will trace the allocation of images.  This includes
// ctor, dtor and update.
// #define TRACE_IMAGE_ALLOCATION

//#define CHEAP_PERFORMANCE_MEASURMENT 1

// Define this to see tiling debug output
// #define DEBUG_TILING

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
  : mImageBits(nsnull)
  , mImagePixmap(nsnull)
  , mTrueAlphaBits(nsnull)
  , mAlphaBits(nsnull)
  , mAlphaPixmap(nsnull)
  , mAlphaXImage(nsnull)
  , mWidth(0)
  , mHeight(0)
  , mRowBytes(0)
  , mSizeImage(0)
  , mDecodedX1(PR_INT32_MAX)
  , mDecodedY1(PR_INT32_MAX)
  , mDecodedX2(0)
  , mDecodedY2(0)
  , mAlphaDepth(0)
  , mTrueAlphaDepth(0)
  , mIsSpacer(PR_TRUE)
  , mPendingUpdate(PR_FALSE)
  , mDepth(0)
  , mOptimized(PR_FALSE)
{
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

  if (mAlphaXImage) {
    mAlphaXImage->data = 0;
    XDestroyImage(mAlphaXImage);
  }

#ifdef TRACE_IMAGE_ALLOCATION
  printf("nsImageGTK::~nsImageGTK(this=%p)\n",
         this);
#endif
}

/* static */ void
nsImageGTK::Shutdown()
{
  if (s1bitGC) {
    gdk_gc_unref(s1bitGC);
    s1bitGC = nsnull;
  }
  if (sXbitGC) {
    gdk_gc_unref(sXbitGC);
    sXbitGC = nsnull;
  }
}

//------------------------------------------------------------

nsresult nsImageGTK::Init(PRInt32 aWidth, PRInt32 aHeight,
                          PRInt32 aDepth, nsMaskRequirements aMaskRequirements)
{
  // Assumed: Init only gets called once by gfxIImageFrame
  g_return_val_if_fail ((aWidth != 0) || (aHeight != 0), NS_ERROR_FAILURE);

  if (24 == aDepth) {
    mNumBytesPixel = 3;
  } else {
    NS_ASSERTION(PR_FALSE, "unexpected image depth");
    return NS_ERROR_UNEXPECTED;
  }

  mWidth = aWidth;
  mHeight = aHeight;
  mDepth = aDepth;

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
    case nsMaskRequirements_kNeeds8Bit:
      mTrueAlphaRowBytes = aWidth;
      mTrueAlphaDepth = 8;

      // 32-bit align each row
      mTrueAlphaRowBytes = (mTrueAlphaRowBytes + 3) & ~0x3;
      mTrueAlphaBits = new PRUint8[mTrueAlphaRowBytes * aHeight];
      memset(mTrueAlphaBits, 0, mTrueAlphaRowBytes*aHeight);

      // FALL THROUGH

    case nsMaskRequirements_kNeeds1Bit:
      mAlphaRowBytes = (aWidth + 7) / 8;
      mAlphaDepth = 1;

      // 32-bit align each row
      mAlphaRowBytes = (mAlphaRowBytes + 3) & ~0x3;

      mAlphaBits = new PRUint8[mAlphaRowBytes * aHeight];
      memset(mAlphaBits, 0, mAlphaRowBytes*aHeight);
      break;

    default:
      break; // avoid compiler warning
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

PRUint8 *nsImageGTK::GetAlphaBits()
{
  if (mTrueAlphaBits)
    return mTrueAlphaBits;
  else
    return mAlphaBits;
}

PRInt32
nsImageGTK::GetAlphaLineStride()
{
  if (mTrueAlphaBits)
    return mTrueAlphaRowBytes;
  else
    return mAlphaRowBytes;
}

void nsImageGTK::ImageUpdated(nsIDeviceContext *aContext,
                              PRUint8 aFlags,
                              nsRect *aUpdateRect)
{
  mPendingUpdate = PR_TRUE;
  mUpdateRegion.Or(mUpdateRegion, *aUpdateRect);

  mDecodedX1 = PR_MIN(mDecodedX1, aUpdateRect->x);
  mDecodedY1 = PR_MIN(mDecodedY1, aUpdateRect->y);

  if (aUpdateRect->YMost() > mDecodedY2)
    mDecodedY2 = aUpdateRect->YMost();
  if (aUpdateRect->XMost() > mDecodedX2)
    mDecodedX2 = aUpdateRect->XMost();
}

void nsImageGTK::UpdateCachedImage()
{
#ifdef TRACE_IMAGE_ALLOCATION
  printf("nsImageGTK::ImageUpdated(this=%p)\n",
         this);
#endif

  nsRegionRectIterator ri(mUpdateRegion);
  const nsRect *rect;

  while ((rect = ri.Next()) != nsnull) {

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
            NS_CLEAR_BIT(mask,x);
            if (mAlphaDepth == 0) {
              mAlphaDepth=1;

              // promoting an image from no alpha channel to 1-bit, so
              // we need to create/clear the alpha pixmap
              CreateOffscreenPixmap(mWidth, mHeight);

              XFillRectangle(GDK_WINDOW_XDISPLAY(mAlphaPixmap),
                             GDK_WINDOW_XWINDOW(mAlphaPixmap),
                             GDK_GC_XGC(s1bitGC),
                             mDecodedX1, mDecodedY1,
                             mDecodedX2 - mDecodedX1 + 1,
                             mDecodedY2 - mDecodedY1 + 1);
            }
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
          for (unsigned x=leftindex; x<=rightindex; x++) {
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

      gdk_draw_rgb_image_dithalign(mImagePixmap, sXbitGC, 
                                   rect->x, rect->y,
                                   rect->width, rect->height,
                                   GDK_RGB_DITHER_MAX,
                                   mImageBits + mRowBytes*rect->y + 3*rect->x,
                                   mRowBytes,
                                   rect->x, rect->y);
    }

    if (mAlphaDepth==1) {
      XPutImage(GDK_WINDOW_XDISPLAY(mAlphaPixmap),
                GDK_WINDOW_XWINDOW(mAlphaPixmap),
                GDK_GC_XGC(s1bitGC),
                mAlphaXImage,
                rect->x, rect->y, 
                rect->x, rect->y,
                rect->width, rect->height);
    }
  }
  
  mUpdateRegion.SetEmpty();
  mPendingUpdate = PR_FALSE;
  mFlags = nsImageUpdateFlags_kBitsChanged; // this should be 0'd out by Draw()
}

#ifdef CHEAP_PERFORMANCE_MEASURMENT
static PRTime gConvertTime, gAlphaTime, gCopyStart, gCopyEnd, gStartTime, gPixmapTime, gEndTime;
#endif

/* Xlib image scaling... */

#define sign(x) ((x)>0 ? 1:-1)

static void XlibStretchHorizontal(long x1,long x2,long y1,long y2,
                                  long ymin,long ymax,
                                  long startColumn, long endColumn,
                                  long offsetX, long offsetY,
                                  GdkPixmap *aSrcImage, GdkPixmap *aDstImage, GdkGC *gc);

/**********************************************************
 XlibRectStretch enlarges or diminishes a source rectangle of a bitmap to
 a destination rectangle. The source rectangle is selected by the two
 points (xs1,ys1) and (xs2,ys2), and the destination rectangle by
 (xd1,yd1) and (xd2,yd2).

 Entry:
	xs1,ys1 - first point of source rectangle
	xs2,ys2 - second point of source rectangle
	xd1,yd1 - first point of destination rectangle
	xd2,yd2 - second point of destination rectangle
  offx, offy - offset to target
**********************************************************/
void
XlibRectStretch(PRInt32 srcWidth, PRInt32 srcHeight,
                PRInt32 dstWidth, PRInt32 dstHeight,
                PRInt32 dstOrigX, PRInt32 dstOrigY,
                PRInt32 aDX, PRInt32 aDY,
                PRInt32 aDWidth, PRInt32 aDHeight,
                GdkPixmap *aSrcImage, GdkPixmap *aDstImage,
                GdkGC *gc, GdkGC *copygc, PRInt32 aDepth)
{
  long dx,dy,e,d,dx2;
  short sx,sy;
  GdkPixmap *aTmpImage = 0;
  PRBool skipHorizontal=PR_FALSE, skipVertical=PR_FALSE;
  long startColumn, startRow, endColumn, endRow;
  long xs1, ys1, xs2, ys2, xd1, yd1, xd2, yd2;

  xs1 = ys1 = xd1 = yd1 = 0;
  xs2 = srcWidth-1;
  ys2 = srcHeight-1;
  xd2 = dstWidth-1;
  yd2 = dstHeight-1;

//  fprintf(stderr, "XRS %p (%ld %ld)-(%ld %ld) (%ld %ld)-(%ld %ld)\n",
//          (void *)aDstImage, xs1, ys1, xs2, ys2, xd1, yd1, xd2, yd2);
  
  startColumn = aDX-dstOrigX;
  startRow    = aDY-dstOrigY;
  endColumn   = aDX+aDWidth-dstOrigX;
  endRow      = aDY+aDHeight-dstOrigY;

//  fprintf(stderr, "startXY = %d %d  endXY = %d %d   %d x %d\n",
//          startColumn, startRow, endColumn, endRow,
//          endColumn-startColumn, endRow-startRow);

  long scaleStartY, scaleEndY;
  scaleStartY = startRow * (ys2-ys1+1) / (yd2-yd1+1);
  scaleEndY   = 1 + endRow * (ys2-ys1+1) / (yd2-yd1+1);

  if (xd2-xd1 == xs2-xs1) {
//    fprintf(stderr, "skipping horizontal\n");
    skipHorizontal = PR_TRUE;
    aTmpImage = aSrcImage;
    scaleStartY = 0;
    scaleEndY = ys2;
  }

  if (yd2-yd1 == ys2-ys1) {
//    fprintf(stderr, "skipping vertical\n");
    skipVertical = PR_TRUE;
    aTmpImage = aDstImage;
  }

  if (skipVertical && skipHorizontal) {
    gdk_draw_pixmap(aDstImage, gc, aSrcImage,
                    0, 0, srcWidth, srcHeight,
                    dstOrigX, dstOrigY);
    return;
  }

//  fprintf(stderr, "scaleY Start/End = %d %d\n", scaleStartY, scaleEndY);

  if (!skipHorizontal && !skipVertical) {
    aTmpImage = gdk_pixmap_new(nsnull,
                               endColumn-startColumn,
                               scaleEndY-scaleStartY,
                               aDepth);
#ifdef MOZ_WIDGET_GTK2
    if (aDepth != 1)
      gdk_drawable_set_colormap(GDK_DRAWABLE(aTmpImage),
                                gdk_rgb_get_colormap());
#endif
  }
 
  dx = abs((int)(yd2-yd1));
  dy = abs((int)(ys2-ys1));
  sx = sign(yd2-yd1);
  sy = sign(ys2-ys1);
  e = dy-dx;
  dx2 = dx;
  dy += 1;
  if (!dx2) dx2=1;

  if (!skipHorizontal)
    XlibStretchHorizontal(xd1, xd2, xs1, xs2, scaleStartY, scaleEndY,
                          startColumn, endColumn,
                          (skipVertical?MAX(dstOrigX,0):0),(skipVertical?MAX(dstOrigY,0):0),
                          aSrcImage, aTmpImage, (skipVertical?gc:copygc));
  
  if (!skipVertical) {
    for (d=0; d<=dx; d++) {
      if ((yd1 >= startRow) && (yd1 <= endRow)) {
        gdk_draw_pixmap(aDstImage, gc, aTmpImage,
                        (skipHorizontal?startColumn:0), ys1-scaleStartY,
                        (dstOrigX>0?dstOrigX:0), dstOrigY+yd1,
                        endColumn-startColumn, 1);
      }
      while (e>=0) {
	      ys1 += sy;
	      e -= dx2;
      }
      yd1 += sx;
      e += dy;
    }
  }

  if (!skipHorizontal && !skipVertical)
    gdk_pixmap_unref(aTmpImage);
}

/**********************************************************
 Stretches a image horizontally by column replication/deletion.
 Used by XlibRectStretch.

 Entry:
	x1,x2 - x-coordinates of the destination line
	y1,y2 - x-coordinates of the source line
	ymin  - y-coordinate of top of stretch region
	ymax  - y-coordinate of bottom of stretch region
**********************************************************/
static void
XlibStretchHorizontal(long x1, long x2, long y1, long y2,
                      long ymin, long ymax,
                      long startColumn, long endColumn,
                      long offsetX, long offsetY,
                      GdkPixmap *aSrcImage, GdkPixmap *aDstImage, GdkGC *gc)
{
  long dx,dy,e,d,dx2;
  short sx,sy;

  dx = abs((int)(x2-x1));
  dy = abs((int)(y2-y1));
  sx = sign(x2-x1);
  sy = sign(y2-y1);
  e = dy-dx;
  dx2 = dx;
  dy += 1;
  if (!dx2) dx2=1;
  for (d=0; d<=dx; d++) {
    if ((x1 >= startColumn) && (x1 <= endColumn)) {
      gdk_draw_pixmap(aDstImage, gc, aSrcImage,
                      y1, ymin, x1-startColumn+offsetX, 0+offsetY,
                      1, ymax-ymin);
    }
    while (e>=0) {
      y1 += sy;
      e -= dx2;
    }
    x1 += sx;
    e += dy;
  }
}

#undef sign



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

  if (mDecodedX2 < mDecodedX1 || mDecodedY2 < mDecodedY1)
    return NS_OK;

#ifdef TRACE_IMAGE_ALLOCATION
  fprintf(stderr, "nsImageGTK::Draw(%p) s=(%4d %4d %4d %4d) d=(%4d %4d %4d %4d)\n",
         this,
         aSX, aSY, aSWidth, aSHeight,
         aDX, aDY, aDWidth, aDHeight);
#endif

  if (aSWidth <= 0 || aDWidth <= 0 || aSHeight <= 0 || aDHeight <= 0) {
    return NS_OK;
  }

  // store some values we'll need for scaling...

  PRInt32 srcWidth, srcHeight, dstWidth, dstHeight;
  PRInt32 dstOrigX, dstOrigY;

  srcWidth = aSWidth;
  srcHeight = aSHeight;
  dstWidth = aDWidth;
  dstHeight = aDHeight;
  dstOrigX = aDX;
  dstOrigY = aDY;

  // clip to decode region
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

  if (aDWidth <= 0 || aDHeight <= 0 || aSWidth <= 0 || aSHeight <= 0) {
    return NS_OK;
  }

  // clip to drawing surface
  nsDrawingSurfaceGTK *drawing = (nsDrawingSurfaceGTK*)aSurface;
  PRUint32 surfaceWidth, surfaceHeight;
  drawing->GetDimensions(&surfaceWidth, &surfaceHeight);

  if (aDX + aDWidth > (PRInt32)surfaceWidth) {
    z = aDX + aDWidth - surfaceWidth;
    aDWidth -= z;
    aSWidth -= z*srcWidth/dstWidth;
  }

  if (aDX < 0) {
    aDWidth += aDX;
    aSWidth += aDX*srcWidth/dstWidth;
    aSX -= aDX*srcWidth/dstWidth;
    aDX = 0;
  }

  if (aDY + aDHeight > (PRInt32)surfaceHeight) {
    z = aDY + aDHeight - surfaceHeight;
    aDHeight -= z;
    aSHeight -= z*srcHeight/dstHeight;
  }

  if (aDY < 0) {
    aDHeight += aDY;
    aSHeight += aDY*srcHeight/dstHeight;
    aSY -= aDY*srcHeight/dstHeight;
    aDY = 0;
  }

  if (aDWidth <= 0 || aDHeight <= 0 || aSWidth <= 0 || aSHeight <= 0) {
    return NS_OK;
  }

  if ((srcWidth != dstWidth) || (srcHeight != dstHeight)) {
    GdkPixmap *pixmap = 0;
    GdkGC *gc = 0;
    nsRegionGTK clipRgn;

    switch (mAlphaDepth) {
    case 8:
      DrawComposited(aContext, aSurface,
                     srcWidth, srcHeight,
                     dstWidth, dstHeight,
                     dstOrigX, dstOrigY,
                     aDX, aDY,
                     aDWidth, aDHeight);
      break;
    case 1:
      pixmap = gdk_pixmap_new(nsnull, dstWidth, dstHeight, 1);
      if (pixmap) {
        XlibRectStretch(srcWidth, srcHeight,
                        dstWidth, dstHeight,
                        0, 0,
                        0, 0,
                        dstWidth, dstHeight,
                        mAlphaPixmap, pixmap,
                        s1bitGC, s1bitGC, 1);
        gc = gdk_gc_new(drawing->GetDrawable());
        if (gc) {
          gdk_gc_set_clip_origin(gc, dstOrigX, dstOrigY);
          gdk_gc_set_clip_mask(gc, pixmap);
        }
      }

      if (gdk_rgb_get_visual()->depth <= 8) {
        PRUint8 *scaledRGB = (PRUint8 *)nsMemory::Alloc(3*dstWidth*dstHeight);

        if (!scaledRGB)
          return NS_ERROR_OUT_OF_MEMORY;

        RectStretch(mWidth, mHeight,
                    dstWidth, dstHeight,
                    0, 0, dstWidth-1, dstHeight-1,
                    mImageBits, mRowBytes, scaledRGB, 3*dstWidth, 24);

        if (NS_SUCCEEDED(((nsRenderingContextGTK&)aContext).CopyClipRegion(clipRgn))) {
          // we have both a set of rectangles and a bitmap defining the clip
          // let X11 clip to the bitmap, do the rectangles by hand
          nsRegionRectSet *rectSet = nsnull;
          clipRgn.Intersect(aDX, aDY, aDWidth, aDHeight);
          clipRgn.GetRects(&rectSet);
          for (PRUint32 i=0; i<rectSet->mRectsLen; i++) {
            nsRegionRect *rect = &(rectSet->mRects[i]);

            gdk_draw_rgb_image_dithalign(drawing->GetDrawable(), gc,
                                         rect->x, rect->y, rect->width, rect->height,
                                         GDK_RGB_DITHER_MAX, 
                                         scaledRGB + 3*((rect->y-dstOrigY)*dstWidth+(rect->x-dstOrigX)),
                                         3*dstWidth,
                                         (rect->x-dstOrigX), (rect->y-dstOrigY));
          }
          clipRgn.FreeRects(rectSet);
        } else {
          gdk_draw_rgb_image_dithalign(drawing->GetDrawable(), gc,
                                       aDX, aDY, aDWidth, aDHeight,
                                       GDK_RGB_DITHER_MAX, 
                                       scaledRGB + 3*((aDY-dstOrigY)*dstWidth+(aDX-dstOrigX)),
                                       3*dstWidth,
                                       (aDX-dstOrigX), (aDY-dstOrigY));
        }
        nsMemory::Free(scaledRGB);
      } else {
        if (NS_SUCCEEDED(((nsRenderingContextGTK&)aContext).CopyClipRegion(clipRgn))) {
          // we have both a set of rectangles and a bitmap defining the clip
          // let X11 clip to the bitmap, do the rectangles by hand
          nsRegionRectSet *rectSet = nsnull;
          clipRgn.Intersect(aDX, aDY, aDWidth, aDHeight);
          clipRgn.GetRects(&rectSet);
          for (PRUint32 i=0; i<rectSet->mRectsLen; i++) {
            nsRegionRect *rect = &(rectSet->mRects[i]);
            
            XlibRectStretch(srcWidth, srcHeight,
                            dstWidth, dstHeight,
                            dstOrigX, dstOrigY,
                            rect->x, rect->y,
                            rect->width, rect->height,
                            mImagePixmap, drawing->GetDrawable(),
                            gc, sXbitGC, gdk_rgb_get_visual()->depth);
          }
          clipRgn.FreeRects(rectSet);
        } else {
          // only a mask
          XlibRectStretch(srcWidth, srcHeight,
                          dstWidth, dstHeight,
                          dstOrigX, dstOrigY,
                          aDX, aDY,
                          aDWidth, aDHeight,
                          mImagePixmap, drawing->GetDrawable(),
                          gc, sXbitGC, gdk_rgb_get_visual()->depth);
        }
      }

      break;
    case 0:
      if (!gc)
        gc = ((nsRenderingContextGTK&)aContext).GetGC();

      if (gdk_rgb_get_visual()->depth <= 8) {
        PRUint8 *scaledRGB = (PRUint8 *)nsMemory::Alloc(3*dstWidth*dstHeight);
        RectStretch(mWidth, mHeight,
                    dstWidth, dstHeight,
                    0, 0, dstWidth-1, dstHeight-1,
                    mImageBits, mRowBytes, scaledRGB, 3*dstWidth, 24);
    
        gdk_draw_rgb_image_dithalign(drawing->GetDrawable(), gc,
                                     aDX, aDY, aDWidth, aDHeight,
                                     GDK_RGB_DITHER_MAX, 
                                     scaledRGB + 3*((aDY-dstOrigY)*dstWidth+(aDX-dstOrigX)),
                                     3*dstWidth,
                                     (aDX-dstOrigX), (aDY-dstOrigY));

        nsMemory::Free(scaledRGB);
      }
      else
        XlibRectStretch(srcWidth, srcHeight,
                        dstWidth, dstHeight,
                        dstOrigX, dstOrigY,
                        aDX, aDY,
                        aDWidth, aDHeight,
                        mImagePixmap, drawing->GetDrawable(),
                        gc, sXbitGC, gdk_rgb_get_visual()->depth);
      break;
    }
    if (gc)
      gdk_gc_unref(gc);
    if (pixmap)
      gdk_pixmap_unref(pixmap);

    mFlags = 0;
    return NS_OK;
  }

  // now start drawing...

  if (mAlphaDepth==8) {
    DrawComposited(aContext, aSurface, 
                   srcWidth, srcHeight,
                   dstWidth, dstHeight,
                   aDX-aSX, aDY-aSY,
                   aDX, aDY,
                   aDWidth, aDHeight);
    return NS_OK;
  }

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

  nsRegionGTK clipRgn;
  if (mAlphaPixmap &&
      NS_SUCCEEDED(((nsRenderingContextGTK&)aContext).CopyClipRegion(clipRgn))) {
    // we have both a set of rectangles and a bitmap defining the clip
    // let X11 clip to the bitmap, do the rectangles by hand
    nsRegionRectSet *rectSet = nsnull;
    clipRgn.Intersect(aDX, aDY, aSWidth, aSHeight);
    clipRgn.GetRects(&rectSet);
    for (PRUint32 i=0; i<rectSet->mRectsLen; i++) {
      nsRegionRect *rect = &(rectSet->mRects[i]);
      gdk_window_copy_area(drawing->GetDrawable(),      // dest window
                           copyGC,                      // gc
                           rect->x,                     // xdest
                           rect->y,                     // ydest
                           mImagePixmap,                // source window
                           aSX+(rect->x-aDX),           // xsrc
                           aSY+(rect->y-aDY),           // ysrc
                           rect->width,                 // width
                           rect->height);               // height
    }
    clipRgn.FreeRects(rectSet);
  } else {
    // normal case - let X11 take care of all the clipping
    gdk_window_copy_area(drawing->GetDrawable(),      // dest window
                         copyGC,                      // gc
                         aDX,                         // xdest
                         aDY,                         // ydest
                         mImagePixmap,                // source window
                         aSX,                         // xsrc
                         aSY,                         // ysrc
                         aSWidth,                     // width
                         aSHeight);                   // height
  }
 
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
                             XImage *ximage, unsigned char *readData, unsigned char *srcData)
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
    unsigned char *baseRow   = srcData     + y*ximage->bytes_per_line;
    unsigned char *targetRow = readData    + 3*(y*ximage->width);
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
                             XImage *ximage, unsigned char *readData, unsigned char *srcData)
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
    unsigned char *baseRow   = srcData     + y*ximage->bytes_per_line;
    unsigned char *targetRow = readData    + 3*(y*ximage->width);
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
                             XImage *ximage, unsigned char *readData, unsigned char *srcData)
{
  GdkVisual *visual   = gdk_rgb_get_visual();

  unsigned *redScale   = (visual->red_prec   == 5) ? scaled5 : scaled6;
  unsigned *greenScale = (visual->green_prec == 5) ? scaled5 : scaled6;
  unsigned *blueScale  = (visual->blue_prec  == 5) ? scaled5 : scaled6;

  for (unsigned y=0; y<height; y++) {
    unsigned char *baseRow   = srcData     + y*ximage->bytes_per_line;
    unsigned char *targetRow = readData    + 3*(y*ximage->width);
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
                                  XImage *ximage, unsigned char *readData, unsigned char *srcData)
{
  GdkVisual *visual     = gdk_rgb_get_visual();
  GdkColormap *colormap = gdk_rgb_get_cmap();

  // flip bytes
  if (flipBytes && (ximage->bits_per_pixel>=16)) {
    for (int row=0; row<ximage->height; row++) {
      unsigned char *ptr = srcData + row*ximage->bytes_per_line;
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

  for (unsigned row=0; row<height; row++) {
    unsigned char *ptr = srcData + row*ximage->bytes_per_line;
    unsigned char *target = readData+3*row*ximage->width;
    for (unsigned col=0; col<width; col++) {
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
    unsigned char *targetRow = readData+3*y*ximage->width;
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
                           PRInt32 srcWidth, PRInt32 srcHeight,
                           PRInt32 dstWidth, PRInt32 dstHeight,
                           PRInt32 dstOrigX, PRInt32 dstOrigY,
                           PRInt32 aDX, PRInt32 aDY,
                           PRInt32 aDWidth, PRInt32 aDHeight)
{
  nsDrawingSurfaceGTK* drawing = (nsDrawingSurfaceGTK*) aSurface;
  GdkVisual *visual = gdk_rgb_get_visual();
    
  Display *dpy = GDK_WINDOW_XDISPLAY(drawing->GetDrawable());
  Drawable drawable = GDK_WINDOW_XWINDOW(drawing->GetDrawable());

  int readX, readY;
  unsigned readWidth, readHeight, destX, destY;

  destX = aDX-dstOrigX;
  destY = aDY-dstOrigY;
  readX = aDX;
  readY = aDY;
  readWidth = aDWidth;
  readHeight = aDHeight;

//  fprintf(stderr, "dstOrigX=%d dstOrigY=%d, dstWidth=%u dstHeight=%u\n", dstOrigX, dstOrigY, dstWidth, dstHeight);
//  fprintf(stderr, "srcWidth=%u srcHeight=%u\n", srcWidth, srcHeight);
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
  if ((srcWidth!=dstWidth) || (srcHeight!=dstHeight)) {
    PRUint32 x1, y1, x2, y2;
    x1 = destX*srcWidth/dstWidth;
    y1 = destY*srcHeight/dstHeight;
    x2 = (destX+aDWidth)*srcWidth/dstWidth;
    y2 = (destY+aDHeight)*srcHeight/dstHeight;

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
    RectStretch(srcWidth, srcHeight,
                dstWidth, dstHeight,
                destX, destY,
                destX+aDWidth-1, destY+aDHeight-1,
                mImageBits, mRowBytes, scaledImage, 3*readWidth, 24);
    RectStretch(srcWidth, srcHeight,
                dstWidth, dstHeight,
                destX, destY,
                destX+aDWidth-1, destY+aDHeight-1,
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
                     readWidth, readHeight, ximage, readData, (unsigned char *)ximage->data);
  else if ((ximage->bits_per_pixel==24) &&
           (visual->red_prec == 8) && 
           (visual->green_prec == 8) &&
           (visual->blue_prec == 8))
    DrawComposited24(isLSB, flipBytes, 
                     imageOrigin, imageStride,
                     alphaOrigin, alphaStride, 
                     readWidth, readHeight, ximage, readData, (unsigned char *)ximage->data);
  else if ((ximage->bits_per_pixel==16) &&
           ((visual->red_prec == 5)   || (visual->red_prec == 6)) &&
           ((visual->green_prec == 5) || (visual->green_prec == 6)) &&
           ((visual->blue_prec == 5)  || (visual->blue_prec == 6)))
    DrawComposited16(isLSB, flipBytes,
                     imageOrigin, imageStride,
                     alphaOrigin, alphaStride, 
                     readWidth, readHeight, ximage, readData, (unsigned char *)ximage->data);
  else
    DrawCompositedGeneral(isLSB, flipBytes,
                     imageOrigin, imageStride,
                     alphaOrigin, alphaStride, 
                     readWidth, readHeight, ximage, readData, (unsigned char *)ximage->data);

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


void
nsImageGTK::DrawCompositeTile(nsIRenderingContext &aContext,
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
  unsigned readWidth, readHeight;
  PRInt32 destX, destY;

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

  XImage *ximage = XGetImage(dpy, drawable,
                             readX, readY, readWidth, readHeight, 
                             AllPlanes, ZPixmap);

  NS_ASSERTION((ximage!=NULL), "XGetImage() failed");
  if (!ximage)
    return;

  unsigned char *readData = 
    (unsigned char *)nsMemory::Alloc(3*readWidth*readHeight);

  PRBool isLSB;
  unsigned test = 1;
  isLSB = (((char *)&test)[0]) ? 1 : 0;

  PRBool flipBytes = 
    ( isLSB && ximage->byte_order != LSBFirst) ||
    (!isLSB && ximage->byte_order == LSBFirst);


  PRUint8 *imageOrigin, *alphaOrigin;
  PRUint32 imageStride, alphaStride;
  PRUint32 compX, compY;
  PRUint8 *compTarget, *compSource;

  imageStride = mRowBytes;
  alphaStride = mAlphaRowBytes;

  if (destX==mWidth)
    destX = 0;
  if (destY==mHeight)
    destY = 0;

  for (unsigned y=0; y<readHeight; y+=compY) {
    if (y==0) {
      compY = PR_MIN(mHeight-destY, readHeight-y);
    } else {
      destY = 0;
      compY = PR_MIN(mHeight, readHeight-y);
    }

    compTarget = readData + 3*y*ximage->width;
    compSource = (unsigned char *)ximage->data + y*ximage->bytes_per_line;

    for (unsigned x=0; x<readWidth; x+=compX) {
      if (x==0) {
        compX = PR_MIN(mWidth-destX, readWidth-x);
        imageOrigin = mImageBits + destY*mRowBytes + 3*destX;
        alphaOrigin = mAlphaBits + destY*mAlphaRowBytes + destX;
      } else {
        compX = PR_MIN(mWidth, readWidth-x);
        imageOrigin = mImageBits + destY*mRowBytes;
        alphaOrigin = mAlphaBits + destY*mAlphaRowBytes;
      }

      if ((ximage->bits_per_pixel==32) &&
          (visual->red_prec == 8) &&
          (visual->green_prec == 8) &&
          (visual->blue_prec == 8))
        DrawComposited32(isLSB, flipBytes, 
                         imageOrigin, imageStride,
                         alphaOrigin, alphaStride, 
                         compX, compY, ximage, compTarget, compSource);
      else if ((ximage->bits_per_pixel==24) &&
               (visual->red_prec == 8) && 
               (visual->green_prec == 8) &&
               (visual->blue_prec == 8))
        DrawComposited24(isLSB, flipBytes, 
                         imageOrigin, imageStride,
                         alphaOrigin, alphaStride, 
                         compX, compY, ximage, compTarget, compSource);
      else if ((ximage->bits_per_pixel==16) &&
               ((visual->red_prec == 5)   || (visual->red_prec == 6)) &&
               ((visual->green_prec == 5) || (visual->green_prec == 6)) &&
               ((visual->blue_prec == 5)  || (visual->blue_prec == 6)))
        DrawComposited16(isLSB, flipBytes,
                         imageOrigin, imageStride,
                         alphaOrigin, alphaStride, 
                         compX, compY, ximage, compTarget, compSource);
      else
        DrawCompositedGeneral(isLSB, flipBytes,
                              imageOrigin, imageStride,
                              alphaOrigin, alphaStride, 
                              compX, compY, ximage, compTarget, compSource);

      compTarget += 3*compX;
      compSource += (ximage->bits_per_pixel*compX)/8;
    }
  }

  GdkGC *imageGC = ((nsRenderingContextGTK&)aContext).GetGC();
  gdk_draw_rgb_image(drawing->GetDrawable(), imageGC,
                     readX, readY, readWidth, readHeight,
                     GDK_RGB_DITHER_MAX,
                     readData, 3*readWidth);
  gdk_gc_unref(imageGC);

  XDestroyImage(ximage);
  nsMemory::Free(readData);
  mFlags = 0;
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
#ifdef MOZ_WIDGET_GTK2
    gdk_drawable_set_colormap(GDK_DRAWABLE(mImagePixmap), gdk_rgb_get_colormap());
#endif
  }

    // Ditto for the clipmask
  if ((!mAlphaPixmap) && (mAlphaDepth==1)) {
    mAlphaPixmap = gdk_pixmap_new(nsnull, aWidth, aHeight, 1);

    // Need an XImage for clipmask updates (XPutImage)
    mAlphaXImage = XCreateImage(GDK_WINDOW_XDISPLAY(mAlphaPixmap),
                                GDK_VISUAL_XVISUAL(gdk_rgb_get_visual()),
                                1, /* visual depth...1 for bitmaps */
                                XYPixmap,
                                0, /* x offset, XXX fix this */
                                (char *)mAlphaBits,  /* cast away our sign. */
                                aWidth,
                                aHeight,
                                32,/* bitmap pad */
                                mAlphaRowBytes); /* bytes per line */

    mAlphaXImage->bits_per_pixel=1;

    /* Image library always places pixels left-to-right MSB to LSB */
    mAlphaXImage->bitmap_bit_order = MSBFirst;

    /* This definition doesn't depend on client byte ordering
       because the image library ensures that the bytes in
       bitmask data are arranged left to right on the screen,
       low to high address in memory. */
    mAlphaXImage->byte_order = MSBFirst;

    if (!s1bitGC) {
      GdkColor fg = { 1, 0, 0, 0 };
      s1bitGC = gdk_gc_new(mAlphaPixmap);
      gdk_gc_set_foreground(s1bitGC, &fg);
    }
  }

  if (!sXbitGC)
    sXbitGC = gdk_gc_new(mImagePixmap);
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
#ifdef TRACE_IMAGE_ALLOCATION
  printf("nsImageGTK::Draw(this=%p,x=%d,y=%d,width=%d,height=%d)\n",
         this,
         aX, aY,
         aWidth, aHeight);
#endif

  return Draw(aContext, aSurface,
              0, 0, mWidth, mHeight,
              aX, aY, mWidth, mHeight);
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
                                   PRInt32 aPadX, PRInt32 aPadY,
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

  if ((mAlphaDepth==1) && mIsSpacer)
    return NS_OK;

  if (mDecodedX2 < mDecodedX1 || mDecodedY2 < mDecodedY1)
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

  if (partial || (mAlphaDepth == 8) || (aPadX || aPadY)) {
    PRInt32 aY0 = aTileRect.y - aSYOffset,
            aX0 = aTileRect.x - aSXOffset,
            aY1 = aTileRect.y + aTileRect.height,
            aX1 = aTileRect.x + aTileRect.width;

    // Set up clipping and call Draw().
    aContext.PushState();
    ((nsRenderingContextGTK&)aContext).SetClipRectInPixels(
      aTileRect, nsClipCombine_kIntersect);
    ((nsRenderingContextGTK&)aContext).UpdateGC();

    if (mAlphaDepth==8) {
      DrawCompositeTile(aContext, aSurface,
                        aSXOffset, aSYOffset, mWidth, mHeight,
                        aTileRect.x, aTileRect.y,
                        aTileRect.width, aTileRect.height);
    } else {
#ifdef DEBUG_TILING
      printf("Warning: using slow tiling\n");
#endif
      for (PRInt32 y = aY0; y < aY1; y += mHeight + aPadY)
        for (PRInt32 x = aX0; x < aX1; x += mWidth + aPadX)
          Draw(aContext,aSurface, x,y,
               PR_MIN(validWidth, aX1-x),
               PR_MIN(validHeight, aY1-y));
    }

    aContext.PopState();

    return NS_OK;
  }

  if (mAlphaDepth == 1) {
    GdkGC *tileGC;
    GdkGCValues values;
    GdkGCValuesMask valuesMask;

    memset(&values, 0, sizeof(GdkGCValues));
    values.fill = GDK_STIPPLED;
    values.function = GDK_AND;
    values.stipple = mAlphaPixmap;
    values.ts_x_origin = aTileRect.x - aSXOffset;
    values.ts_y_origin = aTileRect.y - aSYOffset;
    valuesMask = GdkGCValuesMask(GDK_GC_FOREGROUND | GDK_GC_FUNCTION | 
                                 GDK_GC_FILL | GDK_GC_STIPPLE | 
                                 GDK_GC_TS_X_ORIGIN | GDK_GC_TS_Y_ORIGIN);
    tileGC = gdk_gc_new_with_values(drawing->GetDrawable(), &values, valuesMask);
    
    gdk_draw_rectangle(drawing->GetDrawable(), tileGC, PR_TRUE,
                       aTileRect.x, aTileRect.y,
                       aTileRect.width, aTileRect.height);

    gdk_gc_set_fill(tileGC, GDK_TILED);
    gdk_gc_set_function(tileGC, GDK_OR);
    gdk_gc_set_tile(tileGC, mImagePixmap);

    gdk_draw_rectangle(drawing->GetDrawable(), tileGC, PR_TRUE,
                       aTileRect.x, aTileRect.y,
                       aTileRect.width, aTileRect.height);

    gdk_gc_unref(tileGC);
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
  if (!mOptimized)
    UpdateCachedImage();

  if (mAlphaBits && mTrueAlphaBits) {
    // 8-bit alpha image turned out to be 1-bit - blacken transparent
    // areas so that we can draw it using the fast tile path
    for (PRInt32 y = 0; y < mHeight; y++)
      for (PRInt32 x = 0; x < mWidth; x++)
        if (!mTrueAlphaBits[y * mTrueAlphaRowBytes + x]) {
          mImageBits[y * mRowBytes + 3 * x]     = 0;
          mImageBits[y * mRowBytes + 3 * x + 1] = 0;
          mImageBits[y * mRowBytes + 3 * x + 2] = 0;
        }
    nsRect rect(0, 0, mWidth, mHeight);
    ImageUpdated(nsnull, 0, &rect);
    UpdateCachedImage();
  }

  if ((gdk_rgb_get_visual()->depth > 8) && (mAlphaDepth != 8)) {
    if(nsnull != mImageBits) {
      delete[] mImageBits;
      mImageBits = nsnull;
    }

    if (nsnull != mAlphaBits) {
      delete[] mAlphaBits;
      mAlphaBits = nsnull;
    }
  }
    
  if (mTrueAlphaBits) {
    delete[] mTrueAlphaBits;
    mTrueAlphaBits = nsnull;
  }

  if ((mAlphaDepth==0) && mAlphaPixmap) {
    gdk_pixmap_unref(mAlphaPixmap);
    mAlphaPixmap = nsnull;
  }

  mOptimized = PR_TRUE;

  return NS_OK;
}

//------------------------------------------------------------
// lock the image pixels
NS_IMETHODIMP
nsImageGTK::LockImagePixels(PRBool aMaskPixels)
{
  if (!mOptimized)
    return NS_OK;

  if (aMaskPixels) {
    if (mAlphaDepth != 1 || !mAlphaPixmap)
      return NS_OK;

    XImage *xmask = XGetImage(GDK_WINDOW_XDISPLAY(mAlphaPixmap),
                              GDK_WINDOW_XWINDOW(mAlphaPixmap),
                              0, 0, mWidth, mHeight,
                              AllPlanes, XYPixmap);

    mAlphaBits = new PRUint8[mAlphaRowBytes * mHeight];
    memset(mAlphaBits, 0, mAlphaRowBytes * mHeight);

    for (PRInt32 y = 0; y < mHeight; ++y) {
      PRUint8 *alphaTarget = mAlphaBits + y*mAlphaRowBytes;
      PRUint32 alphaBitPos = 7;

      for (PRInt32 x = 0; x < mWidth; ++x) {
        *alphaTarget |= (XGetPixel(xmask, x, y) << alphaBitPos);
        if (alphaBitPos-- == 0) {
          ++alphaTarget;
          alphaBitPos = 7;
        }
      }
    }

    XDestroyImage(xmask);
    return NS_OK;
  }

  if (!mImagePixmap)
    return NS_OK;

  XImage *ximage, *xmask=0;
  unsigned pix;

  ximage = XGetImage(GDK_WINDOW_XDISPLAY(mImagePixmap),
                     GDK_WINDOW_XWINDOW(mImagePixmap),
                     0, 0, mWidth, mHeight,
                     AllPlanes, ZPixmap);

  if ((mAlphaDepth==1) && mAlphaPixmap)
    xmask = XGetImage(GDK_WINDOW_XDISPLAY(mAlphaPixmap),
                      GDK_WINDOW_XWINDOW(mAlphaPixmap),
                      0, 0, mWidth, mHeight,
                      AllPlanes, XYPixmap);

  mImageBits = (PRUint8*) new PRUint8[mSizeImage];
  GdkVisual *visual = gdk_rgb_get_visual();
  GdkColormap *colormap = gdk_rgb_get_cmap();

  unsigned redScale, greenScale, blueScale, redFill, greenFill, blueFill;
  redScale   = 8 - visual->red_prec;
  greenScale = 8 - visual->green_prec;
  blueScale  = 8 - visual->blue_prec;
  redFill    = 0xff >> visual->red_prec;
  greenFill  = 0xff >> visual->green_prec;
  blueFill   = 0xff >> visual->blue_prec;

  /* read back the image in the slowest (but simplest) way possible... */
  for (PRInt32 y=0; y<mHeight; y++) {
    PRUint8 *target = mImageBits + y*mRowBytes;
    for (PRInt32 x=0; x<mWidth; x++) {
      if (xmask && !XGetPixel(xmask, x, y)) {
        *target++ = 0xFF;
        *target++ = 0xFF;
        *target++ = 0xFF;
      } else {
        pix = XGetPixel(ximage, x, y);
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
  }

  XDestroyImage(ximage);
  if (xmask)
    XDestroyImage(xmask);

  return NS_OK;
}

//------------------------------------------------------------
// unlock the image pixels. nothing to do on gtk
NS_IMETHODIMP
nsImageGTK::UnlockImagePixels(PRBool aMaskPixels)
{
  if (mOptimized)
    Optimize(nsnull);

  return NS_OK;
} 

NS_IMETHODIMP nsImageGTK::DrawToImage(nsIImage* aDstImage,
                                      nscoord aDX, nscoord aDY,
                                      nscoord aDWidth, nscoord aDHeight)
{
  nsImageGTK *dest = NS_STATIC_CAST(nsImageGTK *, aDstImage);

  if (!dest)
    return NS_ERROR_FAILURE;
    
  if (aDX >= dest->mWidth || aDY >= dest->mHeight)
    return NS_OK;

  PRUint8 *rgbPtr=0, *alphaPtr=0;
  PRUint32 rgbStride, alphaStride;

  rgbPtr = mImageBits;
  rgbStride = mRowBytes;
  alphaPtr = mAlphaBits;
  alphaStride = mAlphaRowBytes;

  PRInt32 y;
  PRInt32 ValidWidth = ( aDWidth < ( dest->mWidth - aDX ) ) ? aDWidth : ( dest->mWidth - aDX ); 
  PRInt32 ValidHeight = ( aDHeight < ( dest->mHeight - aDY ) ) ? aDHeight : ( dest->mHeight - aDY );

  // now composite the two images together
  switch (mAlphaDepth) {
  case 1:
    {
      PRUint8 *dst = dest->mImageBits + aDY*dest->mRowBytes + 3*aDX;
      PRUint8 *dstAlpha = dest->mAlphaBits + aDY*dest->mAlphaRowBytes;
      PRUint8 *src = rgbPtr;
      PRUint8 *alpha = alphaPtr;
      PRUint8 offset = aDX & 0x7; // x starts at 0
      int iterations = (ValidWidth+7)/8; // round up

      for (y=0; y<ValidHeight; y++) {
        for (int x=0; x<ValidWidth; x += 8, dst += 3*8, src += 3*8) {
          PRUint8 alphaPixels = *alpha++;
          if (alphaPixels == 0) {
            // all 8 transparent; jump forward
            continue;
          }

          // 1 or more bits are set, handle dstAlpha now - may not be aligned.
          // Are all 8 of these alpha pixels used?
          if (x+7 >= ValidWidth) {
            alphaPixels &= 0xff << (8 - (ValidWidth-x)); // no, mask off unused
            if (alphaPixels == 0)
              continue;  // no 1 alpha pixels left
          }
          if (offset == 0) {
            dstAlpha[(aDX+x)>>3] |= alphaPixels; // the cheap aligned case
          }
          else {
            dstAlpha[(aDX+x)>>3]       |= alphaPixels >> offset;
            // avoid write if no 1's to write - also avoids going past end of array
            PRUint8 alphaTemp = alphaPixels << (8U - offset);
            if (alphaTemp & 0xff)
              dstAlpha[((aDX+x)>>3) + 1] |= alphaTemp;
          }
          
          if (alphaPixels == 0xff) {
            // fix - could speed up by gathering a run of 0xff's and doing 1 memcpy
            // all 8 pixels set; copy and jump forward
            memcpy(dst,src,8*3);
            continue;
          }
          else {
            // else mix of 1's and 0's in alphaPixels, do 1 bit at a time
            // Don't go past end of line!
            PRUint8 *d = dst, *s = src;
            for (PRUint8 aMask = 1<<7, j = 0; aMask && j < ValidWidth-x; aMask >>= 1, j++) {
              // if this pixel is opaque then copy into the destination image
              if (alphaPixels & aMask) {
                // might be faster with *d++ = *s++ 3 times?
                d[0] = s[0];
                d[1] = s[1];
                d[2] = s[2];
                // dstAlpha bit already set
              }
              d += 3;
              s += 3;
            }
          }
        }
        // at end of each line, bump pointers.  Use wordy code because of
        // bug 127455 to avoid possibility of unsigned underflow
        dst = (dst - 3*8*iterations) + dest->mRowBytes;
        src = (src - 3*8*iterations) + rgbStride;
        alpha = (alpha - iterations) + alphaStride;
        dstAlpha += dest->mAlphaRowBytes;
      }
    }
    break;
  case 0:
  default:
    for (y=0; y<ValidHeight; y++)
      memcpy(dest->mImageBits + (y+aDY)*dest->mRowBytes + 3*aDX, 
             rgbPtr + y*rgbStride,
             3*ValidWidth);
  }

  nsRect rect(aDX, aDY, ValidWidth, ValidHeight);
  dest->ImageUpdated(nsnull, 0, &rect);

  return NS_OK;
}
