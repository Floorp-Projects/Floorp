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


#include "nsImageWin.h"
#include "nsRenderingContextWin.h"

static NS_DEFINE_IID(kIImageIID, NS_IIMAGE_IID);

//------------------------------------------------------------

nsImageWin :: nsImageWin()
{
  NS_INIT_REFCNT();

  mImageBits = nsnull;
	mHBitmap = nsnull;
	mHPalette = nsnull;
  mAlphaBits = nsnull;
  mColorMap = nsnull;
  mBHead = nsnull;

	CleanUp(PR_TRUE);
}

//------------------------------------------------------------

nsImageWin :: ~nsImageWin()
{
	CleanUp(PR_TRUE);
}

NS_IMPL_ISUPPORTS(nsImageWin, kIImageIID);

//------------------------------------------------------------

nsresult nsImageWin :: Init(PRInt32 aWidth, PRInt32 aHeight, PRInt32 aDepth,nsMaskRequirements aMaskRequirements)
{
	mHBitmap = nsnull;
	mHPalette = nsnull;
	CleanUp(PR_TRUE);
	ComputePaletteSize(aDepth);

  if (mNumPalleteColors >= 0)
  {
	  mBHead = (LPBITMAPINFOHEADER) new char[sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD) * mNumPalleteColors];
    mBHead->biSize = sizeof(BITMAPINFOHEADER);
	  mBHead->biWidth = aWidth;
	  mBHead->biHeight = aHeight;
	  mBHead->biPlanes = 1;
	  mBHead->biBitCount = (unsigned short) aDepth;
	  mBHead->biCompression = BI_RGB;
	  mBHead->biSizeImage = 0;            // not compressed, so we dont need this to be set
	  mBHead->biXPelsPerMeter = 0;
	  mBHead->biYPelsPerMeter = 0;
	  mBHead->biClrUsed = mNumPalleteColors;
	  mBHead->biClrImportant = mNumPalleteColors;

	  ComputeMetrics();
	  memset(mColorTable, 0, sizeof(RGBQUAD) * mNumPalleteColors);
    mImageBits = new unsigned char[mSizeImage];
    memset(mImageBits, 128, mSizeImage);
    this->MakePalette();

    if (aMaskRequirements != nsMaskRequirements_kNoMask)
      {
      mAlphaWidth=aWidth;
      mAlphaWidth=aHeight;
      mAlphaBits = new unsigned char[aWidth * aHeight];
      }
    else
      {
      mAlphaBits = 0;
      mAlphaWidth=0;
      mAlphaHeight=0;
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
  }

  return NS_OK;
}

//------------------------------------------------------------

// set up the pallete to the passed in color array, RGB only in this array
void nsImageWin :: ImageUpdated(nsIDeviceContext *aContext, PRUint8 aFlags, nsRect *aUpdateRect)
{
  PRInt32 i;
  PRUint8 *cpointer;

  if (aFlags & nsImageUpdateFlags_kColorMapChanged)
  {
    PRUint8 *gamma = aContext->GetGammaTable();

    if (mColorMap->NumColors > 0)
    {
      cpointer = mColorTable;

      for(i = 0; i < mColorMap->NumColors; i++)
      {
		    *cpointer++ = gamma[mColorMap->Index[(3 * i) + 2]];
		    *cpointer++ = gamma[mColorMap->Index[(3 * i) + 1]];
		    *cpointer++ = gamma[mColorMap->Index[(3 * i)]];
		    *cpointer++ = 0;
      }
    }

    this->MakePalette();
  }
  else if ((aFlags & nsImageUpdateFlags_kBitsChanged) &&
           (nsnull != aUpdateRect))
  {
    if (mNumPalleteColors == 0)
    {
      PRInt32 x, y, span = CalcBytesSpan(mBHead->biWidth), idx;
      PRUint8 *pixels = mImageBits + aUpdateRect->y * span + aUpdateRect->x * 3;
      PRUint8 *gamma = aContext->GetGammaTable();
      
      for (y = 0; y < aUpdateRect->height; y++)
      {
        for (x = 0, idx = 0; x < aUpdateRect->width; x++)
        {
          pixels[idx] = gamma[pixels[idx]];
          idx++;
          pixels[idx] = gamma[pixels[idx]];
          idx++;
          pixels[idx] = gamma[pixels[idx]];
          idx++;
        }

        pixels += span;
      }
    }
  }
}

//------------------------------------------------------------

PRUintn nsImageWin :: UsePalette(HDC* aHdc, PRBool bBackground)
{
	if (mHPalette == nsnull) 
    return 0;

  HPALETTE hOldPalette = ::SelectPalette(aHdc, mHPalette, (bBackground == PR_TRUE) ? TRUE : FALSE);

	return ::RealizePalette(aHdc);
}

