/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include "nsImageGTK.h"
#include "nsRenderingContextGTK.h"

#include "nspr.h"

#define IsFlagSet(a,b) (a & b)

#undef CHEAP_PERFORMANCE_MEASURMENT


// Defining this will trace the allocation of images.  This includes
// ctor, dtor and update.
#undef TRACE_IMAGE_ALLOCATION

static NS_DEFINE_IID(kIImageIID, NS_IIMAGE_IID);

//------------------------------------------------------------

nsImageGTK::nsImageGTK()
{
  NS_INIT_REFCNT();
  mImageBits = nsnull;
  mWidth = 0;
  mHeight = 0;
  mDepth = 0;
  mAlphaBits = nsnull;
  mAlphaPixmap = nsnull;
  mImagePixmap = nsnull;

#ifdef TRACE_IMAGE_ALLOCATION
  printf("nsImageGTK::nsImageGTK(this=%p)\n",
         this);
#endif
}

//------------------------------------------------------------

nsImageGTK::~nsImageGTK()
{
  if(nsnull != mImageBits) {
    delete[] (PRUint8*)mImageBits;
    mImageBits = nsnull;
  }

  if (nsnull != mAlphaBits) {
    delete[] (PRUint8*)mAlphaBits;
    mAlphaBits = nsnull;
    if (nsnull != mAlphaPixmap) {
      gdk_pixmap_unref(mAlphaPixmap);
    }
  }

  if (nsnull != mImagePixmap) {
    gdk_pixmap_unref(mImagePixmap);
  }

#ifdef TRACE_IMAGE_ALLOCATION
  printf("nsImageGTK::~nsImageGTK(this=%p)\n",
         this);
#endif
}

NS_IMPL_ISUPPORTS(nsImageGTK, kIImageIID);

//------------------------------------------------------------

nsresult nsImageGTK::Init(PRInt32 aWidth, PRInt32 aHeight,
                          PRInt32 aDepth, nsMaskRequirements aMaskRequirements)
{
  g_return_val_if_fail ((aWidth != 0) || (aHeight != 0), NS_ERROR_FAILURE);

  if (nsnull != mImageBits) {
   delete[] (PRUint8*)mImageBits;
   mImageBits = nsnull;
  }

  if (nsnull != mAlphaBits) {
    delete[] (PRUint8*)mAlphaBits;
    mAlphaBits = nsnull;
    if (nsnull != mAlphaPixmap) {
      gdk_pixmap_unref(mAlphaPixmap);
      mAlphaPixmap = nsnull;
    }
  }

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
  ComputMetrics();

  mImageBits = (PRUint8*) new PRUint8[mSizeImage];

  switch(aMaskRequirements)
  {
    case nsMaskRequirements_kNoMask:
      mAlphaBits = nsnull;
      mAlphaWidth = 0;
      mAlphaHeight = 0;
      break;

    case nsMaskRequirements_kNeeds1Bit:
      mAlphaRowBytes = (aWidth + 7) / 8;
      mAlphaDepth = 1;

      // 32-bit align each row
      mAlphaRowBytes = (mAlphaRowBytes + 3) & ~0x3;

      mAlphaBits = new PRUint8[mAlphaRowBytes * aHeight];
      mAlphaWidth = aWidth;
      mAlphaHeight = aHeight;
      break;

    case nsMaskRequirements_kNeeds8Bit:
      mAlphaBits = nsnull;
      mAlphaWidth = 0;
      mAlphaHeight = 0;
      mAlphaDepth = 8;
      g_print("TODO: want an 8bit mask for an image..\n");
      break;
  }

  return NS_OK;
}

//------------------------------------------------------------

PRInt32 nsImageGTK::CalcBytesSpan(PRUint32  aWidth)
{
  PRInt32 spanbytes;

  spanbytes = (aWidth * mDepth) >> 5;

  if (((PRUint32)aWidth * mDepth) & 0x1F)
    spanbytes++;
  spanbytes <<= 2;
  return(spanbytes);
}

void nsImageGTK::ComputMetrics()
{
  mRowBytes = CalcBytesSpan(mWidth);
  mSizeImage = mRowBytes * mHeight;
}

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

