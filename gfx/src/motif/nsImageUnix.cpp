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
#include "nsImageUnix.h"
#include "nsRenderingContextUnix.h"
#include "nsDeviceContextUnix.h"

#include "nspr.h"

#define IsFlagSet(a,b) (a & b)

static NS_DEFINE_IID(kIImageIID, NS_IIMAGE_IID);

//------------------------------------------------------------

nsImageUnix :: nsImageUnix()
{
  NS_INIT_REFCNT();
  printf("==========================\nnsImageUnix :: nsImageUnix()\n");
  mImage = nsnull ;
  mImageBits = nsnull;
  mWidth = 0;
  mHeight = 0;
  mDepth = 0;
  mColorMap = nsnull;
}

//------------------------------------------------------------

nsImageUnix :: ~nsImageUnix()
{
  if (nsnull != mImage) {
    XDestroyImage(mImage);
    mImage = nsnull;
  }
  if(nsnull != mImageBits)
    {
    //delete[] (PRUint8*)mImageBits;
    mImageBits = nsnull;
    }
}

NS_IMPL_ISUPPORTS(nsImageUnix, kIImageIID);

//------------------------------------------------------------

nsresult nsImageUnix :: Init(PRInt32 aWidth, PRInt32 aHeight, PRInt32 aDepth,nsMaskRequirements aMaskRequirements)
{
  mWidth = aWidth;
  mHeight = aHeight;
  mDepth = aDepth;

  ComputePaletteSize(aDepth);

  // create the memory for the image
  ComputMetrics();

printf("******************\nWidth %d  Height %d  Depth %d mSizeImage %d\n", 
                  mWidth, mHeight, mDepth, mSizeImage);
  mImageBits = (PRUint8*) new PRUint8[mSizeImage];
  //char * buf =  (char*) malloc(mSizeImage+1);
  //printf("Buf address %x\n", buf);
  //mImageBits = buf;
  if (mImageBits == nsnull) {
    printf("Bits are null!\n");
  }

    mColorMap = new nsColorMap;

    if (mColorMap != nsnull)
    {
      mColorMap->NumColors = mNumPalleteColors;
      mColorMap->Index = new PRUint8[3 * mNumPalleteColors];

      // XXX Note: I added this because purify claims that we make a
      // copy of the memory (which we do!). I'm not sure if this
      // matters or not, but this shutup purify.
      memset(mColorMap->Index, 0, sizeof(PRUint8) * (3 * mNumPalleteColors));
    }

  return NS_OK;
}

//------------------------------------------------------------

void nsImageUnix::ComputMetrics()
{

  mRowBytes = CalcBytesSpan(mWidth);
  mSizeImage = mRowBytes * mHeight;

}
// figure out how big our palette needs to be
void nsImageUnix :: ComputePaletteSize(PRIntn nBitCount)
{
        switch (nBitCount)
  {
                case 8:
                        mNumPalleteColors = 256;
      mNumBytesPixel = 1;
      break;

                case 24:
                        mNumPalleteColors = 0;
      mNumBytesPixel = 3;
      break;

                default:
                        mNumPalleteColors = -1;
      mNumBytesPixel = 0;
      break;
  }
}

//------------------------------------------------------------

PRInt32  nsImageUnix :: CalcBytesSpan(PRUint32  aWidth)
{
PRInt32 spanbytes;

  spanbytes = (aWidth * mDepth) >> 5;

	if (((PRUint32)aWidth * mDepth) & 0x1F) 
		spanbytes++;

	spanbytes <<= 2;

  return(spanbytes);
}

//------------------------------------------------------------

// set up the pallete to the passed in color array, RGB only in this array
void nsImageUnix :: ImageUpdated(nsIDeviceContext *aContext, PRUint8 aFlags, nsRect *aUpdateRect)
{

  if (nsnull == mImage)
    return;


  if (IsFlagSet(nsImageUpdateFlags_kBitsChanged, aFlags)){

  }

}


//------------------------------------------------------------

// Draw the bitmap, this method has a source and destination coordinates
PRBool nsImageUnix :: Draw(nsIRenderingContext &aContext, nsDrawingSurface aSurface, PRInt32 aSX, PRInt32 aSY, PRInt32 aSWidth, PRInt32 aSHeight,
                          PRInt32 aDX, PRInt32 aDY, PRInt32 aDWidth, PRInt32 aDHeight)
{
nsDrawingSurfaceUnix	*unixdrawing =(nsDrawingSurfaceUnix*) aSurface;
 
  printf("Draw::XPutImage %d %d %d %d %d %d\n",  aSX,aSY,aDX,aDY,aDWidth,aDHeight);
  if (nsnull == mImage)
    return PR_FALSE;

  printf("Draw::XPutImage %d %d %d %d %d %d\n",  aSX,aSY,aDX,aDY,aDWidth,aDHeight);
  XPutImage(unixdrawing->display,unixdrawing->drawable,unixdrawing->gc,mImage,
                    aSX,aSY,aDX,aDY,aDWidth,aDHeight);  

  return PR_TRUE;
}

//------------------------------------------------------------

// Draw the bitmap, this draw just has destination coordinates
PRBool nsImageUnix :: Draw(nsIRenderingContext &aContext, nsDrawingSurface aSurface,
                          PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
nsDrawingSurfaceUnix	*unixdrawing =(nsDrawingSurfaceUnix*) aSurface;
 
printf("Draw::XPutImage2 %d %d %d %d %d %d\n",  aX,aY,aX,aY,aWidth,aHeight);
  if (nsnull == mImage)
    return PR_FALSE;
printf("Draw::XPutImage2 %d %d %d %d %d %d\n",  aX,aY,aX,aY,aWidth,aHeight);
  XPutImage(unixdrawing->display,unixdrawing->drawable,unixdrawing->gc,mImage,
                    aX,aY,aX,aY,aWidth,aHeight);  

  return PR_TRUE;
}

//------------------------------------------------------------

void nsImageUnix::CompositeImage(nsIImage *aTheImage, nsPoint *aULLocation,nsBlendQuality aBlendQuality)
{

}

//------------------------------------------------------------

// lets build an alpha mask from this image
PRBool nsImageUnix::SetAlphaMask(nsIImage *aTheMask)
{
  return(PR_FALSE);
}

//------------------------------------------------------------

nsresult nsImageUnix::Optimize(nsDrawingSurface aDrawingSurface)
{
  if (nsnull == mImage) {
    CreateImage(aDrawingSurface);
  }
}

//------------------------------------------------------------

void nsImageUnix::CreateImage(nsDrawingSurface aSurface)
{
 XWindowAttributes wa;
 PRUint32 wdepth;
 Visual * visual ;
 PRUint32 format ;
 nsDrawingSurfaceUnix	*unixdrawing =(nsDrawingSurfaceUnix*) aSurface;
 //char * data = (char *) PR_Malloc(sizeof(char) * mWidth * mHeight * mDepth);

  if(mImageBits)
    {
    ::XGetWindowAttributes(unixdrawing->display,
			 unixdrawing->drawable,
			 &wa);
  
    /* XXX What if mDepth != wDepth */
    wdepth = wa.depth;
    visual = wa.visual;

    /* Need to support monochrome too */
    if (visual->c_class == TrueColor || visual->c_class == DirectColor)
      format = ZPixmap;
    else
      format = XYPixmap;

    mImage = ::XCreateImage(unixdrawing->display,
			  visual,
			  wdepth,
			  format,
			  0,
			  mImageBits,
			  mWidth, 
			  mHeight,
			  8,0);
    }	
  return ;
}