//------------------------------------------------------------

// Draw the bitmap, this method has a source and destination coordinates
PRBool nsImageWin :: Draw(nsIRenderingContext &aContext, nsDrawingSurface aSurface, PRInt32 aSX, PRInt32 aSY, PRInt32 aSWidth, PRInt32 aSHeight,
                          PRInt32 aDX, PRInt32 aDY, PRInt32 aDWidth, PRInt32 aDHeight)
{
  PRUint32  value,error;
  HDC       the_hdc = (HDC)aSurface;

  if (mBHead == nsnull) 
    return PR_FALSE;

  if (!IsOptimized())
  {
    value = ::StretchDIBits(the_hdc,aDX,aDY,aDWidth,aDHeight,
                            0,0,aSWidth, aSHeight,
                            mImageBits,(LPBITMAPINFO)mBHead,DIB_RGB_COLORS,SRCCOPY);
    if (value == GDI_ERROR)
      error = ::GetLastError();
  }
  else
  {
    nsIDeviceContext  *dx = aContext.GetDeviceContext();
    HDC srcdc = dx->GetDrawingSurface(aContext);

    if (NULL != srcdc)
    {
      HBITMAP oldbits = ::SelectObject(srcdc, mHBitmap);
 
      if (!::StretchBlt(the_hdc, aDX, aDY, aDWidth, aDHeight, srcdc, aSX, aSY,
                        aSWidth, aSHeight, SRCCOPY))
        error = ::GetLastError();

      if (nsnull != oldbits)
        ::SelectObject(srcdc, oldbits);
    }

    NS_RELEASE(dx);
  }

  return PR_TRUE;
}

//------------------------------------------------------------

// Draw the bitmap, this draw just has destination coordinates
PRBool nsImageWin :: Draw(nsIRenderingContext &aContext, nsDrawingSurface aSurface,
                          PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  PRUint32  value,error;
  HDC   the_hdc = (HDC)aSurface;

  if (mBHead == nsnull) 
    return PR_FALSE;

  if (!IsOptimized())
  {
    value = ::StretchDIBits(the_hdc,aX,aY,aWidth,aHeight,
                            0,0,mBHead->biWidth, mBHead->biHeight,
                            mImageBits,(LPBITMAPINFO)mBHead,DIB_RGB_COLORS,SRCCOPY);
    
    if (value == GDI_ERROR)
      error = ::GetLastError();
  }
  else
  {
    nsIDeviceContext  *dx = aContext.GetDeviceContext();
    HDC srcdc = dx->GetDrawingSurface(aContext);

    if (NULL != srcdc)
    {
      HBITMAP oldbits = ::SelectObject(srcdc, mHBitmap);
      //if((aWidth == mBHead->biWidth) && (aHeight == mBHead->biHeight))
      //BitBlt(the_hdc,aX,aY,aWidth,aHeight,mOptimizeDC,0,0,SRCCOPY);
    
      if (!::StretchBlt(the_hdc, aX, aY, aWidth, aHeight, srcdc, 0, 0,
                        mBHead->biWidth, mBHead->biHeight, SRCCOPY))
        error = ::GetLastError();

      if (nsnull != oldbits)
        ::SelectObject(srcdc, oldbits);
    }

    NS_RELEASE(dx);
  }

  return PR_TRUE;
}

//------------------------------------------------------------

void nsImageWin::CompositeImage(nsIImage *aTheImage, nsPoint *aULLocation,nsBlendQuality aBlendQuality)
{
PRInt32   dlinespan,slinespan,mlinespan,numbytes,numlines,level;
PRUint8   *alphabits,*s1,*d1,*m1;

  // optimized bitmaps cannot be composited, "YET"
  if( IsOptimized() )
    return;

  if(CalcAlphaMetrics(aTheImage,aULLocation,&numlines,&numbytes,&s1,&d1,&m1,&slinespan,&dlinespan,&mlinespan))
    {
    alphabits = aTheImage->GetAlphaBits();

    if ((mNumBytesPixel == 3) && (((nsImageWin *)aTheImage)->mNumBytesPixel == 3))
      {
      if(alphabits)
        {
        numbytes/=3;    // since the mask is only 8 bits, this routine wants number of pixels
        Do24BlendWithMask(numlines,numbytes,s1,d1,m1,slinespan,dlinespan,mlinespan,aBlendQuality);
        }
      else
        {
        level = aTheImage->GetAlphaLevel();
        Do24Blend(level,numlines,numbytes,s1,d1,slinespan,dlinespan,aBlendQuality);
        }
      }
    else
      if ((mNumBytesPixel == 1) && (((nsImageWin *)aTheImage)->mNumBytesPixel == 1))
        {
        if(alphabits)
          {
          Do8BlendWithMask(numlines,numbytes,s1,d1,m1,slinespan,dlinespan,mlinespan,aBlendQuality);
          }
        else
          {
          level = aTheImage->GetAlphaLevel();
          Do8Blend(level,numlines,numbytes,s1,d1,slinespan,dlinespan,aBlendQuality);
          }
        }
    }
}

