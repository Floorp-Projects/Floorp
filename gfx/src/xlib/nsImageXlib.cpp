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

#include "nsImageXlib.h"
#include "nsDrawingSurfaceXlib.h"
#include "xlibrgb.h"
#include "prlog.h"

static NS_DEFINE_IID(kIImageIID, NS_IIMAGE_IID);

static PRLogModuleInfo *ImageXlibLM = PR_NewLogModule("ImageXlib");

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
  mImagePixmap = 0;
  mAlphaPixmap = 0;
  mAlphaDepth = 0;
  mAlphaRowBytes = 0;
  mAlphaWidth = 0;
  mAlphaHeight = 0;
  mLocation.x = 0;
  mLocation.y = 0;
  mDisplay = nsnull;
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

    if (mAlphaPixmap != 0) 
    {
      // The display cant be null.  It gets fetched from the drawing 
      // surface used to create the pixmap.  It gets assigned once
      // in Draw()
      NS_ASSERTION(nsnull != mDisplay,"display is null.");

      printf("XFreePixmap(display = %p)\n",mDisplay);

      XFreePixmap(mDisplay, mAlphaPixmap);

    }
  }

  if (mImagePixmap != 0) 
  {
    NS_ASSERTION(nsnull != mDisplay,"display is null.");

    printf("XFreePixmap(display = %p)\n",mDisplay);

    XFreePixmap(mDisplay, mImagePixmap);
  }
}

NS_IMPL_ISUPPORTS(nsImageXlib, kIImageIID);

PRInt32
nsImageXlib::GetHeight()
{
  PR_LOG(ImageXlibLM, PR_LOG_DEBUG, ("nsImageXlib::GetHeight()\n"));
  return mHeight;
}

PRInt32
nsImageXlib::GetWidth()
{
  PR_LOG(ImageXlibLM, PR_LOG_DEBUG, ("nsImageXlib::GetWidth()\n"));
  return mWidth;
}

PRUint8*
nsImageXlib::GetBits()
{
  PR_LOG(ImageXlibLM, PR_LOG_DEBUG, ("nsImageXlib::GetBits()\n"));
  return mImageBits;
}

PRInt32
nsImageXlib::GetLineStride()
{
  PR_LOG(ImageXlibLM, PR_LOG_DEBUG, ("nsImageXlib::GetLineStride()\n"));
  return mRowBytes;
}

