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
#include "nsRenderingContextWin.h"

#ifdef NGLAYOUT_DDRAW
#include "ddraw.h"
#endif

static NS_DEFINE_IID(kIBlenderIID, NS_IBLENDER_IID);

//------------------------------------------------------------

nsBlenderWin :: nsBlenderWin()
{
  NS_INIT_REFCNT();

  mSaveBytes = nsnull;
  mSrcBytes = nsnull;
  mDstBytes = nsnull;
  mSaveLS = 0;
  mSaveNumLines = 0;
  mSrcbinfo = nsnull;
  mDstbinfo = nsnull;
  mRestorePtr = nsnull;
  mSaveNumBytes = 0;
  mResLS = 0;
  mSRowBytes = 0;
  mDRowBytes = 0;
  mSrcDS = nsnull;
  mDstDS = nsnull;
}

//------------------------------------------------------------

nsBlenderWin :: ~nsBlenderWin()
{
  NS_IF_RELEASE(mSrcDS);
  NS_IF_RELEASE(mDstDS);

  // get rid of the DIB's

  if (nsnull != mSrcbinfo)
    DeleteDIB(&mSrcbinfo, &mSrcBytes);

  if (nsnull != mDstbinfo)
    DeleteDIB(&mDstbinfo, &mDstBytes);

  if (mSaveBytes != nsnull)
  {
    delete [] mSaveBytes;
    mSaveBytes == nsnull;
  }

  mDstBytes = nsnull;
  mSaveLS = 0;
  mSaveNumLines = 0;
}

NS_IMPL_ISUPPORTS(nsBlenderWin, kIBlenderIID);

//------------------------------------------------------------

nsresult
nsBlenderWin::Init(nsDrawingSurface aSrc, nsDrawingSurface aDst)
{
  NS_ASSERTION(!(aSrc == nsnull), "no source surface");
  NS_ASSERTION(!(aDst == nsnull), "no dest surface");

  if (mSaveBytes != nsnull)
  {
    delete [] mSaveBytes;
    mSaveBytes = nsnull;
  }

  mSrcDS = (nsDrawingSurfaceWin *)aSrc;
  mDstDS = (nsDrawingSurfaceWin *)aDst;

  NS_IF_ADDREF(mSrcDS);
  NS_IF_ADDREF(mDstDS);

  return NS_OK;
}

//------------------------------------------------------------