//------------------------------------------------------------

// lets build an alpha mask from this image
PRBool nsImageWin::SetAlphaMask(nsIImage *aTheMask)
{
PRInt32   num;
PRUint8   *srcbits;

  if (aTheMask && (((nsImageWin*)aTheMask)->mNumBytesPixel == 1))
  {
    mLocation.x = 0;
    mLocation.y = 0;
    mAlphaDepth = 8;
    mAlphaWidth = aTheMask->GetWidth();
    mAlphaHeight = aTheMask->GetWidth();
    num = mAlphaWidth*mAlphaHeight;
    mARowBytes = aTheMask->GetLineStride();
    mAlphaBits = new unsigned char[mARowBytes * mAlphaHeight];

    srcbits = aTheMask->GetBits();
    memcpy(mAlphaBits,srcbits,num); 

    return(PR_TRUE);
  }

  return(PR_FALSE);
}

//------------------------------------------------------------

PRBool nsImageWin::CalcAlphaMetrics(nsIImage *aTheImage,nsPoint *aULLocation,PRInt32 *aNumlines,
                              PRInt32 *aNumbytes,PRUint8 **aSImage,PRUint8 **aDImage,
                              PRUint8 **aMImage,PRInt32 *aSLSpan,PRInt32 *aDLSpan,PRInt32 *aMLSpan)
{
PRBool    doalpha = PR_FALSE;
nsRect    arect,srect,drect,irect;
PRInt32   startx,starty,x,y;
PRUint8   *alphabits;

  if( IsOptimized() )
    return doalpha;

  alphabits = aTheImage->GetAlphaBits();
  if(alphabits)
    {
    arect.SetRect(0,0,this->GetWidth(),this->GetHeight());
    srect.SetRect(aTheImage->GetAlphaXLoc(),aTheImage->GetAlphaYLoc(),aTheImage->GetAlphaWidth(),aTheImage->GetAlphaHeight());
    arect.IntersectRect(arect,srect);
    }
  else
    {
    arect.SetRect(0, 0, this->GetWidth(), this->GetHeight());
    x = y = 0;
    }

  srect.SetRect(aULLocation->x, aULLocation->y, aTheImage->GetWidth(), aTheImage->GetHeight());
  drect = arect;

  if (irect.IntersectRect(srect, drect))
    {
    // calculate destination information
    *aDLSpan = this->GetLineStride();
    *aNumbytes = this->CalcBytesSpan(irect.width);
    *aNumlines = irect.height;
    startx = irect.x;
    starty = this->GetHeight() - (irect.y + irect.height);
    *aDImage = mImageBits + (starty * (*aDLSpan)) + (mNumBytesPixel * startx);

    // get the intersection relative to the source rectangle
    srect.SetRect(0, 0, aTheImage->GetWidth(), aTheImage->GetHeight());
    drect = irect;
    drect.MoveBy(-aULLocation->x, -aULLocation->y);

    drect.IntersectRect(drect,srect);
    *aSLSpan = aTheImage->GetLineStride();
    startx = drect.x;
    starty = aTheImage->GetHeight() - (drect.y + drect.height);
    *aSImage = aTheImage->GetBits() + (starty * (*aSLSpan)) + (mNumBytesPixel * startx);
         
    doalpha = PR_TRUE;

    if(alphabits)
      {
      *aMLSpan = aTheImage->GetAlphaLineStride();
      *aMImage = alphabits;
      }
    else
      {
      aMLSpan = 0;
      *aMImage = nsnull;
      }
  }

  return doalpha;
}

//------------------------------------------------------------

