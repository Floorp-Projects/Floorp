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

  mSaveBytes = nsnull;
  mSrcBytes = nsnull;
  mDstBytes = nsnull;
  mTempB1 = nsnull;
  mTempB2 = nsnull;
  mSaveLS = 0;
  mSaveNumLines = 0;

}

//------------------------------------------------------------

nsBlenderWin :: ~nsBlenderWin()
{
  CleanUp();
}

NS_IMPL_ISUPPORTS(nsBlenderWin, kIBlenderIID);

//------------------------------------------------------------

nsresult
nsBlenderWin::Init(nsDrawingSurface aSrc,nsDrawingSurface aDst)
{
PRInt32             numbytes;
HDC                 srcdc,dstdc;
HBITMAP             srcbits,dstbits;

  if(mSaveBytes != nsnull)
    delete [] mSaveBytes;
  mSaveBytes = nsnull;

  // lets build some DIB's for the source and destination from the HDC's
  srcdc = (HDC)aSrc;
  dstdc = (HDC)aDst;

  mSrcDC = srcdc;
  mDstDC = dstdc;

  // source
  mTempB1 = CreateCompatibleBitmap(srcdc,3,3);
  srcbits = ::SelectObject(srcdc, mTempB1);
  numbytes = ::GetObject(srcbits,sizeof(BITMAP),&mSrcInfo);
  BuildDIB(&mSrcbinfo,&mSrcBytes,mSrcInfo.bmWidth,mSrcInfo.bmHeight,mSrcInfo.bmBitsPixel);
  numbytes = ::GetDIBits(srcdc,srcbits,0,mSrcInfo.bmHeight,mSrcBytes,(LPBITMAPINFO)mSrcbinfo,DIB_RGB_COLORS);

  if(numbytes > 0)
    {
    // destination
    mTempB2 = CreateCompatibleBitmap(dstdc,3,3);
    dstbits = ::SelectObject(dstdc, mTempB2);
    ::GetObject(dstbits,sizeof(BITMAP),&mDstInfo);
    BuildDIB(&mDstbinfo,&mDstBytes,mDstInfo.bmWidth,mDstInfo.bmHeight,mDstInfo.bmBitsPixel);
    numbytes = ::GetDIBits(dstdc,dstbits,0,mDstInfo.bmHeight,mDstBytes,(LPBITMAPINFO)mDstbinfo,DIB_RGB_COLORS);
    
    // put the old stuff back
    ::SelectObject(srcdc,srcbits);
    ::SelectObject(dstdc,dstbits);
    }

  return NS_OK;
}

//------------------------------------------------------------

void
nsBlenderWin::CleanUp()
{

  if(mTempB1)
    ::DeleteObject(mTempB1);
  if(mTempB2)
  ::DeleteObject(mTempB2);

  // get rid of the DIB's
  DeleteDIB(&mSrcbinfo,&mSrcBytes);
  DeleteDIB(&mDstbinfo,&mDstBytes);

  if(mSaveBytes != nsnull)
    delete [] mSaveBytes;
  mSaveBytes == nsnull;

  mTempB1 = nsnull;
  mTempB2 = nsnull;
  mDstBytes = nsnull;
  mSaveLS = 0;
  mSaveNumLines = 0;
}

//------------------------------------------------------------

