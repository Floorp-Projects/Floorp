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
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include "nsImageGTK.h"
#include "nsRenderingContextGTK.h"

#include "nspr.h"

#define IsFlagSet(a,b) ((a) & (b))

// this is just a little bit faster than the GDK code
#define USE_XLIB_ALPHA_MASKING

// Defining this will trace the allocation of images.  This includes
// ctor, dtor and update.
#undef TRACE_IMAGE_ALLOCATION

#undef CHEAP_PERFORMANCE_MEASURMENT

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
  mAlphaBits = nsnull;
  mAlphaPixmap = nsnull;
  mImagePixmap = nsnull;
  mAlphaDepth = 0;
  mRowBytes = 0;
  mSizeImage = 0;
  mAlphaHeight = 0;
  mAlphaWidth = 0;
  mConvertedBits = nsnull;
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
   delete[] (PRUint8*)mImageBits;
   mImageBits = nsnull;
  }

  if (nsnull != mAlphaBits) {
    delete[] (PRUint8*)mAlphaBits;
    mAlphaBits = nsnull;
  }

  if (nsnull != mAlphaPixmap) {
    gdk_pixmap_unref(mAlphaPixmap);
    mAlphaPixmap = nsnull;
  }

  SetDecodedRect(0,0,0,0);  //init


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
      mAlphaRowBytes = aWidth;
      mAlphaDepth = 8;

      // 32-bit align each row
      mAlphaRowBytes = (mAlphaRowBytes + 3) & ~0x3;
      mAlphaBits = new PRUint8[mAlphaRowBytes * aHeight];
      mAlphaWidth = aWidth;
      mAlphaHeight = aHeight;
      g_print("TODO: want an 8bit mask for an image..\n");
      break;
  }

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

  mFlags = aFlags; // this should be 0'd out by Draw()

}

#ifdef CHEAP_PERFORMANCE_MEASURMENT
static PRTime gConvertTime, gAlphaTime, gAlphaEnd, gStartTime, gPixmapTime, gEndTime;
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
                      ((nsRenderingContextGTK&)aContext).GetGC(),
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

  // make a copy of the GC so that we can completly restore the things we are about to change
  GdkGC *copyGC;
  copyGC = gdk_gc_new(drawing->GetDrawable());
  gdk_gc_copy(copyGC, ((nsRenderingContextGTK&)aContext).GetGC());

#ifdef CHEAP_PERFORMANCE_MEASURMENT
  gStartTime = gPixmapTime = PR_Now();
  gAlphaTime = PR_Now();
#endif


  // this code is a little bit faster than the GTK code below
#ifdef USE_XLIB_ALPHA_MASKING
  XImage *x_image = nsnull;
  Pixmap pixmap = 0;
  Display *dpy = nsnull;
  Visual *visual = nsnull;

  // Create gc clip-mask on demand
  if (mAlphaBits && IsFlagSet(nsImageUpdateFlags_kBitsChanged, mFlags)) {

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
      GdkGCValues gcv;
      memset(&gcv, 0, sizeof(GdkGCValues));
      gcv.function = GDK_COPY;
      s1bitGC = gdk_gc_new_with_values(mAlphaPixmap, &gcv, GDK_GC_FUNCTION);
    }

    XPutImage(dpy, pixmap, GDK_GC_XGC(s1bitGC), x_image, 0, 0, 0, 0,
              aWidth, aHeight);

    // Now we are done with the temporary image
    x_image->data = 0;          /* Don't free the IL_Pixmap's bits. */
    XDestroyImage(x_image);
  }