// This routine can not be fast enough
void
nsImageWin::Do24BlendWithMask(PRInt32 aNumlines,PRInt32 aNumbytes,PRUint8 *aSImage,PRUint8 *aDImage,PRUint8 *aMImage,PRInt32 aSLSpan,PRInt32 aDLSpan,PRInt32 aMLSpan,nsBlendQuality aBlendQuality)
{
PRUint8   *d1,*d2,*s1,*s2,*m1,*m2;
PRInt32   x,y;
PRUint32  val1,val2;
PRUint32  temp1,numlines,xinc,yinc;
PRInt32   sspan,dspan,mspan;

  sspan = aSLSpan;
  dspan = aDLSpan;
  mspan = aMLSpan;

  // now go thru the image and blend (remember, its bottom upwards)
  s1 = aSImage;
  d1 = aDImage;
  m1 = aMImage;

  numlines = aNumlines;  
  xinc = 1;
  yinc = 1;

  for (y = 0; y < aNumlines; y++)
    {
    s2 = s1;
    d2 = d1;
    m2 = m1;

    for(x=0;x<aNumbytes;x++)
      {
      val1 = (*m2);
      val2 = 255-val1;

      temp1 = (((*d2)*val1)+((*s2)*val2))>>8;
      if(temp1>255)
        temp1 = 255;
      *d2 = (unsigned char)temp1;
      d2++;
      s2++;
  
      temp1 = (((*d2)*val1)+((*s2)*val2))>>8;
      if(temp1>255)
        temp1 = 255;
      *d2 = (unsigned char)temp1;
      d2++;
      s2++;

      temp1 = (((*d2)*val1)+((*s2)*val2))>>8;
      if(temp1>255)
        temp1 = 255;
      *d2 = (unsigned char)temp1;
      d2++;
      s2++;
      m2++;
      }
    s1 += sspan;
    d1 += dspan;
    m1 += mspan;
    }
}

//------------------------------------------------------------

// This routine can not be fast enough
void
nsImageWin::Do24Blend(PRUint8 aBlendVal,PRInt32 aNumlines,PRInt32 aNumbytes,PRUint8 *aSImage,PRUint8 *aDImage,PRInt32 aSLSpan,PRInt32 aDLSpan,nsBlendQuality aBlendQuality)
{
PRUint8   *d1,*d2,*s1,*s2;
PRUint32  val1,val2;
PRInt32   x,y,temp1,numlines,xinc,yinc;


  aBlendVal = (aBlendVal*255)/100;
  val2 = aBlendVal;
  val1 = 255-val2;

  // now go thru the image and blend (remember, its bottom upwards)
  s1 = aSImage;
  d1 = aDImage;


  numlines = aNumlines;  
  xinc = 1;
  yinc = 1;

  for(y = 0; y < aNumlines; y++)
    {
    s2 = s1;
    d2 = d1;

    for(x = 0; x < aNumbytes; x++)
      {
      temp1 = (((*d2)*val1)+((*s2)*val2))>>8;
      if(temp1>255)
        temp1 = 255;
      *d2 = (unsigned char)temp1; 

      d2++;
      s2++;
      }

    s1 += aSLSpan;
    d1 += aDLSpan;
    }
}

//------------------------------------------------------------

// This routine can not be fast enough
void
nsImageWin::Do8BlendWithMask(PRInt32 aNumlines,PRInt32 aNumbytes,PRUint8 *aSImage,PRUint8 *aDImage,PRUint8 *aMImage,PRInt32 aSLSpan,PRInt32 aDLSpan,PRInt32 aMLSpan,nsBlendQuality aBlendQuality)
{
PRUint8   *d1,*d2,*s1,*s2,*m1,*m2;
PRInt32   x,y;
PRUint32  val1,val2,temp1,numlines,xinc,yinc;
PRInt32   sspan,dspan,mspan;

  sspan = aSLSpan;
  dspan = aDLSpan;
  mspan = aMLSpan;

  // now go thru the image and blend (remember, its bottom upwards)
  s1 = aSImage;
  d1 = aDImage;
  m1 = aMImage;

  numlines = aNumlines;  
  xinc = 1;
  yinc = 1;

  for (y = 0; y < aNumlines; y++)
    {
    s2 = s1;
    d2 = d1;
    m2 = m1;

    for(x=0;x<aNumbytes;x++)
      {
      val1 = (*m2);
      val2 = 255-val1;

      temp1 = (((*d2)*val1)+((*s2)*val2))>>8;
      if(temp1>255)
        temp1 = 255;
      *d2 = (unsigned char)temp1;
      d2++;
      s2++;
      m2++;
      }
    s1 += sspan;
    d1 += dspan;
    m1 += mspan;
    }
}

//------------------------------------------------------------

extern void inv_cmap(PRInt16 colors,PRUint8 *aCMap,PRInt16 bits,PRUint32 *dist_buf,PRUint8 *aRGBMap );