nsresult
nsBlenderWin::Blend(PRInt32 aSX, PRInt32 aSY, PRInt32 aWidth, PRInt32 aHeight,
                    nsDrawingSurface aDst, PRInt32 aDX, PRInt32 aDY, float aSrcOpacity,PRBool aSaveBlendArea)
{
  nsresult    result = NS_ERROR_FAILURE;
  HBITMAP     dstbits, tb1;
  nsPoint     srcloc, maskloc;
  PRInt32     dlinespan, slinespan, mlinespan, numbytes, numlines, level, size, oldsize;
  PRUint8     *s1, *d1, *m1, *mask = NULL;
  nsColorMap  *colormap;
  HDC         srcdc, dstdc;
  PRBool      srcissurf = PR_FALSE, dstissurf = PR_FALSE;

  // source

#ifdef NGLAYOUT_DDRAW
  RECT  srect;

  srect.left = aSX;
  srect.top = aSY;
  srect.right = aSX + aWidth;
  srect.bottom = aSY + aHeight;

  if (PR_TRUE == LockSurface(mSrcDS->mSurface, &mSrcSurf, &mSrcInfo, &srect, DDLOCK_READONLY))
  {
    srcissurf = PR_TRUE;
    mSRowBytes = mSrcInfo.bmWidthBytes;
  }
  else
#endif
  {
    if (nsnull == mSrcbinfo)
    {
      HBITMAP srcbits;
      srcdc = mSrcDS->mDC;

      if (nsnull == mSrcDS->mSelectedBitmap)
      {
        HBITMAP hbits = ::CreateCompatibleBitmap(srcdc, 2, 2);
        srcbits = (HBITMAP)::SelectObject(srcdc, hbits);
        ::GetObject(srcbits, sizeof(BITMAP), &mSrcInfo);
        ::SelectObject(srcdc, srcbits);
        ::DeleteObject(hbits);
      }
      else
      {
        ::GetObject(mSrcDS->mSelectedBitmap, sizeof(BITMAP), &mSrcInfo);
        srcbits = mSrcDS->mSelectedBitmap;
      }

      BuildDIB(&mSrcbinfo, &mSrcBytes, mSrcInfo.bmWidth, mSrcInfo.bmHeight, mSrcInfo.bmBitsPixel);
      numbytes = ::GetDIBits(srcdc, srcbits, 0, mSrcInfo.bmHeight, mSrcBytes, (LPBITMAPINFO)mSrcbinfo, DIB_RGB_COLORS);

      mSRowBytes = CalcBytesSpan(mSrcInfo.bmWidth, mSrcInfo.bmBitsPixel);
    }

    mSrcInfo.bmBits = mSrcBytes;
  }

  // destination

#ifdef NGLAYOUT_DDRAW
  RECT  drect;

  drect.left = aDX;
  drect.top = aDY;
  drect.right = aDX + aWidth;
  drect.bottom = aDY + aHeight;

  if (PR_TRUE == LockSurface(mDstDS->mSurface, &mDstSurf, &mDstInfo, &drect, 0))
  {
    dstissurf = PR_TRUE;
    mDRowBytes = mDstInfo.bmWidthBytes;
  }
  else
#endif
  {
    if (nsnull == mDstbinfo)
    {
      HBITMAP dstbits;
      dstdc = mDstDS->mDC;

      if (nsnull == mDstDS->mSelectedBitmap)
      {
        HBITMAP hbits = CreateCompatibleBitmap(dstdc, 2, 2);
        dstbits = (HBITMAP)::SelectObject(dstdc, hbits);
        ::GetObject(dstbits, sizeof(BITMAP), &mDstInfo);
        ::SelectObject(dstdc, dstbits);
        ::DeleteObject(hbits);
      }
      else
      {
        ::GetObject(mDstDS->mSelectedBitmap, sizeof(BITMAP), &mDstInfo);
        dstbits = mDstDS->mSelectedBitmap;
      }

      BuildDIB(&mDstbinfo, &mDstBytes, mDstInfo.bmWidth, mDstInfo.bmHeight, mDstInfo.bmBitsPixel);
      numbytes = ::GetDIBits(dstdc, dstbits, 0, mDstInfo.bmHeight, mDstBytes, (LPBITMAPINFO)mDstbinfo, DIB_RGB_COLORS);
  
      mDRowBytes = CalcBytesSpan(mDstInfo.bmWidth, mDstInfo.bmBitsPixel);
    }

    mDstInfo.bmBits = mDstBytes;
  }

  // calculate the metrics, no mask right now
  srcloc.x = aSX;
  srcloc.y = aSY;
  maskloc.x = 0;
  maskloc.y = 0;

  if (CalcAlphaMetrics(&mSrcInfo, &mDstInfo, &srcloc, NULL, &maskloc, aWidth, aHeight, &numlines, &numbytes,
                       &s1, &d1, &m1, &slinespan, &dlinespan, &mlinespan))
  {
    if (nsnull != aSaveBlendArea)
    {
      oldsize = mSaveLS * mSaveNumLines;

      // allocate some memory
      mSaveLS = numbytes;
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

    if (mSrcInfo.bmBitsPixel == mDstInfo.bmBitsPixel)
    {
      // now do the blend
      switch (mSrcInfo.bmBitsPixel)
      {
        case 32:
          if (!mask)
          {
            level = (PRInt32)(100 - aSrcOpacity*100);
            Do32Blend(level,numlines,numbytes,s1,d1,slinespan,dlinespan,nsHighQual,aSaveBlendArea);
            result = NS_OK;
          }
          else
            result = NS_ERROR_FAILURE;
          break;

        case 24:
          if (mask)
          {
            Do24BlendWithMask(numlines,numbytes,s1,d1,m1,slinespan,dlinespan,mlinespan,nsHighQual,aSaveBlendArea);
            result = NS_OK;
          }
          else
          {
            level = (PRInt32)(100 - aSrcOpacity*100);
            Do24Blend(level,numlines,numbytes,s1,d1,slinespan,dlinespan,nsHighQual,aSaveBlendArea);
            result = NS_OK;
          }
          break;

        case 16:
          if (!mask)
          {
            level = (PRInt32)(100 - aSrcOpacity*100);
            Do16Blend(level,numlines,numbytes,s1,d1,slinespan,dlinespan,nsHighQual,aSaveBlendArea);
            result = NS_OK;
          }
          else
            result = NS_ERROR_FAILURE;
          break;

        case 8:
          if (mask)
          {
            Do8BlendWithMask(numlines,numbytes,s1,d1,m1,slinespan,dlinespan,mlinespan,nsHighQual,aSaveBlendArea);
            result = NS_OK;
          }
          else
          {
            level = (PRInt32)(100 - aSrcOpacity*100);
            Do8Blend(level,numlines,numbytes,s1,d1,slinespan,dlinespan,colormap,nsHighQual,aSaveBlendArea);
            result = NS_OK;
          }
          break;
      }

      if (PR_FALSE == dstissurf)
      {
        // put the new bits in
        dstdc = ((nsDrawingSurfaceWin *)aDst)->mDC;
        dstbits = ::CreateDIBitmap(dstdc, mDstbinfo, CBM_INIT, mDstBytes, (LPBITMAPINFO)mDstbinfo, DIB_RGB_COLORS);
        tb1 = (HBITMAP)::SelectObject(dstdc, dstbits);
        ::DeleteObject(tb1);
      }
    }
    else
        result == NS_ERROR_FAILURE;
  }

#ifdef NGLAYOUT_DDRAW
  if (PR_TRUE == srcissurf)
    mSrcDS->mSurface->Unlock(mSrcSurf.lpSurface);

  if (PR_TRUE == dstissurf)
    mDstDS->mSurface->Unlock(mDstSurf.lpSurface);
#endif

  return result;
}

//------------------------------------------------------------

PRBool
nsBlenderWin::RestoreImage(nsDrawingSurface aDst)
{
PRBool    result = PR_FALSE;
PRInt32   y,x;
PRUint8   *saveptr,*savebyteptr;
PRUint8   *orgptr,*orgbyteptr;
HDC       dstdc;
HBITMAP   dstbits, tb1;

  //XXX this is busted with directdraw... MMP

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
    dstdc = ((nsDrawingSurfaceWin *)aDst)->mDC;
    dstbits = ::CreateDIBitmap(dstdc, mDstbinfo, CBM_INIT, mDstBytes, (LPBITMAPINFO)mDstbinfo, DIB_RGB_COLORS);
    tb1 = (HBITMAP)::SelectObject(dstdc,dstbits);
    ::DeleteObject(tb1);
  }

  return(result);
}