void
nsBlenderWin::Blend(nsDrawingSurface aSrc,PRInt32 aSX, PRInt32 aSY, PRInt32 aWidth, PRInt32 aHeight,
                     nsDrawingSurface aDst, PRInt32 aDX, PRInt32 aDY, float aSrcOpacity,PRBool aSaveBlendArea)
{
HDC                 dstdc,tb1;
HBITMAP             dstbits;
nsPoint             srcloc,maskloc;
PRInt32             dlinespan,slinespan,mlinespan,numbytes,numlines,level,size,oldsize;
PRUint8             *s1,*d1,*m1,*mask=NULL;
nsColorMap          *colormap;


  // calculate the metrics, no mask right now
  srcloc.x = aSX;
  srcloc.y = aSY;
  mSrcInfo.bmBits = mSrcBytes;
  mDstInfo.bmBits = mDstBytes;
  maskloc.x = aSX;
  maskloc.y = aSY;
  if(CalcAlphaMetrics(&mSrcInfo,&mDstInfo,&srcloc,NULL,&maskloc,aWidth,aHeight,&numlines,&numbytes,
                &s1,&d1,&m1,&slinespan,&dlinespan,&mlinespan))
    {
    if( aSaveBlendArea )
      {
      oldsize = mSaveLS*mSaveNumLines;

      // allocate some memory
      mSaveLS = CalcBytesSpan(numbytes,mDstInfo.bmBitsPixel);
      mSaveNumLines = numlines;
      size = mSaveLS * numlines; 
      mSaveNumBytes = numbytes;

      if(mSaveBytes != nsnull) 
        {
        if(oldsize != size)
          {
          delete [] mSaveBytes;
          mSaveBytes = new unsigned char[size];
          }
        }
      else
        mSaveBytes = new unsigned char[size];

      mRestorePtr = d1;
      mResLS = dlinespan;
      }

    // now do the blend
    if ((mSrcInfo.bmBitsPixel==24) && (mDstInfo.bmBitsPixel==24))
      {
      if(mask)
        {
        numbytes/=3;    // since the mask is only 8 bits, this routine wants number of pixels
        this->Do24BlendWithMask(numlines,numbytes,s1,d1,m1,slinespan,dlinespan,mlinespan,nsHighQual,aSaveBlendArea);
        }
      else
        {
          level = (PRInt32)(aSrcOpacity*100);
        this->Do24Blend(level,numlines,numbytes,s1,d1,slinespan,dlinespan,nsHighQual,aSaveBlendArea);
        }
      }
    else
      if ((mSrcInfo.bmBitsPixel==8) && (mDstInfo.bmBitsPixel==8))
        {
        if(mask)
          {
          this->Do8BlendWithMask(numlines,numbytes,s1,d1,m1,slinespan,dlinespan,mlinespan,nsHighQual,aSaveBlendArea);
          }
        else
          {
          level = (PRInt32)(aSrcOpacity*100);
          this->Do8Blend(level,numlines,numbytes,s1,d1,slinespan,dlinespan,colormap,nsHighQual,aSaveBlendArea);
          }
        }

      // put the new bits in
    dstdc = (HDC)aDst;
    dstbits = ::CreateDIBitmap(dstdc, mDstbinfo, CBM_INIT, mDstBytes, (LPBITMAPINFO)mDstbinfo, DIB_RGB_COLORS);
    tb1 = ::SelectObject(dstdc,dstbits);
    ::DeleteObject(tb1);
    }

}

//------------------------------------------------------------

PRBool
nsBlenderWin::RestoreImage(nsDrawingSurface aDst)
{
PRBool    result = PR_FALSE;
PRInt32   y,x;
PRUint8   *saveptr,*savebyteptr;
PRUint8   *orgptr,*orgbyteptr;
HDC       dstdc,tb1;
HBITMAP   dstbits;


  if(mSaveBytes!=nsnull)
    {
    result = PR_TRUE;
    saveptr = mSaveBytes;
    orgptr = mRestorePtr;
    for(y=0;y<mSaveNumLines;y++)
      {
      savebyteptr = saveptr;
      orgbyteptr = orgptr;
      for(x=0;x<mSaveNumBytes;x++)
        {
        *orgbyteptr = *savebyteptr;
        savebyteptr++;
        orgbyteptr++;
        }
      saveptr+=mSaveLS;
      orgptr+=mResLS;
      }

      // put the new bits in
    dstdc = (HDC)aDst;
    dstbits = ::CreateDIBitmap(dstdc, mDstbinfo, CBM_INIT, mDstBytes, (LPBITMAPINFO)mDstbinfo, DIB_RGB_COLORS);
    tb1 = ::SelectObject(dstdc,dstbits);
    ::DeleteObject(tb1);
    }
  return(result);
}


//------------------------------------------------------------

PRBool 
nsBlenderWin::CalcAlphaMetrics(BITMAP *aSrcInfo,BITMAP *aDestInfo,nsPoint *aSrcUL,
                              BITMAP  *aMaskInfo,nsPoint *aMaskUL,
                              PRInt32 aWidth,PRInt32 aHeight,
                              PRInt32 *aNumlines,
                              PRInt32 *aNumbytes,PRUint8 **aSImage,PRUint8 **aDImage,
                              PRUint8 **aMImage,PRInt32 *aSLSpan,PRInt32 *aDLSpan,PRInt32 *aMLSpan)
{
PRBool    doalpha = PR_FALSE;
nsRect    arect,srect,drect,irect;
PRInt32   startx,starty;


  if(aMaskInfo)
    {
    arect.SetRect(0,0,aDestInfo->bmWidth,aDestInfo->bmHeight);
    srect.SetRect(aMaskUL->x,aMaskUL->y,aMaskInfo->bmWidth,aSrcInfo->bmHeight);
    arect.IntersectRect(arect,srect);
    }
  else
    {
    arect.SetRect(0,0,aDestInfo->bmWidth,aDestInfo->bmHeight);
    srect.SetRect(aMaskUL->x,aMaskUL->y,aWidth,aHeight);
    arect.IntersectRect(arect,srect);

    //arect.SetRect(0, 0,aDestInfo->bmWidth, aDestInfo->bmHeight);
    //x = y = 0;
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

    spanbytes = CalcBytesSpan(aWidth,aDepth);

    imagesize = spanbytes * (*aBHead)->biHeight;    // no compression

    // set the color table in the info header
	  colortable = (PRUint8 *)(*aBHead) + sizeof(BITMAPINFOHEADER);

	  memset(colortable, 0, sizeof(RGBQUAD) * numpalletcolors);
    *aBits = new unsigned char[imagesize];
    memset(*aBits, 128, imagesize);
  }

  return NS_OK;
}