// This routine can not be fast enough
void
nsImageWin::Do8Blend(PRUint8 aBlendVal,PRInt32 aNumlines,PRInt32 aNumbytes,PRUint8 *aSImage,PRUint8 *aDImage,PRInt32 aSLSpan,PRInt32 aDLSpan,nsBlendQuality aBlendQuality)
{
PRUint32   r,g,b,r1,g1,b1,i;
PRUint8   *d1,*d2,*s1,*s2;
PRInt32   x,y,val1,val2,numlines,xinc,yinc;;
PRUint8   *mapptr,*invermap;
PRUint32  *distbuffer;
PRUint32  quantlevel,tnum,num,shiftnum;

  aBlendVal = (aBlendVal*255)/100;
  val2 = aBlendVal;
  val1 = 255-val2;

  // calculate the inverse map
  mapptr = mColorMap->Index;
  quantlevel = aBlendQuality+2;
  shiftnum = (8-quantlevel)+8;
  tnum = 2;
  for(i=1;i<quantlevel;i++)
    tnum = 2*tnum;

  num = tnum;
  for(i=1;i<3;i++)
    num = num*tnum;

  distbuffer = new PRUint32[num];
  invermap  = new PRUint8[num*3];
  inv_cmap(256,mapptr,quantlevel,distbuffer,invermap );

  // now go thru the image and blend (remember, its bottom upwards)
  s1 = aSImage;
  d1 = aDImage;


  numlines = aNumlines;  
  xinc = 1;
  yinc = 1;

  for(y = 0; y < aNumlines; y++)
    {
    s2 = s1;
    d2 = d1;

    for(x = 0; x < aNumbytes; x++)
      {
      i = (*d2);
      r = mColorMap->Index[(3 * i) + 2];
      g = mColorMap->Index[(3 * i) + 1];
      b = mColorMap->Index[(3 * i)];

      i =(*s2);
      r1 = mColorMap->Index[(3 * i) + 2];
      g1 = mColorMap->Index[(3 * i) + 1];
      b1 = mColorMap->Index[(3 * i)];

      r = ((r*val1)+(r1*val2))>>shiftnum;
      if(r>tnum)
        r = tnum;

      g = ((g*val1)+(g1*val2))>>shiftnum;
      if(g>tnum)
        g = tnum;

      b = ((b*val1)+(b1*val2))>>shiftnum;
      if(b>tnum)
        b = tnum;

      r = (r<<(2*quantlevel))+(g<<quantlevel)+b;
      (*d2) = invermap[r];
      d2++;
      s2++;
      }

    s1 += aSLSpan;
    d1 += aDLSpan;
    }

   
  /*{
  PRUint8 *mapptr = mColorMap->Index;
  int     i;

    mapptr = mColorMap->Index;        
    for (i=0; i < mColorMap->NumColors; i++) 
      {
      *mapptr++ = i;
      *mapptr++ = i;
      *mapptr++ = i;
      }
  }*/

  delete[] distbuffer;
  delete[] invermap;

}

//------------------------------------------------------------

nsIImage*      
nsImageWin::DuplicateImage()
{
PRInt32     num,i;
nsImageWin  *theimage;

  if( IsOptimized() )
    return nsnull;

  theimage = new nsImageWin();

  NS_ADDREF(theimage);

  // get the header and copy it
  theimage->mBHead = (LPBITMAPINFOHEADER)new char[sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD) * this->mNumPalleteColors];
  num = sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD) * this->mNumPalleteColors;
  memcpy(theimage->mBHead,this->mBHead,num);

  // set in compute metrics
  theimage->mSizeImage = this->mSizeImage;
  theimage->mRowBytes = this->mRowBytes;
  theimage->mColorTable = (PRUint8 *)theimage->mBHead + sizeof(BITMAPINFOHEADER);
  theimage->mNumBytesPixel = this->mNumBytesPixel;
  theimage->mNumPalleteColors = this->mNumPalleteColors;
  theimage->mAlphaDepth = this->mAlphaDepth;
  theimage->mARowBytes = this->mARowBytes;
  theimage->mAlphaWidth = this->mAlphaWidth;
  theimage->mAlphaHeight = this->mAlphaHeight;
  theimage->mLocation = this->mLocation;
  theimage->mIsOptimized = this->mIsOptimized;

  // set up the memory
  num = sizeof(RGBQUAD) * mNumPalleteColors;
  if(num>0)
    memcpy(mColorTable,this->mColorTable,sizeof(RGBQUAD) * mNumPalleteColors);

    // the bits of the image
  if(theimage->mSizeImage>0)
    {
    theimage->mImageBits = new unsigned char[theimage->mSizeImage];
    memcpy(theimage->mImageBits,this->mImageBits,theimage->mSizeImage);
    }
  else
    theimage->mImageBits = nsnull;


  // bits of the alpha mask
  num = theimage->mAlphaWidth*theimage->mAlphaHeight;
  if(num>0)
    {
    theimage->mAlphaBits = new unsigned char[num];
    memcpy(theimage->mImageBits,this->mImageBits,theimage->mSizeImage);
    }
  else
    theimage->mAlphaBits = nsnull;

  theimage->mColorMap = new nsColorMap;
  if (theimage->mColorMap != nsnull) 
	  {
    theimage->mColorMap->NumColors = theimage->mNumPalleteColors;
    theimage->mColorMap->Index = new PRUint8[3 * theimage->mNumPalleteColors];
    memset(theimage->mColorMap->Index, 0, sizeof(PRUint8) * (3 * theimage->mNumPalleteColors));
		}

  for(i = 0; i < theimage->mColorMap->NumColors; i++)
    {
		theimage->mColorMap->Index[(3 * i) + 2] = this->mColorMap->Index[(3 * i) + 2];
		theimage->mColorMap->Index[(3 * i) + 1] = this->mColorMap->Index[(3 * i) + 1];
		theimage->mColorMap->Index[(3 * i)] = this->mColorMap->Index[(3 * i)];
    }

  theimage->MakePalette();

  theimage->mHBitmap = nsnull;    

  return (theimage);
}