//------------------------------------------------------------

// set up the palette to the passed in color array, RGB only in this array
void nsImageGTK::ImageUpdated(nsIDeviceContext *aContext,
                              PRUint8 aFlags,
                              nsRect *aUpdateRect)
{
#ifdef TRACE_IMAGE_ALLOCATION
  printf("nsImageGTK::ImageUpdated(this=%p,%d)\n",
         this,
         aFlags);
#endif

  if (IsFlagSet(nsImageUpdateFlags_kBitsChanged, aFlags)){
    if (nsnull != mAlphaPixmap) {
      gdk_pixmap_unref(mAlphaPixmap);
      mAlphaPixmap = nsnull;
    }

    // mImagePixmap gets created once per unique image bits in Draw()
    // ImageUpdated(nsImageUpdateFlags_kBitsChanged) can cause the
    // image bits to change and mImagePixmap will be unrefed and nulled.
    if (nsnull != mImagePixmap) {
      gdk_pixmap_unref(mImagePixmap);
      mImagePixmap = nsnull;
    }
  }
}

#ifdef CHEAP_PERFORMANCE_MEASURMENT
static PRTime gConvertTime, gStartTime, gPixmapTime, gEndTime;
#endif

// Draw the bitmap, this method has a source and destination coordinates
NS_IMETHODIMP
nsImageGTK::Draw(nsIRenderingContext &aContext, nsDrawingSurface aSurface,
                 PRInt32 aSX, PRInt32 aSY, PRInt32 aSWidth, PRInt32 aSHeight,
                 PRInt32 aDX, PRInt32 aDY, PRInt32 aDWidth, PRInt32 aDHeight)
{
  g_return_val_if_fail ((aSurface != nsnull), NS_ERROR_FAILURE);

  nsDrawingSurfaceGTK *drawing = (nsDrawingSurfaceGTK*)aSurface;

  gdk_draw_rgb_image (drawing->GetDrawable(),
                      drawing->GetGC(),
                      aDX, aDY, aDWidth, aDHeight,
                      GDK_RGB_DITHER_MAX,
                      mImageBits + mRowBytes * aSY + 3 * aDX,
                      mRowBytes);

  return NS_OK;
}

//------------------------------------------------------------