#else /* USE_XLIB_ALPHA_MASKING */
  if (mAlphaBits && IsFlagSet(nsImageUpdateFlags_kBitsChanged, mFlags)) {
    nsIRegion *clipRegion = nsnull;
    GdkImage *img;
    GdkGC *gc;

    if (!mAlphaPixmap) {
      mAlphaPixmap = gdk_pixmap_new(nsnull, aWidth, aHeight, 1);
    }

    gc = gdk_gc_new(mAlphaPixmap);
    gdk_gc_set_function(gc, GDK_COPY_INVERT);

    img = gdk_image_new_bitmap(gdk_rgb_get_visual(), mAlphaBits, aWidth, aHeight);
    GDK_IMAGE_XIMAGE(img)->bits_per_pixel = 1;
    GDK_IMAGE_XIMAGE(img)->bytes_per_line = mAlphaRowBytes;
    GDK_IMAGE_XIMAGE(img)->bitmap_pad = 32;
    GDK_IMAGE_XIMAGE(img)->bitmap_bit_order = MSBFirst;
    GDK_IMAGE_XIMAGE(img)->byte_order = MSBFirst;
    gdk_draw_image(mAlphaPixmap,gc,img,0,0,0,0,aWidth,aHeight);
    GDK_IMAGE_XIMAGE(img)->data = 0;
    gdk_image_destroy(img);
    gdk_gc_unref(gc);
  }
#endif


#ifdef CHEAP_PERFORMANCE_MEASURMENT
    gAlphaEnd = PR_Now();
    gPixmapTime = PR_Now();
#endif

  // Render unique image bits onto an off screen pixmap only once
  // The image bits can change as a result of ImageUpdated() - for
  // example: animated GIFs.
  if (!mImagePixmap) {
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


  }


  PRInt32 validX, validY, validWidth, validHeight;

  validX = 0;
  validY = 0;
  validWidth = aWidth;
  validHeight = aHeight;

  // limit the image rectangle to the size of the image data which
  // has been validated.
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
    validWidth -= mDecodedX1;
    validX = mDecodedX1;
  }

  if (IsFlagSet(nsImageUpdateFlags_kBitsChanged, mFlags)) {

    if (!sXbitGC) {
      GdkGCValues gcv;
      memset(&gcv, 0, sizeof(GdkGCValues));
      gcv.function = GDK_COPY;
      sXbitGC = gdk_gc_new(mImagePixmap);
    }
    /*    GdkGC *ggc;
          ggc = gdk_gc_new(mImagePixmap);*/

    // Render the image bits into an off screen pixmap
    gdk_draw_rgb_image (mImagePixmap,
                        sXbitGC,
                        validX, validY, validWidth, validHeight,
                        GDK_RGB_DITHER_MAX,
                        mImageBits, mRowBytes);
  }

  if (mAlphaPixmap)
  {
    // Setup gc to use the given alpha-pixmap for clipping
    XGCValues xvalues;
    unsigned long xvalues_mask = 0;
    xvalues.clip_x_origin = aX;
    xvalues.clip_y_origin = aY;
    xvalues.clip_mask = GDK_WINDOW_XWINDOW(mAlphaPixmap);
    xvalues_mask = GCClipXOrigin | GCClipYOrigin | GCClipMask;

    XChangeGC(GDK_DISPLAY(), GDK_GC_XGC(copyGC), xvalues_mask, &xvalues);

    /* this causes 2 ChangeGC's to happen.
      gdk_gc_set_clip_mask(copyGC, mAlphaPixmap);
      gdk_gc_set_clip_origin(copyGC, aX, aY);
    */
  }

#ifdef TRACE_IMAGE_ALLOCATION
  printf("nsImageGTK::Draw(this=%p) gdk_draw_pixmap(x=%d,y=%d,width=%d,height=%d)\n",
         this,
         validX+aX,
         validY+aY,
         validWidth,                  
         validHeight);
#endif

  

  // copy our off screen pixmap onto the window.
  gdk_window_copy_area(drawing->GetDrawable(),      // dest window
                       copyGC,                      // gc
                       validX+aX,                   // xsrc
                       validY+aY,                   // ysrc
                       mImagePixmap,                // source window
                       validX,                      // xdest
                       validY,                      // ydest
                       validWidth,                  // width
                       validHeight);                // height


  gdk_gc_unref(copyGC);


#ifdef CHEAP_PERFORMANCE_MEASURMENT
  gEndTime = PR_Now();
  printf("nsImageGTK::Draw(this=%p,w=%d,h=%d) total=%lld pixmap=%lld, alpha=%lld\n",
         this,
         aWidth, aHeight,
         gEndTime - gStartTime,
         gPixmapTime - gStartTime,
         gAlphaEnd - gAlphaTime);
#endif

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