//------------------------------------------------------------

PRBool nsImageWin::MakePalette()
{
	// makes a logical palette (mHPalette) from the DIB's color table
	// this palette will be selected and realized prior to drawing the DIB

	if (mNumPalleteColors == 0) 
    return PR_FALSE;

	if (mHPalette != nsnull) 
    ::DeleteObject(mHPalette);

  LPLOGPALETTE pLogPal = (LPLOGPALETTE) new char[2 * sizeof(WORD) + mNumPalleteColors * sizeof(PALETTEENTRY)];
	pLogPal->palVersion = 0x300;
	pLogPal->palNumEntries = mNumPalleteColors;
	LPRGBQUAD pDibQuad = (LPRGBQUAD) mColorTable;

	for (int i = 0; i < mNumPalleteColors; i++) 
  {
		pLogPal->palPalEntry[i].peRed = pDibQuad->rgbRed;
		pLogPal->palPalEntry[i].peGreen = pDibQuad->rgbGreen;
		pLogPal->palPalEntry[i].peBlue = pDibQuad->rgbBlue;
		pLogPal->palPalEntry[i].peFlags = 0;
		pDibQuad++;
  }

	mHPalette = ::CreatePalette(pLogPal);
	delete pLogPal;

	return PR_TRUE;
}	

//------------------------------------------------------------

PRBool nsImageWin :: SetSystemPalette(HDC* aHdc)
{
  PRInt32 nsyscol, npal, nument;

	// if the DIB doesn't have a color table, we can use the system palette

	if (mNumPalleteColors != 0) 
    return PR_FALSE;

	if (!::GetDeviceCaps(aHdc, RASTERCAPS) & RC_PALETTE) 
    return PR_FALSE;

	nsyscol = ::GetDeviceCaps(aHdc, NUMCOLORS);
	npal = ::GetDeviceCaps(aHdc, SIZEPALETTE);
	nument = (npal == 0) ? nsyscol : npal;

	LPLOGPALETTE pLogPal = (LPLOGPALETTE) new char[2 * sizeof(WORD) + nument * sizeof(PALETTEENTRY)];

	pLogPal->palVersion = 0x300;
	pLogPal->palNumEntries = nument;

	::GetSystemPaletteEntries(aHdc, 0, nument, (LPPALETTEENTRY)((LPBYTE)pLogPal + 2 * sizeof(WORD)));

	mHPalette = ::CreatePalette(pLogPal);

	delete pLogPal;

	return PR_TRUE;
}

//------------------------------------------------------------

// creates an optimized bitmap, or HBITMAP
nsresult nsImageWin :: Optimize(nsDrawingSurface aSurface)
{
  //return NS_OK;                 // TAKE THIS OUT

  HDC the_hdc = (HDC)aSurface;

  if ((the_hdc != NULL) && !IsOptimized() && (mSizeImage > 0))
  {
    mHBitmap = ::CreateDIBitmap(the_hdc, mBHead, CBM_INIT, mImageBits, (LPBITMAPINFO)mBHead, DIB_RGB_COLORS);

    mIsOptimized = PR_TRUE;
    CleanUp(PR_FALSE);
  }

  return NS_OK;
}

//------------------------------------------------------------

// figure out how big our palette needs to be
void nsImageWin :: ComputePaletteSize(PRIntn nBitCount)
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

PRInt32  nsImageWin :: CalcBytesSpan(PRUint32  aWidth)
{
  PRInt32 spanbytes;

  spanbytes = (aWidth * mBHead->biBitCount) >> 5;

	if (((PRUint32)mBHead->biWidth * mBHead->biBitCount) & 0x1F) 
		spanbytes++;

	spanbytes <<= 2;

  return(spanbytes);
}

//------------------------------------------------------------

void nsImageWin :: ComputeMetrics()
{
	mSizeImage = mBHead->biSizeImage;

	if (mSizeImage == 0) 
  {
    mRowBytes = CalcBytesSpan(mBHead->biWidth);
		mSizeImage = mRowBytes * mBHead->biHeight; // no compression
  }

  // set the color table in the info header

	mColorTable = (PRUint8 *)mBHead + sizeof(BITMAPINFOHEADER);
}
//------------------------------------------------------------


