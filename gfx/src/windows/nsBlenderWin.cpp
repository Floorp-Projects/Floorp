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

#include <windows.h>
#include "nsBlenderWin.h"

static NS_DEFINE_IID(kIBlenderIID, NS_IBLENDER_IID);

//------------------------------------------------------------

nsBlenderWin :: nsBlenderWin()
{

  NS_INIT_REFCNT();

}

//------------------------------------------------------------

nsBlenderWin :: ~nsBlenderWin()
{

}

NS_IMPL_ISUPPORTS(nsBlenderWin, kIBlenderIID);

//------------------------------------------------------------

nsresult
nsBlenderWin::Init()
{

   return NS_OK;
}

//------------------------------------------------------------

void
nsBlenderWin::Blend(nsDrawingSurface aSrc,PRInt32 aSX, PRInt32 aSY, PRInt32 aWidth, PRInt32 aHeight,
                     nsDrawingSurface aDst, PRInt32 aDX, PRInt32 aDY, float aSrcOpacity)
{
HDC                 srcdc,dstdc;
HBITMAP             srcbits,dstbits,tb1,tb2;
BITMAP              srcinfo,dstinfo;
nsPoint             srcloc,maskloc;
PRInt32             dlinespan,slinespan,mlinespan,numbytes,numlines;
PRUint8             *s1,*d1,*m1,*srcbytes,*dstbytes;
LPBITMAPINFOHEADER  srcbinfo,dstbinfo;


  // we have to extract the bitmaps from the nsDrawingSurface, which in this case is a hdc
  srcdc = (HDC)aSrc;
  dstdc = (HDC)aDst;

  // we use this bitmap to select into the HDC's while we work on the ones they give us
  tb1 = CreateCompatibleBitmap(aSrc,3,3);

  // get the HBITMAP, and then grab the information about the source bitmap and bits
  srcbits = ::SelectObject(srcdc, tb1);
  numbytes = ::GetObject(srcbits,sizeof(BITMAP),&srcinfo);
  // put into a DIB
  BuildDIB(&srcbinfo,&srcbytes,srcinfo.bmWidth,srcinfo.bmHeight,srcinfo.bmBitsPixel);
  numbytes = ::GetDIBits(srcdc,srcbytes,1,srcinfo.bmHeight,srcbits,(LPBITMAPINFO)srcbinfo,DIB_RGB_COLORS);

  // get the HBITMAP, and then grab the information about the destination bitmap
  tb2 = CreateCompatibleBitmap(aSrc,3,3);
  dstbits = ::SelectObject(dstdc, tb2);
  ::GetObject(dstbits,sizeof(BITMAP),&dstinfo);
  // put into a DIB
  BuildDIB(&dstbinfo,&dstbytes,dstinfo.bmWidth,dstinfo.bmHeight,dstinfo.bmBitsPixel);
  numbytes = ::GetDIBits(dstdc,dstbytes,1,dstinfo.bmHeight,dstbits,(LPBITMAPINFO)dstbinfo,DIB_RGB_COLORS);

  // calculate the metrics, no mask right now
  if(CalcAlphaMetrics(&srcinfo,&dstinfo,&srcloc,NULL,&maskloc,&numlines,&numbytes,
                &s1,&d1,&m1,&slinespan,&dlinespan,&mlinespan))
    {
    // now do the blend
    

    }

  ::SelectObject(srcdc,srcbits);
  ::SelectObject(dstdc,dstbits);
}

//------------------------------------------------------------

