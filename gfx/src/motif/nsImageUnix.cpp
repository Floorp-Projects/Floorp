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

  ComputePaletteSize(aDepth);

  // create the memory for the image
  ComputMetrics();

printf("******************\nWidth %d  Height %d  Depth %d mSizeImage %d\n", 
                  mWidth, mHeight, mDepth, mSizeImage);
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

  printf("Draw::XPutImage %d %d %d %d %d %d\n",  aSX,aSY,aDX,aDY,aDWidth,aDHeight);
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

  if(nsnull == mImage) {
    printf("NOT OPTIMIZED YET OOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOO\n");
    Optimize(aSurface);
  }
 
  if (nsnull == mImage)
    return PR_FALSE;
printf("Draw::XPutImage2 %d %d %d %d\n", aX,aY,aWidth,aHeight);
  XPutImage(unixdrawing->display,unixdrawing->drawable,unixdrawing->gc,mImage,
                    0,0,aX,aY,aWidth,aHeight);  

printf("Out Draw::XPutImage2 %d %d %d %d\n", aX,aY,aWidth,aHeight);
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

void nsImageUnix::ConvertImage(nsDrawingSurface aDrawingSurface)
{
nsDrawingSurfaceUnix	*unixdrawing =(nsDrawingSurfaceUnix*) aDrawingSurface;
PRUint8			*tempbuffer,*cursrc,*curdest;
PRInt32			oldrowbytes,x,y;
PRInt16			red,green,blue,*cur16;
  
  if((unixdrawing->depth==24) &&  (mDepth==8))
    {
    printf("Converting the image NOWWWWWWWWWWWWWW\n");

    // convert this nsImage to a 24 bit image
    oldrowbytes = mRowBytes;
    mDepth = 24;

    ComputePaletteSize(mDepth);

    // create the memory for the image
    ComputMetrics();

    tempbuffer = (PRUint8*) new PRUint8[mSizeImage];

    for(y=0;y<mHeight;y++)
      {
      cursrc = mImageBits+(y*oldrowbytes);
      curdest =tempbuffer+(y*mRowBytes);
      for(x=0;x<oldrowbytes;x++)
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
   
    // assign the new buffer to this nsImage
    delete[] (PRUint8*)mImageBits;
    mImageBits = tempbuffer; 
   
    // after we are finished converting the image, build a new color map   
    mColorMap = new nsColorMap;

    if (mColorMap != nsnull)
      {
      mColorMap->NumColors = mNumPalleteColors;
      mColorMap->Index = new PRUint8[3 * mNumPalleteColors];
      memset(mColorMap->Index, 0, sizeof(PRUint8) * (3 * mNumPalleteColors));
      }
    }


  if((unixdrawing->depth==16) && (mDepth==8))
    {
    printf("Converting 8 to 16\n");

    // convert this nsImage to a 24 bit image
    oldrowbytes = mRowBytes;
    mDepth = 16;

    ComputePaletteSize(mDepth);

    // create the memory for the image
    ComputMetrics();
    tempbuffer = (PRUint8*) new PRUint8[mSizeImage];

    for(y=0;y<mHeight;y++)
      {
      cursrc = mImageBits+(y*oldrowbytes);
      cur16 = (PRInt16*) (tempbuffer+(y*mRowBytes));

      for(x=0;x<oldrowbytes;x++)
        {
        red = mColorMap->Index[(3*(*cursrc))+2];  // red
        green = mColorMap->Index[(3*(*cursrc))+1];  // green
        blue = mColorMap->Index[(3*(*cursrc))];  // blue
//	blue = 255; red =0 ; green = 0;

        *cur16 = ((red&0xf8)<<8)|((green&0xfc)<<3)| ((blue&0xf8)>>3);	
        cur16++;
        } 
      }
   
    // assign the new buffer to this nsImage
    delete[] (PRUint8*)mImageBits;
    mImageBits = tempbuffer; 

    if (mColorMap != nsnull)
      {
      mColorMap->NumColors = mNumPalleteColors;
      mColorMap->Index = new PRUint8[3 * mNumPalleteColors];
      memset(mColorMap->Index, 0, sizeof(PRUint8) * (3 * mNumPalleteColors));
      }
    }


}

//------------------------------------------------------------

nsresult nsImageUnix::Optimize(nsDrawingSurface aDrawingSurface)
{
PRInt16 i;
printf("Optimize.................................\n");
  if (nsnull == mImage)
    {
#ifdef NOTNOW  
  if(mColorMap->NumColors>0)
      {
      for(i=0;i<mColorMap->NumColors;i++)
        {
        printf("Red = %d\n",mColorMap->Index[(3*i)+2]);
        printf("Green = %d\n",mColorMap->Index[(3*i)+1]);
        printf("Blue = %d\n",mColorMap->Index[(3*i)]);
        }
      }
#endif
    ConvertImage(aDrawingSurface);
    CreateImage(aDrawingSurface);
 //   delete[] (PRUint8*)mImageBits;
 //   mImageBits = nsnull;
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
      printf("%s\n", (unixdrawing->visual->c_class == TrueColor?"True Color":"DirectColor"));
      } 
    else 
      {
      format = XYPixmap;
      printf("Not True Color\n");
      }
printf("Width %d  Height %d Visual Depth %d  Image Depth %d\n", 
                  mWidth, mHeight,  
                  unixdrawing->depth, mDepth);

    mImage = ::XCreateImage(unixdrawing->display,
			    unixdrawing->visual,
			    unixdrawing->depth,
			    format,
			    0,
			    (char *)mImageBits,
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