#ifdef NGLAYOUT_DDRAW

PRBool nsBlenderWin :: LockSurface(IDirectDrawSurface *aSurface, DDSURFACEDESC *aDesc, BITMAP *aBitmap, RECT *aRect, DWORD aLockFlags)
{
  if (nsnull != aSurface)
  {
    aDesc->dwSize = sizeof(DDSURFACEDESC);

    if (DD_OK == aSurface->Lock(aRect, aDesc, DDLOCK_WAIT | DDLOCK_SURFACEMEMORYPTR | aLockFlags, NULL))
    {
      if ((aDesc->ddpfPixelFormat.dwFlags &
          (DDPF_ALPHA | DDPF_PALETTEINDEXED1 |
          DDPF_PALETTEINDEXED2 | DDPF_PALETTEINDEXED4 |
          DDPF_PALETTEINDEXEDTO8 | DDPF_YUV | DDPF_ZBUFFER)) ||
          (aDesc->ddpfPixelFormat.dwRGBBitCount < 8))
      {
        //this is a surface that we can't, or don't want to handle.

        aSurface->Unlock(aDesc->lpSurface);
        return PR_FALSE;
      }
      
      aBitmap->bmType = 0;
      aBitmap->bmWidth = aDesc->dwWidth;
      aBitmap->bmHeight = aDesc->dwHeight;
      aBitmap->bmWidthBytes = aDesc->lPitch;
      aBitmap->bmPlanes = 1;
      aBitmap->bmBitsPixel = (PRUint16)aDesc->ddpfPixelFormat.dwRGBBitCount;
      aBitmap->bmBits = aDesc->lpSurface;

      return PR_TRUE;
    }
    else
      return PR_FALSE;
  }
  else
    return PR_FALSE;
}

