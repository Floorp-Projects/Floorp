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

static NS_DEFINE_IID(kIImageIID, NS_IIMAGE_IID);

nsImageXlib::nsImageXlib()
{
  //  printf("nsImageXlib::nsImageXlib()\n");
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
}

nsImageXlib::~nsImageXlib()
{
  //printf("nsImageXlib::nsImageXlib()\n");
  if (nsnull != mImageBits) {
    delete[] mImageBits;
    mImageBits = nsnull;
  }
  if (nsnull != mAlphaBits) {
    delete[] mAlphaBits;
    mAlphaBits = nsnull;
    if (mAlphaPixmap != 0) {
      XFreePixmap(gDisplay, mAlphaPixmap);
    }
  }
  if (nsnull != mImagePixmap) {
    XFreePixmap(gDisplay, mImagePixmap);
  }
}

NS_IMPL_ISUPPORTS(nsImageXlib, kIImageIID);

PRInt32
nsImageXlib::GetHeight()
{
  //  printf("nsImageXlib::GetHeight()\n");
  return mHeight;
}

PRInt32
nsImageXlib::GetWidth()
{
  //  printf("nsImageXlib::GetWidth()\n");
  return mWidth;
}

PRUint8*
nsImageXlib::GetBits()
{
  //  printf("nsImageXlib::GetBits()\n");
  return mImageBits;
}

PRInt32
nsImageXlib::GetLineStride()
{
  //  printf("nsImageXlib::GetLineStride()\n");
  return mRowBytes;
}

NS_IMETHODIMP
nsImageXlib::Draw(nsIRenderingContext &aContext,
                  nsDrawingSurface aSurface,
                  PRInt32 aX, PRInt32 aY,
                  PRInt32 aWidth, PRInt32 aHeight)
{
  printf("nsImageXlib::Draw()\n");
  // XXX from gtk code: this is temporary until the
  // height/width args are removed from the draw method
  if ((aWidth != mWidth) || (aHeight != mHeight)) {
    aWidth = mWidth;
    aHeight = mHeight;
  }
  nsDrawingSurfaceXlib *drawing = (nsDrawingSurfaceXlib*)aSurface;

  XImage *x_image = nsnull;
  GC gc;
  XGCValues gcv;
  
  if ((mAlphaBits != nsnull) && (mAlphaPixmap == 0)) {
    // create a pixmap for this.
    mAlphaPixmap = XCreatePixmap(gDisplay, RootWindow(gDisplay, gScreenNum), aWidth, aHeight, 1);
    // create an x image for it.
    x_image = XCreateImage(gDisplay, gVisual,
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
    gc = XCreateGC(gDisplay, mAlphaPixmap, GCFunction, &gcv);
    XPutImage(gDisplay, mAlphaPixmap, gc, x_image,
                   0, 0, 0, 0,
                   aWidth, aHeight);
    XFreeGC(gDisplay, gc);

    // done with the temp image.
    x_image->data = 0;  /* don't free IL_Pixmap's bits */
    XDestroyImage(x_image);
  }
  
  if (nsnull == mImagePixmap) {
    mImagePixmap = XCreatePixmap(gDisplay, RootWindow(gDisplay, gScreenNum),
                                 aWidth, aHeight, gDepth);
    XSetClipOrigin(gDisplay, drawing->GetGC(), 0, 0);
    XSetClipMask(gDisplay, drawing->GetGC(), None);
#if 0
    // XXX render the image here...
#endif
  }

  if (nsnull != mAlphaPixmap) {
    // set up the gc to use the alpha pixmap for clipping
    XSetClipOrigin(gDisplay, drawing->GetGC(), 0, 0);
    XSetClipMask(gDisplay, drawing->GetGC(), mAlphaPixmap);
  }

  // XXX do the drawing here.

  if (mAlphaPixmap != nsnull) {
    XSetClipOrigin(gDisplay, drawing->GetGC(), 0, 0);
    XSetClipMask(gDisplay, drawing->GetGC(), None);
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
  printf("nsImageXlib::Draw()\n");
  return NS_OK;
}

nsColorMap*
nsImageXlib::GetColorMap()
{
  printf("nsImageXlib::GetColorMap()\n");
  return 0;
}

void
nsImageXlib::ImageUpdated(nsIDeviceContext *aContext,
                          PRUint8 aFlags, nsRect *aUpdateRect)
{
  printf("nsImageXlib::ImageUpdated()\n");
  if (nsImageUpdateFlags_kBitsChanged & aFlags) {
    if (mAlphaPixmap != 0) {
      XFreePixmap(gDisplay, mAlphaPixmap);
      mAlphaPixmap = 0;
    }
    if (mImagePixmap != 0) {
      XFreePixmap(gDisplay, mImagePixmap);
      mImagePixmap = 0;
    }
  }
}

nsresult
nsImageXlib::Init(PRInt32 aWidth, PRInt32 aHeight,
                  PRInt32 aDepth, nsMaskRequirements aMaskRequirements)
{
  printf("nsImageXlib::Init()\n");
  
  if (nsnull != mImageBits) {
    delete[] mImageBits;
    mImageBits = nsnull;
  }
  
  if (nsnull != mAlphaBits) {
    delete[] mAlphaBits;
    mAlphaBits = nsnull;
    if (nsnull != mAlphaPixmap) {
      XFreePixmap(gDisplay, mAlphaPixmap);
      mAlphaPixmap = 0;
    }
  }
  if (nsnull != mImagePixmap) {
    XFreePixmap(gDisplay, mImagePixmap);
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
  //  printf("nsImageXlib::IsOptimized()\n");
  return PR_TRUE;
}

nsresult
nsImageXlib::Optimize(nsIDeviceContext* aContext)
{
  //  printf("nsImageXlib::Optimize()\n");
  return 0;
}

PRUint8*
nsImageXlib::GetAlphaBits()
{
  //  printf("nsImageXlib::GetAlphaBits()\n");
  return mAlphaBits;
}

PRInt32
nsImageXlib::GetAlphaWidth()
{
  //  printf("nsImageXlib::GetAlphaWidth()\n");
  return mAlphaWidth;
}

PRInt32
nsImageXlib::GetAlphaHeight()
{
  //  printf("nsImageXlib::GetAlphaHeight()\n");
  return mAlphaHeight;
}

PRInt32
nsImageXlib::GetAlphaLineStride()
{
  //  printf("nsImageXlib::GetAlphaLineStride()\n");
  return mAlphaRowBytes;
}

PRInt32
nsImageXlib::CalcBytesSpan(PRUint32  aWidth)
{
  //  printf("nsImageXlib::CalcBytesSpan()\n");
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
  //  printf("nsImageXlib::SetAlphaLevel()\n");
  return;
}

PRInt32 nsImageXlib::GetAlphaLevel()
{
  //  printf("nsImageXlib::GetAlphaLevel()\n");
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