PRBool 
nsBlenderWin::CalcAlphaMetrics(BITMAP *aSrcInfo,BITMAP *aDestInfo,nsPoint *aSrcUL,
                              BITMAP  *aMaskInfo,nsPoint *aMaskUL,
                              PRInt32 *aNumlines,
                              PRInt32 *aNumbytes,PRUint8 **aSImage,PRUint8 **aDImage,
                              PRUint8 **aMImage,PRInt32 *aSLSpan,PRInt32 *aDLSpan,PRInt32 *aMLSpan)
{
PRBool    doalpha = PR_FALSE;
nsRect    arect,srect,drect,irect;
PRInt32   startx,starty,x,y;


  if(aMaskInfo)
    {
    arect.SetRect(0,0,aDestInfo->bmWidth,aDestInfo->bmHeight);
    srect.SetRect(aMaskUL->x,aMaskUL->y,aMaskInfo->bmWidth,aSrcInfo->bmHeight);
    arect.IntersectRect(arect,srect);
    }
  else
    {
    arect.SetRect(0, 0,aDestInfo->bmWidth, aDestInfo->bmHeight);
    x = y = 0;
    }

  srect.SetRect(aSrcUL->x, aSrcUL->y, aSrcInfo->bmWidth, aSrcInfo->bmHeight);
  drect = arect;

  if (irect.IntersectRect(srect, drect))
    {
    // calculate destination information
    *aDLSpan = aDestInfo->bmWidthBytes;
    *aNumbytes = this->CalcBytesSpan(irect.width,aDestInfo->bmBitsPixel);
    *aNumlines = irect.height;
    startx = irect.x;
    starty = aDestInfo->bmHeight - (irect.y + irect.height);
    *aDImage = ((PRUint8*)aDestInfo->bmBits) + (starty * (*aDLSpan)) + (aDestInfo->bmBitsPixel * startx);

    // get the intersection relative to the source rectangle
    srect.SetRect(0, 0, aSrcInfo->bmWidth, aSrcInfo->bmHeight);
    drect = irect;
    drect.MoveBy(-aSrcUL->x, -aSrcUL->y);

    drect.IntersectRect(drect,srect);
    *aSLSpan = aSrcInfo->bmWidthBytes;
    startx = drect.x;
    starty = aSrcInfo->bmHeight - (drect.y + drect.height);
    *aSImage = ((PRUint8*)aSrcInfo->bmBits) + (starty * (*aSLSpan)) + (aDestInfo->bmBitsPixel * startx);
         
    doalpha = PR_TRUE;

    if(aMaskInfo)
      {
      *aMLSpan = aMaskInfo->bmWidthBytes;
      *aMImage = (PRUint8*)aMaskInfo->bmBits;
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

PRInt32  
nsBlenderWin :: CalcBytesSpan(PRUint32  aWidth,PRUint32  aBitsPixel)
{
PRInt32 spanbytes;

  spanbytes = (aWidth * aBitsPixel) >> 5;

	if ((aWidth * aBitsPixel) & 0x1F) 
		spanbytes++;

	spanbytes <<= 2;

  return(spanbytes);
}


//-----------------------------------------------------------

nsresult 
nsBlenderWin :: BuildDIB(LPBITMAPINFOHEADER  *aBHead,unsigned char **aBits,PRInt32 aWidth, PRInt32 aHeight, PRInt32 aDepth)
{
PRInt32   numpalletcolors,imagesize,spanbytes;
PRUint8   *colortable;


	switch (aDepth) 
    {
		case 8:
			numpalletcolors = 256;
      break;
		case 24:
			numpalletcolors = 0;
      break;
		default:
			numpalletcolors = -1;
      break;
    }

  if (numpalletcolors >= 0)
    {
	  (*aBHead) = (LPBITMAPINFOHEADER) new char[sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD) * numpalletcolors];
    (*aBHead)->biSize = sizeof(BITMAPINFOHEADER);
	  (*aBHead)->biWidth = aWidth;
	  (*aBHead)->biHeight = aHeight;
	  (*aBHead)->biPlanes = 1;
	  (*aBHead)->biBitCount = (unsigned short) aDepth;
	  (*aBHead)->biCompression = BI_RGB;
	  (*aBHead)->biSizeImage = 0;            // not compressed, so we dont need this to be set
	  (*aBHead)->biXPelsPerMeter = 0;
	  (*aBHead)->biYPelsPerMeter = 0;
	  (*aBHead)->biClrUsed = numpalletcolors;
	  (*aBHead)->biClrImportant = numpalletcolors;

    spanbytes = ((*aBHead)->biWidth * (*aBHead)->biBitCount) >> 5;
	  if (((PRUint32)(*aBHead)->biWidth * (*aBHead)->biBitCount) & 0x1F) 
		  spanbytes++;
    spanbytes <<= 2;
    imagesize = spanbytes * (*aBHead)->biHeight;    // no compression

    // set the color table in the info header
	  colortable = (PRUint8 *)(*aBHead) + sizeof(BITMAPINFOHEADER);

	  memset(colortable, 0, sizeof(RGBQUAD) * numpalletcolors);
    *aBits = new unsigned char[imagesize];
    memset(*aBits, 128, imagesize);
  }

  return NS_OK;
}