// clean up our memory
void nsImageWin :: CleanUp(PRBool aCleanUpAll)
{
	// this only happens when we need to clean up everything
  if (aCleanUpAll == PR_TRUE)
  {
    if (mAlphaBits != nsnull)
      delete [] mAlphaBits;

	  if (mHBitmap != nsnull) 
      ::DeleteObject(mHBitmap);

    if(mBHead)
      {
      delete[] mBHead;
      mBHead = nsnull;
      }

    mHBitmap = nsnull;

    mAlphaBits = nsnull;
    mIsOptimized = PR_FALSE;

	  if (mImageBits != nsnull) 
    {
      delete [] mImageBits;
      mImageBits = nsnull;
    }
  }

	if (mHPalette != nsnull) 
    ::DeleteObject(mHPalette);

	// Should be an ISupports, so we can release
  if (mColorMap != nsnull)
  {
    if (mColorMap->Index != nsnull)
      delete [] mColorMap->Index;
    delete mColorMap;
  }

	mColorTable = nsnull;
	mNumPalleteColors = -1;
  mNumBytesPixel = 0;
	mSizeImage = 0;
	mHPalette = nsnull;
  mImageBits = nsnull;
  mColorMap = nsnull;
}

//------------------------------------------------------------

static PRInt32   bcenter, gcenter, rcenter;
static PRUint32  gdist, rdist, cdist;
static PRInt32   cbinc, cginc, crinc;
static PRUint32  *gdp, *rdp, *cdp;
static PRUint8   *grgbp, *rrgbp, *crgbp;
static PRInt32   gstride,   rstride;
static PRInt32   x, xsqr, colormax;
static PRInt32   cindex;


static void maxfill(PRUint32 *buffer,PRInt32 side );
static PRInt32 redloop( void );
static PRInt32 greenloop( PRInt32 );
static PRInt32 blueloop( PRInt32 );

void
inv_cmap(PRInt16 colors,PRUint8 *aCMap,PRInt16 aBits,PRUint32 *dist_buf,PRUint8 *aRGBMap )
{
PRInt32         nbits = 8 - aBits;
PRUint32        r,g,b;

  colormax = 1 << aBits;
  x = 1 << nbits;
  xsqr = 1 << (2 * nbits);

  // Compute "strides" for accessing the arrays. */
  gstride = colormax;
  rstride = colormax * colormax;

  maxfill( dist_buf, colormax );

  for (cindex = 0;cindex < colors;cindex++ )
    {
    r = aCMap[(3 * cindex) + 2];
    g = aCMap[(3 * cindex) + 1];
    b = aCMap[(3 * cindex)];

	  rcenter = r >> nbits;
	  gcenter = g >> nbits;
	  bcenter = b >> nbits;

	  rdist = r - (rcenter * x + x/2);
	  gdist = g - (gcenter * x + x/2);
	  cdist = b - (bcenter * x + x/2);
	  cdist = rdist*rdist + gdist*gdist + cdist*cdist;

	  crinc = 2 * ((rcenter + 1) * xsqr - (r * x));
	  cginc = 2 * ((gcenter + 1) * xsqr - (g * x));
	  cbinc = 2 * ((bcenter + 1) * xsqr - (b * x));

	  // Array starting points.
	  cdp = dist_buf + rcenter * rstride + gcenter * gstride + bcenter;
	  crgbp = aRGBMap + rcenter * rstride + gcenter * gstride + bcenter;

	  (void)redloop();
    }
}

//------------------------------------------------------------

// redloop -- loop up and down from red center.
static PRInt32
redloop()
{
PRInt32        detect,r,first;
PRInt32        txsqr = xsqr + xsqr;
static PRInt32 rxx;

  detect = 0;

  rdist = cdist;
  rxx = crinc;
  rdp = cdp;
  rrgbp = crgbp;
  first = 1;
  for (r=rcenter;r<colormax;r++,rdp+=rstride,rrgbp+=rstride,rdist+=rxx,rxx+=txsqr,first=0)
    {
    if ( greenloop( first ) )
      detect = 1;
    else 
      if ( detect )
        break;
    }
   
  rxx=crinc-txsqr;
  rdist = cdist-rxx;
  rdp=cdp-rstride;
  rrgbp=crgbp-rstride;
  first=1;
  for (r=rcenter-1;r>=0;r--,rdp-=rstride,rrgbp-=rstride,rxx-=txsqr,rdist-=rxx,first=0)
    {
    if ( greenloop( first ) )
      detect = 1;
    else 
      if ( detect )
        break;
    }
    
  return detect;
}