// Draw the bitmap, this draw just has destination coordinates
NS_IMETHODIMP
nsImageGTK::Draw(nsIRenderingContext &aContext,
                 nsDrawingSurface aSurface,
                 PRInt32 aX, PRInt32 aY,
                 PRInt32 aWidth, PRInt32 aHeight)
{
  g_return_val_if_fail ((aSurface != nsnull), NS_ERROR_FAILURE);


  // XXX kipp: this is temporary code until we eliminate the
  // width/height arguments from the draw method.
  if ((aWidth != mWidth) || (aHeight != mHeight)) {
    aWidth = mWidth;
    aHeight = mHeight;
  }

#ifdef TRACE_IMAGE_ALLOCATION
  printf("nsImageGTK::Draw(this=%p,x=%d,y=%d,width=%d,height=%d)\n",
         this,
         aX,
         aY,
         aWidth,
         aHeight);
#endif

  nsDrawingSurfaceGTK* drawing = (nsDrawingSurfaceGTK*) aSurface;

  XImage *x_image = nsnull;
  Pixmap pixmap = 0;
  Display *dpy = nsnull;
  Visual *visual = nsnull;
  GC      gc;
  XGCValues gcv;

#ifdef CHEAP_PERFORMANCE_MEASURMENT
  gStartTime = gPixmapTime = PR_Now();
#endif

  // Create gc clip-mask on demand
  if ((mAlphaBits != nsnull) && (nsnull == mAlphaPixmap))
  {
    mAlphaPixmap = gdk_pixmap_new(nsnull, aWidth, aHeight, 1);

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
    memset(&gcv, 0, sizeof(XGCValues));
    gcv.function = GXcopy;
    gc = XCreateGC(dpy, pixmap, GCFunction, &gcv);
    XPutImage(dpy, pixmap, gc, x_image, 0, 0, 0, 0,
              aWidth, aHeight);
    XFreeGC(dpy, gc);

    // Now we are done with the temporary image
    x_image->data = 0;          /* Don't free the IL_Pixmap's bits. */
    XDestroyImage(x_image);

#ifdef CHEAP_PERFORMANCE_MEASURMENT
    gPixmapTime = PR_Now();
#endif
  }

#if 0
  // this code doesn't work right.  leaving here for future reference.
  if ((mAlphaBits != nsnull) && (nsnull == mAlphaPixmap))
  {
    GdkImage *img;
    GdkGC *gc;
    mAlphaPixmap = gdk_pixmap_new(nsnull, aWidth, aHeight, 1);
    gc = gdk_gc_new(mAlphaPixmap);
    gdk_gc_set_function(gc, GDK_COPY);
    img = gdk_image_new_bitmap(gdk_rgb_get_visual(), mAlphaBits, aWidth, aHeight);
    gdk_draw_image(mAlphaPixmap,gc,img,0,0,0,0,aWidth,aHeight);
    gdk_image_destroy(img);
    gdk_gc_unref(gc);
  }
#endif
  // Render unique image bits onto an off screen pixmap only once
  // The image bits can change as a result of ImageUpdated() - for
  // example: animated GIFs.
  if (nsnull == mImagePixmap)
  {
#ifdef TRACE_IMAGE_ALLOCATION
    printf("nsImageGTK::Draw(this=%p) gdk_pixmap_new(nsnull,width=%d,height=%d,depth=%d)\n",
           this,
           aWidth,
           aHeight,
           mDepth);
#endif

    // Create an off screen pixmap to hold the image bits.
    mImagePixmap = gdk_pixmap_new(nsnull,
                                  aWidth, aHeight,
                                  gdk_rgb_get_visual()->depth);

    // Make sure the clip region is clear, since we are rendering the 
    // image bits to an off screen pixmap and this always happens at the
    // origin.
    gdk_gc_set_clip_origin(drawing->GetGC(), 0, 0);
    gdk_gc_set_clip_mask(drawing->GetGC(), nsnull);

    // Render the image bits into an off screen pixmap
    gdk_draw_rgb_image (mImagePixmap,
                        drawing->GetGC(),
                        0, 0, aWidth, aHeight,
                        GDK_RGB_DITHER_MAX,
                        mImageBits, mRowBytes);
  }

  if (nsnull != mAlphaPixmap)
  {
    // Setup gc to use the given alpha-pixmap for clipping
    gdk_gc_set_clip_mask(drawing->GetGC(), mAlphaPixmap);
    gdk_gc_set_clip_origin(drawing->GetGC(), aX, aY);
  }

#ifdef TRACE_IMAGE_ALLOCATION
  printf("nsImageGTK::Draw(this=%p) gdk_draw_pixmap(x=%d,y=%d,width=%d,height=%d)\n",
         this,
         aX,
         aY,
         aWidth,
         aHeight);
#endif

  // copy our off screen pixmap onto the window.

  gdk_window_copy_area(drawing->GetDrawable(),      // dest window
                       drawing->GetGC(),            // gc
                       aX,                          // xsrc
                       aY,                          // ysrc
                       mImagePixmap,                // source window
                       0,                           // xdest
                       0,                           // ydest
                       aWidth,                      // width
                       aHeight);                    // height

  if (mAlphaPixmap != nsnull)
  {
    // Revert gc to its old clip-mask and origin
    gdk_gc_set_clip_origin(drawing->GetGC(), 0, 0);
    gdk_gc_set_clip_mask(drawing->GetGC(), nsnull);
  }

#ifdef CHEAP_PERFORMANCE_MEASURMENT
  gEndTime = PR_Now();
  printf("nsImageGTK::Draw(this=%p,w=%d,h=%d) total=%lld pixmap=%lld, cvt=%lld\n",
         this,
         aWidth, aHeight,
         gEndTime - gStartTime,
         gPixmapTime - gStartTime,
         gConvertTime - gPixmapTime);
#endif

  return NS_OK;
}


//------------------------------------------------------------

nsresult nsImageGTK::Optimize(nsIDeviceContext* aContext)
{
  return NS_OK;
}