#endif

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
    //arect.SetRect(0,0,aDestInfo->bmWidth,aDestInfo->bmHeight);
    //srect.SetRect(aMaskUL->x,aMaskUL->y,aWidth,aHeight);
    //arect.IntersectRect(arect,srect);

    arect.SetRect(0, 0,aDestInfo->bmWidth, aDestInfo->bmHeight);
  }

  srect.SetRect(aSrcUL->x, aSrcUL->y, aSrcInfo->bmWidth, aSrcInfo->bmHeight);
  drect = arect;

  if (irect.IntersectRect(srect, drect))
  {
    // calculate destination information
    *aDLSpan = mDRowBytes;
    *aNumbytes = CalcBytesSpan(irect.width,aDestInfo->bmBitsPixel);
    *aNumlines = irect.height;
    startx = irect.x;
    starty = aDestInfo->bmHeight - (irect.y + irect.height);
    *aDImage = ((PRUint8*)aDestInfo->bmBits) + (starty * (*aDLSpan)) + ((aDestInfo->bmBitsPixel >> 3) * startx);

    // get the intersection relative to the source rectangle
    srect.SetRect(0, 0, aSrcInfo->bmWidth, aSrcInfo->bmHeight);
    drect = irect;
    drect.MoveBy(-aSrcUL->x, -aSrcUL->y);

    drect.IntersectRect(drect,srect);
    //*aSLSpan = aSrcInfo->bmWidthBytes;
    *aSLSpan = mSRowBytes;
    startx = drect.x;
    starty = aSrcInfo->bmHeight - (drect.y + drect.height);
    *aSImage = ((PRUint8*)aSrcInfo->bmBits) + (starty * (*aSLSpan)) + ((aDestInfo->bmBitsPixel >> 3) * startx);
         
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
nsBlenderWin :: CalcBytesSpan(PRUint32 aWidth, PRUint32 aBitsPixel)
{
  PRInt32 spanbytes;

  spanbytes = (aWidth * aBitsPixel) >> 5;

	if ((aWidth * aBitsPixel) & 0x1F) 
		spanbytes++;

	spanbytes <<= 2;

  return spanbytes;
}

//-----------------------------------------------------------

nsresult 
nsBlenderWin :: BuildDIB(LPBITMAPINFOHEADER  *aBHead,unsigned char **aBits,PRInt32 aWidth, PRInt32 aHeight, PRInt32 aDepth)
{
  PRInt32 palsize, imagesize, spanbytes, allocsize;
  PRUint8 *colortable;
  DWORD   bicomp, masks[3];

	switch (aDepth) 
  {
		case 8:
			palsize = 256;
			allocsize = 256;
      bicomp = BI_RGB;
      break;

    case 16:
      palsize = 0;
			allocsize = 3;
      bicomp = BI_BITFIELDS;
      masks[0] = 0xf800;
      masks[1] = 0x07e0;
      masks[2] = 0x001f;
      break;

		case 24:
      palsize = 0;
			allocsize = 0;
      bicomp = BI_RGB;
      break;

		case 32:
      palsize = 0;
			allocsize = 3;
      bicomp = BI_BITFIELDS;
      masks[0] = 0xff0000;
      masks[1] = 0x00ff00;
      masks[2] = 0x0000ff;
      break;

		default:
			palsize = -1;
      break;
  }

  if (palsize >= 0)
  {
    spanbytes = CalcBytesSpan(aWidth, aDepth);
    imagesize = spanbytes * aHeight;

	  (*aBHead) = (LPBITMAPINFOHEADER) new char[sizeof(BITMAPINFOHEADER) + (sizeof(RGBQUAD) * allocsize)];
    (*aBHead)->biSize = sizeof(BITMAPINFOHEADER);
	  (*aBHead)->biWidth = aWidth;
	  (*aBHead)->biHeight = aHeight;
	  (*aBHead)->biPlanes = 1;
	  (*aBHead)->biBitCount = (unsigned short)aDepth;
	  (*aBHead)->biCompression = bicomp;
	  (*aBHead)->biSizeImage = imagesize;
	  (*aBHead)->biXPelsPerMeter = 0;
	  (*aBHead)->biYPelsPerMeter = 0;
	  (*aBHead)->biClrUsed = palsize;
	  (*aBHead)->biClrImportant = palsize;

    // set the color table in the info header
	  colortable = (PRUint8 *)(*aBHead) + sizeof(BITMAPINFOHEADER);

    if ((aDepth == 16) || (aDepth == 32))
      nsCRT::memcpy(colortable, masks, sizeof(DWORD) * allocsize);
    else
	    nsCRT::zero(colortable, sizeof(RGBQUAD) * palsize);

    *aBits = new unsigned char[imagesize];

    return NS_OK;
  }
  else
    return NS_ERROR_FAILURE;
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

// This routine can not be fast enough (and it's the same as the
// 24 bit routine, so maybe it can be killed...)
void
nsBlenderWin::Do32Blend(PRUint8 aBlendVal,PRInt32 aNumlines,PRInt32 aNumbytes,PRUint8 *aSImage,PRUint8 *aDImage,PRInt32 aSLSpan,PRInt32 aDLSpan,nsBlendQuality aBlendQuality,PRBool aSaveBlendArea)
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

  if (aSaveBlendArea)
  {
    for (y = 0; y < aNumlines; y++)
    {
      s2 = s1;
      d2 = d1;
      sv2 = saveptr;

      for (x = 0; x < aNumbytes; x++)
      {
        *sv2 = *d2;

        temp1 = (((*d2) * val1) + ((*s2) * val2)) >> 8;

        if (temp1 > 255)
          temp1 = 255;

        *d2 = (PRUint8)temp1; 

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
    for (y = 0; y < aNumlines; y++)
    {
      s2 = s1;
      d2 = d1;

      for (x = 0; x < aNumbytes; x++)
      {
        temp1 = (((*d2) * val1) + ((*s2) * val2)) >> 8;

        if (temp1 > 255)
          temp1 = 255;

        *d2 = (PRUint8)temp1;

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
        *d2 = (PRUint8)temp1; 

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
        *d2 = (PRUint8)temp1; 

        d2++;
        s2++;
        }

      s1 += aSLSpan;
      d1 += aDLSpan;
      }
    }

}

//------------------------------------------------------------

#define RED16(x)    (((x) & 0xf800) >> 8)
#define GREEN16(x)  (((x) & 0x07e0) >> 3)
#define BLUE16(x)   (((x) & 0x001f) << 3)

// This routine can not be fast enough
void
nsBlenderWin::Do16Blend(PRUint8 aBlendVal,PRInt32 aNumlines,PRInt32 aNumbytes,PRUint8 *aSImage,PRUint8 *aDImage,PRInt32 aSLSpan,PRInt32 aDLSpan,nsBlendQuality aBlendQuality,PRBool aSaveBlendArea)
{
PRUint16    *d1,*d2,*s1,*s2;
PRUint32    val1,val2,red,green,blue,stemp,dtemp;
PRInt32     x,y,numlines,xinc,yinc;
PRUint16    *saveptr,*sv2;
PRInt16     dspan,sspan,span,savesp;

  // since we are using 16 bit pointers, the num bytes need to be cut by 2
  saveptr = (PRUint16*)mSaveBytes;
  aBlendVal = (aBlendVal * 255) / 100;
  val2 = aBlendVal;
  val1 = 255-val2;

  // now go thru the image and blend (remember, its bottom upwards)
  s1 = (PRUint16*)aSImage;
  d1 = (PRUint16*)aDImage;
  dspan = aDLSpan >> 1;
  sspan = aSLSpan >> 1;
  span = aNumbytes >> 1;
  savesp = mSaveLS >> 1;
  numlines = aNumlines;
  xinc = 1;
  yinc = 1;

  if (aSaveBlendArea)
  {
    for (y = 0; y < aNumlines; y++)
    {
      s2 = s1;
      d2 = d1;
      sv2 = saveptr;

      for (x = 0; x < span; x++)
      {
        stemp = *s2;
        dtemp = *d2;

        red = (RED16(dtemp) * val1 + RED16(stemp) * val2) >> 8;

        if (red > 255)
          red = 255;

        green = (GREEN16(dtemp) * val1 + GREEN16(stemp) * val2) >> 8;

        if (green > 255)
          green = 255;

        blue = (BLUE16(dtemp) * val1 + BLUE16(stemp) * val2) >> 8;

        if (blue > 255)
          blue = 255;

        *sv2 = *d2;
        *d2 = (PRUint16)((red & 0xf8) << 8) | ((green & 0xfc) << 3) | ((blue & 0xf8) >> 3);

        sv2++;
        d2++;
        s2++;
      }

      s1 += sspan;
      d1 += dspan;
      saveptr += savesp;
    }
  }
  else
  {
    for (y = 0; y < aNumlines; y++)
    {
      s2 = s1;
      d2 = d1;

      for (x = 0; x < span; x++)
      {
        stemp = *s2;
        dtemp = *d2;

        red = (RED16(dtemp) * val1 + RED16(stemp) * val2) >> 8;

        if (red > 255)
          red = 255;

        green = (GREEN16(dtemp) * val1 + GREEN16(stemp) * val2) >> 8;

        if (green > 255)
          green = 255;

        blue = (BLUE16(dtemp) * val1 + BLUE16(stemp) * val2) >> 8;

        if (blue > 255)
          blue = 255;

        *d2 = (PRUint16)((red & 0xf8) << 8) | ((green & 0xfc) << 3) | ((blue & 0xf8) >> 3);

        d2++;
        s2++;
      }

      s1 += sspan;
      d1 += dspan;
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

