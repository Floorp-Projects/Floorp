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

HDC nsImageWin::mOptimizeDC = nsnull;

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
      mAlphaBits = new unsigned char[aWidth * aHeight];

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
void nsImageWin :: ImageUpdated(PRUint8 aFlags, nsRect *aUpdateRect)
{
  PRInt32 i;
  PRUint8 *cpointer;

  if (aFlags & nsImageUpdateFlags_kColorMapChanged)
  {
    if (mColorMap->NumColors > 0)
    {
      cpointer = mColorTable;

      for(i = 0; i < mColorMap->NumColors; i++)
      {
		    *cpointer++ = mColorMap->Index[(3 * i) + 2];
		    *cpointer++ = mColorMap->Index[(3 * i) + 1];
		    *cpointer++ = mColorMap->Index[(3 * i)];
		    *cpointer++ = 0;
      }
    }

    this->MakePalette();
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
PRBool nsImageWin :: Draw(nsDrawingSurface aSurface, PRInt32 aSX, PRInt32 aSY, PRInt32 aSWidth, PRInt32 aSHeight,
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
    SelectObject(mOptimizeDC,mHBitmap);
 
    if (!::StretchBlt(the_hdc,aDX,aDY,aDWidth,aDHeight,mOptimizeDC,aSX,aSY,
                      aSWidth,aSHeight,SRCCOPY))
      error = ::GetLastError();
  }

  return PR_TRUE;
}

//------------------------------------------------------------

// Draw the bitmap, this draw just has destination coordinates
PRBool nsImageWin :: Draw(nsDrawingSurface aSurface, PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
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
    SelectObject(mOptimizeDC,mHBitmap);
    //if((aWidth == mBHead->biWidth) && (aHeight == mBHead->biHeight))
    //BitBlt(the_hdc,aX,aY,aWidth,aHeight,mOptimizeDC,0,0,SRCCOPY);
    
    if (!::StretchBlt(the_hdc,aX,aY,aWidth,aHeight,mOptimizeDC,0,0,
                      mBHead->biWidth,mBHead->biHeight,SRCCOPY))
      error = ::GetLastError();
  }

  return PR_TRUE;
}

//------------------------------------------------------------

void nsImageWin::CompositeImage(nsIImage *aTheImage, nsPoint *aULLocation)
{
  // call the correct sub routine for each blend

  if ((mNumBytesPixel == 3) && (((nsImageWin *)aTheImage)->mNumBytesPixel == 3))
    this->Comp24to24((nsImageWin*)aTheImage, aULLocation);

}

//------------------------------------------------------------

// lets build an alpha mask from this image
PRBool nsImageWin::SetAlphaMask(nsIImage *aTheMask)
{
  PRInt32 num;
  LPBYTE  srcbits;

  if (aTheMask && (((nsImageWin*)aTheMask)->mNumBytesPixel == 1))
  {
    mLocation.x = 0;
    mLocation.y = 0;
    mlphaDepth = 8;
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

// this routine has to flip the y, since the bits are in bottom scan
// line to top.
void nsImageWin::Comp24to24(nsImageWin *aTheImage, nsPoint *aULLocation)
{
  nsRect  arect, srect, drect, irect;
  PRInt32 dlinespan, slinespan, mlinespan, startx, starty, numbytes, numlines, x, y;
  LPBYTE  d1, d2, s1, s2, m1, m2;
  double  a1, a2;

  if (mAlphaBits)
  {
    x = mLocation.x;
    y = mLocation.y;
    arect.SetRect(0, 0, this->GetWidth(), this->GetHeight());
    srect.SetRect(mLocation.x, mLocation.y, mAlphaWidth, mAlphaHeight);
    arect.IntersectRect(arect, srect);
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

    dlinespan = this->GetLineStride();
    numbytes = this->CalcBytesSpan(irect.width);
    numlines = irect.height;
    startx = irect.x;
    starty = this->GetHeight() - (irect.y + irect.height);
    d1 = mImageBits + (starty * dlinespan) + (3 * startx);

    // get the intersection relative to the source rectangle
    srect.SetRect(0, 0, aTheImage->GetWidth(), aTheImage->GetHeight());
    drect = irect;
    drect.MoveBy(-aULLocation->x, -aULLocation->y);

    drect.IntersectRect(drect,srect);
    slinespan = aTheImage->GetLineStride();
    startx = drect.x;
    starty = aTheImage->GetHeight() - (drect.y + drect.height);
    s1 = aTheImage->GetBits() + (starty * slinespan) + (3 * startx);

    if (mAlphaBits)
    {
      mlinespan = this->GetAlphaLineStride();
      m1 = mAlphaBits;
      numbytes /= 3;

      // now go thru the image and blend (remember, its bottom upwards)

      for (y = 0; y < numlines; y++)
      {
        s2 = s1;
        d2 = d1;
        m2 = m1;

        for (x = 0; x < numbytes; x++)
        {
          a1 = (*m2) * (1.0 / 256.0);
          a2 = 1.0 - a1;
          *d2 = (PRUint8)((*d2) * a1 + (*s2) * a2);
          d2++;
          s2++;
          
          *d2 = (PRUint8)((*d2) * a1 + (*s2) * a2);
          d2++;
          s2++;

          *d2 = (PRUint8)((*d2) * a1 + (*s2) * a2);
          d2++;
          s2++;
          m2++;
        }

        s1 += slinespan;
        d1 += dlinespan;
        m1 += mlinespan;
      }
    }
    else
    {
      // now go thru the image and blend (remember, its bottom upwards)

      for(y = 0; y < numlines; y++)
      {
        s2 = s1;
        d2 = d1;

        for(x = 0; x < numbytes; x++)
        {
          *d2 = (*d2 + *s2) >> 1;
          d2++;
          s2++;
        }

        s1 += slinespan;
        d1 += dlinespan;
      }
    }
  }
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
  nsRenderingContextWin *therc = (nsRenderingContextWin*)aSurface;
  HDC                   the_hdc;

  if ((therc != nsnull) && !IsOptimized() && (mSizeImage > 0))
  {
    the_hdc = therc->getDrawingSurface();
  
    if (mOptimizeDC == nsnull)
      mOptimizeDC = therc->CreateOptimizeSurface();

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

  spanbytes = (aWidth * mBHead->biBitCount) / 32; 

	if (((PRUint32)mBHead->biWidth * mBHead->biBitCount) % 32) 
		spanbytes++;

	spanbytes *= 4;

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
      delete[] mBHead;

    mHBitmap = nsnull;

    mAlphaBits = nsnull;
    mIsOptimized = PR_FALSE;
	  mBHead = nsnull;

	  if (mImageBits != nsnull) 
    {
      delete [] mImageBits;
      mImageBits = nsnull;
    }
  }

  // clean up the DIB
	if (mImageBits != nsnull) 
    delete [] mImageBits;

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