NS_IMETHODIMP
nsImageXlib::Draw(nsIRenderingContext &aContext,
                  nsDrawingSurface aSurface,
                  PRInt32 aX, PRInt32 aY,
                  PRInt32 aWidth, PRInt32 aHeight)
{
  PR_LOG(ImageXlibLM, PR_LOG_DEBUG, ("nsImageXlib::Draw()\n"));
  // XXX from gtk code: this is temporary until the
  // height/width args are removed from the draw method
  if ((aWidth != mWidth) || (aHeight != mHeight)) {
    aWidth = mWidth;
    aHeight = mHeight;
  }
  nsDrawingSurfaceXlib *drawing = (nsDrawingSurfaceXlib*)aSurface;

  // Assign the display only once, since this is will be the only
  // time we will have a access to a drawing surface - from which
  // we can fetch the display.
  if (nsnull == mDisplay)
    mDisplay = drawing->GetDisplay();

  XImage *x_image = nsnull;
  GC gc;
  XGCValues gcv;
  
  if ((mAlphaBits != nsnull) && (mAlphaPixmap == 0)) {
    // create a pixmap for this.
    mAlphaPixmap = XCreatePixmap(mDisplay, 
                                 RootWindow(mDisplay, drawing->GetScreenNumber()), 
                                 aWidth, aHeight, 1);
    // create an x image for it.
    x_image = XCreateImage(mDisplay, 
                           drawing->GetVisual(),
                           1, /* visual depth - this is a bitmap */
                           XYPixmap,
                           0,
                           (char *)mAlphaBits,
                           aWidth,
                           aHeight,
                           32,
                           mAlphaRowBytes);
    x_image->bits_per_pixel=1;
    /* Image library always places pixels left-to-right MSB to LSB */
    x_image->bitmap_bit_order = MSBFirst;
    /* This definition doesn't depend on client byte ordering
       because the image library ensures that the bytes in
       bitmask data are arranged left to right on the screen,
       low to high address in memory. */
    x_image->byte_order = MSBFirst;
    memset(&gcv, 0, sizeof(XGCValues));
    gcv.function = GXcopy;
    gc = XCreateGC(mDisplay, mAlphaPixmap, GCFunction, &gcv);
    XPutImage(mDisplay, mAlphaPixmap, gc, x_image,
                   0, 0, 0, 0,
                   aWidth, aHeight);
    XFreeGC(mDisplay, gc);

    // done with the temp image.
    x_image->data = 0;  /* don't free IL_Pixmap's bits */
    XDestroyImage(x_image);
  }
  
  if (nsnull == mImagePixmap) {
    mImagePixmap = XCreatePixmap(mDisplay, 
                                 RootWindow(mDisplay, drawing->GetScreenNumber()),
                                 aWidth, 
                                 aHeight, 
                                 drawing->GetDepth());

    XSetClipOrigin(mDisplay, drawing->GetGC(), 0, 0);
    XSetClipMask(mDisplay, drawing->GetGC(), None);
    xlib_draw_rgb_image (mImagePixmap,
                         drawing->GetGC(),
                         0, 0, aWidth, aHeight,
                         XLIB_RGB_DITHER_MAX,
                         mImageBits, mRowBytes);
  }
  
  if (nsnull != mAlphaPixmap) {
    // set up the gc to use the alpha pixmap for clipping
    XSetClipOrigin(mDisplay, drawing->GetGC(), aX, aY);
    XSetClipMask(mDisplay, drawing->GetGC(), mAlphaPixmap);
  }

  XCopyArea(mDisplay,                  // display
            mImagePixmap,              // source
            drawing->GetDrawable(),    // dest
            drawing->GetGC(),          // GC
            0, 0,                    // xsrc, ysrc
            aWidth, aHeight,           // width, height
            aX, aY);                     // xdest, ydest
            
  if (mAlphaPixmap != nsnull) {
    XSetClipOrigin(mDisplay, drawing->GetGC(), 0, 0);
    XSetClipMask(mDisplay, drawing->GetGC(), None);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsImageXlib::Draw(nsIRenderingContext &aContext,
                  nsDrawingSurface aSurface,
                  PRInt32 aSX, PRInt32 aSY,
                  PRInt32 aSWidth, PRInt32 aSHeight,
                  PRInt32 aDX, PRInt32 aDY,
                  PRInt32 aDWidth, PRInt32 aDHeight)
{
  PR_LOG(ImageXlibLM, PR_LOG_DEBUG, ("nsImageXlib::Draw()\n"));
  nsDrawingSurfaceXlib *drawing = (nsDrawingSurfaceXlib *)aSurface;

  // Assign the display only once, since this is will be the only
  // time we will have a access to a drawing surface - from which
  // we can fetch the display.
  if (nsnull == mDisplay)
    mDisplay = drawing->GetDisplay();

  xlib_draw_rgb_image (drawing->GetDrawable(),
                       drawing->GetGC(),
                       aDX, aDY, aDWidth, aDHeight,
                       XLIB_RGB_DITHER_MAX,
                       mImageBits + mRowBytes * aSY + 3 * aDX,
                       mRowBytes);

  return NS_OK;
}

nsColorMap*
nsImageXlib::GetColorMap()
{
  PR_LOG(ImageXlibLM, PR_LOG_DEBUG, ("nsImageXlib::GetColorMap()\n"));
  return 0;
}

void
nsImageXlib::ImageUpdated(nsIDeviceContext *aContext,
                          PRUint8 aFlags, nsRect *aUpdateRect)
{
  PR_LOG(ImageXlibLM, PR_LOG_DEBUG, ("nsImageXlib::ImageUpdated()\n"));
  if (nsImageUpdateFlags_kBitsChanged & aFlags) {
    if (mAlphaPixmap != 0) {
      XFreePixmap(mDisplay, mAlphaPixmap);
      mAlphaPixmap = 0;
    }
    if (mImagePixmap != 0) {
      XFreePixmap(mDisplay, mImagePixmap);
      mImagePixmap = 0;
    }
  }
}

nsresult
nsImageXlib::Init(PRInt32 aWidth, PRInt32 aHeight,
                  PRInt32 aDepth, nsMaskRequirements aMaskRequirements)
{
  PR_LOG(ImageXlibLM, PR_LOG_DEBUG, ("nsImageXlib::Init()\n"));
  
  if (nsnull != mImageBits) {
    delete[] mImageBits;
    mImageBits = nsnull;
  }
  
  if (nsnull != mAlphaBits) {
    delete[] mAlphaBits;
    mAlphaBits = nsnull;
    if (mAlphaPixmap != 0) {
      XFreePixmap(mDisplay, mAlphaPixmap);
      mAlphaPixmap = 0;
    }
  }
  if (mImagePixmap != 0) {
    XFreePixmap(mDisplay, mImagePixmap);
    mImagePixmap = 0;
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
  
  ComputeMetrics();

  mImageBits = (PRUint8*) new PRUint8[mSizeImage];
  
  switch(aMaskRequirements) {
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
    
    mAlphaBits = new unsigned char[mAlphaRowBytes * aHeight];
    mAlphaWidth = aWidth;
    mAlphaHeight = aHeight;
    break;
    
  case nsMaskRequirements_kNeeds8Bit:
    mAlphaBits = nsnull;
    mAlphaWidth = 0;
    mAlphaHeight = 0;
    break;
  }
  return NS_OK;

}

PRBool
nsImageXlib::IsOptimized()
{
  PR_LOG(ImageXlibLM, PR_LOG_DEBUG, ("nsImageXlib::IsOptimized()\n"));
  return PR_TRUE;
}

nsresult
nsImageXlib::Optimize(nsIDeviceContext* aContext)
{
  PR_LOG(ImageXlibLM, PR_LOG_DEBUG, ("nsImageXlib::Optimize()\n"));
  return 0;
}

PRUint8*
nsImageXlib::GetAlphaBits()
{
  PR_LOG(ImageXlibLM, PR_LOG_DEBUG, ("nsImageXlib::GetAlphaBits()\n"));
  return mAlphaBits;
}

PRInt32
nsImageXlib::GetAlphaWidth()
{
  PR_LOG(ImageXlibLM, PR_LOG_DEBUG, ("nsImageXlib::GetAlphaWidth()\n"));
  return mAlphaWidth;
}

PRInt32
nsImageXlib::GetAlphaHeight()
{
  PR_LOG(ImageXlibLM, PR_LOG_DEBUG, ("nsImageXlib::GetAlphaHeight()\n"));
  return mAlphaHeight;
}

PRInt32
nsImageXlib::GetAlphaLineStride()
{
  PR_LOG(ImageXlibLM, PR_LOG_DEBUG, ("nsImageXlib::GetAlphaLineStride()\n"));
  return mAlphaRowBytes;
}

PRInt32
nsImageXlib::CalcBytesSpan(PRUint32  aWidth)
{
  PR_LOG(ImageXlibLM, PR_LOG_DEBUG, ("nsImageXlib::CalcBytesSpan()\n"));
  PRInt32 spanbytes;

  spanbytes = (aWidth * mDepth) >> 5;

  if (((PRUint32)aWidth * mDepth) & 0x1F)
    spanbytes++;
  spanbytes <<= 2;
  return(spanbytes);
}

void
nsImageXlib::SetAlphaLevel(PRInt32 aAlphaLevel)
{
  PR_LOG(ImageXlibLM, PR_LOG_DEBUG, ("nsImageXlib::SetAlphaLevel()\n"));
  return;
}

PRInt32 nsImageXlib::GetAlphaLevel()
{
  PR_LOG(ImageXlibLM, PR_LOG_DEBUG, ("nsImageXlib::GetAlphaLevel()\n"));
  return 0;
}

void*
nsImageXlib::GetBitInfo()
{
  return 0;
}


void
nsImageXlib::ComputeMetrics()
{
  mRowBytes = CalcBytesSpan(mWidth);
  mSizeImage = mRowBytes * mHeight;
}
