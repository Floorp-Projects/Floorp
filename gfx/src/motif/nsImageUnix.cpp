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
  printf("[[[[[[[[[[[[[[[[[[[[ New Image Created ]]]]]]]]]]]]]]]]]]]]]]\n");
  NS_INIT_REFCNT();
  mImage = nsnull ;
  mImageBits = nsnull;
  mConvertedBits = nsnull;
  mBitsForCreate = nsnull;
  mWidth = 0;
  mHeight = 0;
  mDepth = 0;
  mOriginalDepth = 0;
  mColorMap = nsnull;
}

//------------------------------------------------------------

nsImageUnix :: ~nsImageUnix()
{
  if (nsnull != mImage) {
    XDestroyImage(mImage);
    mImage = nsnull;
  }
  if(nsnull != mConvertedBits) 
    {
    delete[] (PRUint8*)mConvertedBits;
    mConvertedBits = nsnull;
    }

  if(nsnull != mImageBits)
    {
    delete[] (PRUint8*)mImageBits;
    mImageBits = nsnull;
    }
  if(nsnull!= mColorMap)
    delete mColorMap;
}

NS_IMPL_ISUPPORTS(nsImageUnix, kIImageIID);

//------------------------------------------------------------

nsresult nsImageUnix :: Init(PRInt32 aWidth, PRInt32 aHeight, PRInt32 aDepth,nsMaskRequirements aMaskRequirements)
{
  if(nsnull != mImageBits)
    delete[] (PRUint8*)mImageBits;

  if(nsnull != mColorMap)
    delete[] mColorMap;

  if (nsnull != mImage) {
    XDestroyImage(mImage);
    mImage = nsnull;
  }
  mWidth = aWidth;
  mHeight = aHeight;
  mDepth = aDepth;
  mOriginalDepth = aDepth;
  mOriginalRowBytes = CalcBytesSpan(aWidth);
  mConverted = PR_FALSE;

  ComputePaletteSize(aDepth);

  // create the memory for the image
  ComputMetrics();

  mImageBits = (PRUint8*) new PRUint8[mSizeImage]; 

  mColorMap = new nsColorMap;

  if (mColorMap != nsnull)
    {
    mColorMap->NumColors = mNumPalleteColors;
    mColorMap->Index = new PRUint8[3 * mNumPalleteColors];
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

//------------------------------------------------------------

// figure out how big our palette needs to be
void nsImageUnix :: ComputePaletteSize(PRIntn nBitCount)
{
  switch (nBitCount)
    {
    case 8:
      mNumPalleteColors = 256;
      mNumBytesPixel = 1;
      break;
    case 16:
      mNumPalleteColors = 0;
      mNumBytesPixel = 2;
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
 
  if (nsnull == mImage)
    return PR_FALSE;

  XPutImage(unixdrawing->display,unixdrawing->drawable,unixdrawing->gc,mImage,
                    aSX,aSY,aDX,aDY,aDWidth,aDHeight);  

  return PR_TRUE;
}

//------------------------------------------------------------

// Draw the bitmap, this draw just has destination coordinates
PRBool nsImageUnix :: Draw(nsIRenderingContext &aContext, 
                       nsDrawingSurface aSurface,
                       PRInt32 aX, PRInt32 aY, 
                       PRInt32 aWidth, PRInt32 aHeight)
{
nsDrawingSurfaceUnix	*unixdrawing =(nsDrawingSurfaceUnix*) aSurface;

    mImage = nsnull;
    Optimize(aSurface);
 
  if (nsnull == mImage)
    return PR_FALSE;
  XPutImage(unixdrawing->display,unixdrawing->drawable,unixdrawing->gc,mImage,
                    0,0,aX,aY,aWidth,aHeight);  
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


void nsImageUnix::AllocConvertedBits(PRUint32 aSize)
{
  if (nsnull == mConvertedBits)
    mConvertedBits = (PRUint8*) new PRUint8[aSize]; 
}

//------------------------------------------------------------

void nsImageUnix::ConvertImage(nsDrawingSurface aDrawingSurface)
{
nsDrawingSurfaceUnix	*unixdrawing =(nsDrawingSurfaceUnix*) aDrawingSurface;
PRUint8			*tempbuffer,*cursrc,*curdest;
PRInt32			x,y;
PRInt16			red,green,blue,*cur16;

  mBitsForCreate = mImageBits;

  if((unixdrawing->depth==24) &&  (mOriginalDepth==8))
    {
    // convert this nsImage to a 24 bit image
    mDepth = 24;
    ComputePaletteSize(mDepth);
    ComputMetrics();
    AllocConvertedBits(mSizeImage);
    tempbuffer = mConvertedBits;
    mBitsForCreate = mConvertedBits;

    for(y=0;y<mHeight;y++)
      {
      cursrc = mImageBits+(y*mOriginalRowBytes);
      curdest =tempbuffer+(y*mRowBytes);
      for(x=0;x<mOriginalRowBytes;x++)
        {
        *curdest = mColorMap->Index[(3*(*cursrc))+2];  // red
        curdest++;
        *curdest = mColorMap->Index[(3*(*cursrc))+1];  // green
        curdest++;
        *curdest = mColorMap->Index[(3*(*cursrc))];  // blue
        curdest++;
        cursrc++;
        } 
      }
   
#if 0 
    if(mColorMap)
      delete mColorMap;

    // after we are finished converting the image, build a new color map   
    mColorMap = new nsColorMap;

    if (mColorMap != nsnull)
      {
      mColorMap->NumColors = mNumPalleteColors;
      mColorMap->Index = new PRUint8[3 * mNumPalleteColors];
      memset(mColorMap->Index, 0, sizeof(PRUint8) * (3 * mNumPalleteColors));
      }
#endif
    }
 
  // convert the 8 bit image to 16 bit
  if((unixdrawing->depth==16) && (mOriginalDepth==8))
    {
    mDepth = 16;
    ComputePaletteSize(mDepth);
    ComputMetrics();
    AllocConvertedBits(mSizeImage);
    tempbuffer = mConvertedBits;
    mBitsForCreate = mConvertedBits;

    for(y=0;y<mHeight;y++)
      {
      cursrc = mImageBits+(y*mOriginalRowBytes);
      cur16 = (PRInt16*) (tempbuffer+(y*mRowBytes));

      for(x=0;x<mOriginalRowBytes;x++)
        {
        red = mColorMap->Index[(3*(*cursrc))+2];  // red
        green = mColorMap->Index[(3*(*cursrc))+1];  // green
        blue = mColorMap->Index[(3*(*cursrc))];  // blue
        *cur16 = ((red&0xf8)<<8)|((green&0xfc)<<3)| ((blue&0xf8)>>3);	
        cur16++;
        } 
      }
   
#if 0
    if (mColorMap != nsnull)
      {
      mColorMap->NumColors = mNumPalleteColors;
      mColorMap->Index = new PRUint8[3 * mNumPalleteColors];
      memset(mColorMap->Index, 0, sizeof(PRUint8) * (3 * mNumPalleteColors));
      }
#endif
    }

}

//------------------------------------------------------------

nsresult nsImageUnix::Optimize(nsDrawingSurface aDrawingSurface)
{
PRInt16 i;

  if (nsnull == mImage)
    {
    ConvertImage(aDrawingSurface);
    CreateImage(aDrawingSurface);
    }
}

//------------------------------------------------------------

void nsImageUnix::CreateImage(nsDrawingSurface aSurface)
{
  PRUint32 wdepth;
  Visual * visual ;
  PRUint32 format ;
  nsDrawingSurfaceUnix	*unixdrawing =(nsDrawingSurfaceUnix*) aSurface;
  
  if(mImageBits) {
    /* Need to support monochrome too */
    if (unixdrawing->visual->c_class == TrueColor || 
        unixdrawing->visual->c_class == DirectColor) 
      {
      format = ZPixmap;
      } 
    else 
      {
      format = XYPixmap;
      }
/*printf("Width %d  Height %d Visual Depth %d  Image Depth %d\n", 
                  mWidth, mHeight,  
                  unixdrawing->depth, mDepth);*/

    mImage = ::XCreateImage(unixdrawing->display,
			    unixdrawing->visual,
			    unixdrawing->depth,
			    format,
			    0,
			    (char *)mBitsForCreate,
			    (unsigned int)mWidth, 
			    (unsigned int)mHeight,
			    32,mRowBytes);

    mImage->byte_order       = ImageByteOrder(unixdrawing->display);
    mImage->bits_per_pixel   = unixdrawing->depth;
    mImage->bitmap_bit_order = BitmapBitOrder(unixdrawing->display);
    mImage->bitmap_unit      = 32;

  }	
  return ;
}