//------------------------------------------------------------

void
nsBlenderWin::DeleteDIB(LPBITMAPINFOHEADER  *aBHead,unsigned char **aBits)
{

  delete[] *aBHead;
  aBHead = 0;
  delete[] *aBits;
  aBits = 0;

}

//------------------------------------------------------------

// This routine can not be fast enough
void
nsBlenderWin::Do24BlendWithMask(PRInt32 aNumlines,PRInt32 aNumbytes,PRUint8 *aSImage,PRUint8 *aDImage,PRUint8 *aMImage,PRInt32 aSLSpan,PRInt32 aDLSpan,PRInt32 aMLSpan,nsBlendQuality aBlendQuality,PRBool aSaveBlendArea)
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
nsBlenderWin::Do24Blend(PRUint8 aBlendVal,PRInt32 aNumlines,PRInt32 aNumbytes,PRUint8 *aSImage,PRUint8 *aDImage,PRInt32 aSLSpan,PRInt32 aDLSpan,nsBlendQuality aBlendQuality,PRBool aSaveBlendArea)
{
PRUint8   *d1,*d2,*s1,*s2;
PRUint32  val1,val2;
PRInt32   x,y,temp1,numlines,xinc,yinc;
PRUint8   *saveptr,*sv2;

  saveptr = mSaveBytes;
  aBlendVal = (aBlendVal*255)/100;
  val2 = aBlendVal;
  val1 = 255-val2;

  // now go thru the image and blend (remember, its bottom upwards)
  s1 = aSImage;
  d1 = aDImage;

  numlines = aNumlines;  
  xinc = 1;
  yinc = 1;

  if(aSaveBlendArea)
    {
    for(y = 0; y < aNumlines; y++)
      {
      s2 = s1;
      d2 = d1;
      sv2 = saveptr;

      for(x = 0; x < aNumbytes; x++)
        {
        temp1 = (((*d2)*val1)+((*s2)*val2))>>8;
        if(temp1>255)
          temp1 = 255;
        *sv2 = *d2;
        *d2 = (unsigned char)temp1; 

        sv2++;
        d2++;
        s2++;
        }

      s1 += aSLSpan;
      d1 += aDLSpan;
      saveptr+= mSaveLS;
      }
    }
  else
    {
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

}

//------------------------------------------------------------

// This routine can not be fast enough
void
nsBlenderWin::Do8BlendWithMask(PRInt32 aNumlines,PRInt32 aNumbytes,PRUint8 *aSImage,PRUint8 *aDImage,PRUint8 *aMImage,PRInt32 aSLSpan,PRInt32 aDLSpan,PRInt32 aMLSpan,nsBlendQuality aBlendQuality,PRBool aSaveBlendArea)
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

extern void inv_colormap(PRInt16 colors,PRUint8 *aCMap,PRInt16 bits,PRUint32 *dist_buf,PRUint8 *aRGBMap );

// This routine can not be fast enough
void
nsBlenderWin::Do8Blend(PRUint8 aBlendVal,PRInt32 aNumlines,PRInt32 aNumbytes,PRUint8 *aSImage,PRUint8 *aDImage,PRInt32 aSLSpan,PRInt32 aDLSpan,nsColorMap *aColorMap,nsBlendQuality aBlendQuality,PRBool aSaveBlendArea)
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
  mapptr = aColorMap->Index;       
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
  inv_colormap(256,mapptr,quantlevel,distbuffer,invermap );

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
      r = aColorMap->Index[(3 * i) + 2];
      g = aColorMap->Index[(3 * i) + 1];
      b = aColorMap->Index[(3 * i)];

      i =(*s2);
      r1 = aColorMap->Index[(3 * i) + 2];
      g1 = aColorMap->Index[(3 * i) + 1];
      b1 = aColorMap->Index[(3 * i)];

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

  delete[] distbuffer;
  delete[] invermap;
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

//------------------------------------------------------------

void
inv_colormap(PRInt16 colors,PRUint8 *aCMap,PRInt16 aBits,PRUint32 *dist_buf,PRUint8 *aRGBMap )
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

//------------------------------------------------------------

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

//------------------------------------------------------------

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

//------------------------------------------------------------

static void
maxfill(PRUint32 *buffer,PRInt32 side )
{
register PRInt32 maxv = ~0L;
register PRInt32 i;
register PRUint32 *bp;

for (i=side*side*side,bp=buffer;i>0;i--,bp++)
  *bp = maxv;
}

//------------------------------------------------------------