// greenloop -- loop up and down from green center.
static PRInt32
greenloop(PRInt32 aRestart)
{
PRInt32          detect,g,first;
PRInt32          txsqr = xsqr + xsqr;
static  PRInt32  here, min, max;
static  PRInt32  ginc, gxx, gcdist;	
static  PRUint32  *gcdp;
static  PRUint8   *gcrgbp;	

  if(aRestart)
    {
    here = gcenter;
    min = 0;
    max = colormax - 1;
    ginc = cginc;
    }

  detect = 0;


  gcdp=rdp;
  gdp=rdp;
  gcrgbp=rrgbp;
  grgbp=rrgbp;
  gcdist=rdist;
  gdist=rdist;

  // loop up. 
  for(g=here,gxx=ginc,first=1;g<=max;
      g++,gdp+=gstride,gcdp+=gstride,grgbp+=gstride,gcrgbp+=gstride,
      gdist+=gxx,gcdist+=gxx,gxx+=txsqr,first=0)
    {
    if(blueloop(first))
      {
      if (!detect)
        {
        if (g>here)
          {
	        here = g;
	        rdp = gcdp;
	        rrgbp = gcrgbp;
	        rdist = gcdist;
	        ginc = gxx;
          }
        detect=1;
        }
      }
	  else 
      if (detect)
        {
        break;
        }
    }
    
  // loop down
  gcdist = rdist-gxx;
  gdist = gcdist;
  gdp=rdp-gstride;
  gcdp=gdp;
  grgbp=rrgbp-gstride;
  gcrgbp = grgbp;

  for (g=here-1,gxx=ginc-txsqr,
      first=1;g>=min;g--,gdp-=gstride,gcdp-=gstride,grgbp-=gstride,gcrgbp-=gstride,
      gxx-=txsqr,gdist-=gxx,gcdist-=gxx,first=0)
    {
    if (blueloop(first))
      {
      if (!detect)
        {
        here = g;
        rdp = gcdp;
        rrgbp = gcrgbp;
        rdist = gcdist;
        ginc = gxx;
        detect = 1;
        }
      }
    else 
      if ( detect )
        {
        break;
        }
    }
  return detect;
}

static PRInt32
blueloop(PRInt32 aRestart )
{
PRInt32  detect,b,i=cindex;
register  PRUint32  *dp;
register  PRUint8   *rgbp;
register  PRInt32   bxx;
PRUint32            bdist;
register  PRInt32   txsqr = xsqr + xsqr;
register  PRInt32   lim;
static    PRInt32   here, min, max;
static    PRInt32   binc;

  if (aRestart)
    {
    here = bcenter;
    min = 0;
    max = colormax - 1;
    binc = cbinc;
    }

  detect = 0;
  bdist = gdist;

// Basic loop, finds first applicable cell.
  for (b=here,bxx=binc,dp=gdp,rgbp=grgbp,lim=max;b<=lim;
      b++,dp++,rgbp++,bdist+=bxx,bxx+=txsqr)
    {
    if(*dp>bdist)
      {
      if(b>here)
        {
        here = b;
        gdp = dp;
        grgbp = rgbp;
        gdist = bdist;
        binc = bxx;
        }
      detect = 1;
      break;
      }
    }

// Second loop fills in a run of closer cells.
for (;b<=lim;b++,dp++,rgbp++,bdist+=bxx,bxx+=txsqr)
  {
  if (*dp>bdist)
    {
    *dp = bdist;
    *rgbp = i;
    }
  else
    {
    break;
    }
  }

// Basic loop down, do initializations here
lim = min;
b = here - 1;
bxx = binc - txsqr;
bdist = gdist - bxx;
dp = gdp - 1;
rgbp = grgbp - 1;

// The 'find' loop is executed only if we didn't already find something.
if (!detect)
  for (;b>=lim;b--,dp--,rgbp--,bxx-=txsqr,bdist-=bxx)
    {
    if (*dp>bdist)
      {
      here = b;
      gdp = dp;
      grgbp = rgbp;
      gdist = bdist;
      binc = bxx;
      detect = 1;
      break;
      }
    }

// Update loop.
for (;b>=lim;b--,dp--,rgbp--,bxx-=txsqr,bdist-=bxx)
  {
  if (*dp>bdist)
    {
    *dp = bdist;
    *rgbp = i;
    }
  else
    {
    break;
    }
  }

// If we saw something, update the edge trackers.
return detect;
}

static void
maxfill(PRUint32 *buffer,PRInt32 side )
{
register PRInt32 maxv = ~0L;
register PRInt32 i;
register PRUint32 *bp;

for (i=side*side*side,bp=buffer;i>0;i--,bp++)
  *bp = maxv;
}
