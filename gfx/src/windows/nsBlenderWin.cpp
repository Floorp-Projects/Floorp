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
#include "il_util.h"


#ifdef NGLAYOUT_DDRAW
#include "ddraw.h"
#endif

//static NS_DEFINE_IID(kIBlenderIID, NS_IBLENDER_IID);

/** --------------------------------------------------------------------------
 * Construct and set the initial values for this windows specific blender
 * @update dc - 10/29/98
 */
nsBlenderWin :: nsBlenderWin()
{
  mSrcBytes = nsnull;
  mSecondSrcBytes = nsnull;
  mDstBytes = nsnull;
  mSrcbinfo = nsnull;
  mDstbinfo = nsnull;
  mSecondSrcbinfo = nsnull;
  mSRowBytes = 0;
  mDRowBytes = 0;
}

/** --------------------------------------------------------------------------
 * Release and cleanup all the windows specific information for this blender
 * @update dc - 10/29/98
 */
nsBlenderWin :: ~nsBlenderWin()
{
  // get rid of the DIB's
  if (nsnull != mSrcbinfo)
    DeleteDIB(&mSrcbinfo, &mSrcBytes);

  if (nsnull != mDstbinfo)
    DeleteDIB(&mDstbinfo, &mDstBytes);

  if (nsnull != mSecondSrcbinfo)
    DeleteDIB(&mSecondSrcbinfo, &mSecondSrcBytes);
}

/** --------------------------------------------------------------------------
 * Set  all the windows specific data for a blender to some initial values
 * @update dc 11/4/98
 * @param - aTheDevCon is the device context we will use to get information from for the blend
 */
NS_IMETHODIMP
nsBlenderWin::Init(nsIDeviceContext *aTheDevCon)
{
  mTheDeviceCon = aTheDevCon;

  return NS_OK;
}

/** --------------------------------------------------------------------------
 * Run the blend using the passed in drawing surfaces
 * @update dc - 10/29/98
 * @param aSX -- left location for the blend
 * @param aSY -- top location for the blend
 * @param aWidth -- width of the blend
 * @param aHeight -- height of the blend
 * @param aDst -- Destination drawing surface for the blend
 * @param aDX -- left location for the destination of the blend
 * @param aDY -- top location for the destination of the blend
 * @param aSrcOpacity -- the percentage for the blend
 * @result NS_OK if the blend worked.
 */
NS_IMETHODIMP
nsBlenderWin::Blend(PRInt32 aSX, PRInt32 aSY, PRInt32 aWidth, PRInt32 aHeight,nsDrawingSurface aSrc,
                    nsDrawingSurface aDst, PRInt32 aDX, PRInt32 aDY, float aSrcOpacity,
                    nsDrawingSurface aSecondSrc, nscolor aSrcBackColor, nscolor aSecondSrcBackColor)
{
nsresult      result = NS_ERROR_FAILURE;
HBITMAP       dstbits, tb1;
nsPoint       srcloc, maskloc;
PRInt32       dlinespan, slinespan, mlinespan, numbytes, numlines, level;
PRUint8       *s1, *d1, *m1, *mask = NULL, *ssl = NULL;
IL_ColorSpace *thespace=nsnull;
HDC           srcdc, dstdc, secondsrcdc;
PRBool        srcissurf = PR_FALSE;
PRBool        secondsrcissurf = PR_FALSE;
PRBool        dstissurf = PR_FALSE;
nsPixelFormat pixformat;

// This is a temporary solution, nsDrawingSurface is a void*, but on windows it is really a
// nsDrawingSurfaceWin, which is an XPCom object.  I am going to cast it here just temporarily
// until I fix all the platforms to use a XPComed version of the nsDrawingSurface

  nsDrawingSurfaceWin   *SrcWinSurf, *DstWinSurf, *SecondSrcWinSurf;

  SrcWinSurf = (nsDrawingSurfaceWin*)aSrc;
  DstWinSurf = (nsDrawingSurfaceWin*)aDst;
  SecondSrcWinSurf = (nsDrawingSurfaceWin*)aSecondSrc;

  // source

#ifdef NGLAYOUT_DDRAW
  RECT  srect;

  srect.left = aSX;
  srect.top = aSY;
  srect.right = aSX + aWidth;
  srect.bottom = aSY + aHeight;

  if (PR_TRUE == LockSurface(SrcWinSurf->mSurface, &mSrcSurf, &mSrcInfo, &srect, DDLOCK_READONLY, &pixformat)){
    srcissurf = PR_TRUE;
    mSRowBytes = mSrcInfo.bmWidthBytes;
  }else
#endif
  {
    if (nsnull == mSrcbinfo){
      HBITMAP srcbits;
      srcdc = SrcWinSurf->mDC;

      if (nsnull == SrcWinSurf->mSelectedBitmap){
        HBITMAP hbits = ::CreateCompatibleBitmap(srcdc, 2, 2);
        srcbits = (HBITMAP)::SelectObject(srcdc, hbits);
        ::GetObject(srcbits, sizeof(BITMAP), &mSrcInfo);
        ::SelectObject(srcdc, srcbits);
        ::DeleteObject(hbits);
      }else{
        ::GetObject(SrcWinSurf->mSelectedBitmap, sizeof(BITMAP), &mSrcInfo);
        srcbits = SrcWinSurf->mSelectedBitmap;
      }

      BuildDIB(&mSrcbinfo, &mSrcBytes, mSrcInfo.bmWidth, mSrcInfo.bmHeight, mSrcInfo.bmBitsPixel, &pixformat);
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

  if (PR_TRUE == LockSurface(DstWinSurf->mSurface, &mDstSurf, &mDstInfo, &drect, 0, nsnull)){
    dstissurf = PR_TRUE;
    mDRowBytes = mDstInfo.bmWidthBytes;
  }
  else
#endif
  {
    if (nsnull == mDstbinfo){
      HBITMAP dstbits;
      dstdc = DstWinSurf->mDC;

      if (nsnull == DstWinSurf->mSelectedBitmap){
        HBITMAP hbits = CreateCompatibleBitmap(dstdc, 2, 2);
        dstbits = (HBITMAP)::SelectObject(dstdc, hbits);
        ::GetObject(dstbits, sizeof(BITMAP), &mDstInfo);
        ::SelectObject(dstdc, dstbits);
        ::DeleteObject(hbits);
      }else{
        ::GetObject(DstWinSurf->mSelectedBitmap, sizeof(BITMAP), &mDstInfo);
        dstbits = DstWinSurf->mSelectedBitmap;
      }

      BuildDIB(&mDstbinfo, &mDstBytes, mDstInfo.bmWidth, mDstInfo.bmHeight, mDstInfo.bmBitsPixel, nsnull);
      numbytes = ::GetDIBits(dstdc, dstbits, 0, mDstInfo.bmHeight, mDstBytes, (LPBITMAPINFO)mDstbinfo, DIB_RGB_COLORS);
  
      mDRowBytes = CalcBytesSpan(mDstInfo.bmWidth, mDstInfo.bmBitsPixel);
    }

    mDstInfo.bmBits = mDstBytes;
  }

  // optional second source

  if (nsnull != SecondSrcWinSurf)
  {
#ifdef NGLAYOUT_DDRAW
    RECT  srect;

    srect.left = aSX;
    srect.top = aSY;
    srect.right = aSX + aWidth;
    srect.bottom = aSY + aHeight;

    if (PR_TRUE == LockSurface(SecondSrcWinSurf->mSurface, &mSecondSrcSurf, &mSecondSrcInfo, &srect, DDLOCK_READONLY, nsnull)){
      secondsrcissurf = PR_TRUE;
    }else
#endif
    {
      if (nsnull == mSecondSrcbinfo){
        HBITMAP srcbits;
        secondsrcdc = SecondSrcWinSurf->mDC;

        if (nsnull == SecondSrcWinSurf->mSelectedBitmap){
          HBITMAP hbits = ::CreateCompatibleBitmap(secondsrcdc, 2, 2);
          srcbits = (HBITMAP)::SelectObject(secondsrcdc, hbits);
          ::GetObject(srcbits, sizeof(BITMAP), &mSecondSrcInfo);
          ::SelectObject(secondsrcdc, srcbits);
          ::DeleteObject(hbits);
        }else{
          ::GetObject(SecondSrcWinSurf->mSelectedBitmap, sizeof(BITMAP), &mSecondSrcInfo);
          srcbits = SecondSrcWinSurf->mSelectedBitmap;
        }

        BuildDIB(&mSecondSrcbinfo, &mSecondSrcBytes, mSecondSrcInfo.bmWidth, mSecondSrcInfo.bmHeight, mSecondSrcInfo.bmBitsPixel, nsnull);
        numbytes = ::GetDIBits(secondsrcdc, srcbits, 0, mSecondSrcInfo.bmHeight, mSecondSrcBytes, (LPBITMAPINFO)mSecondSrcbinfo, DIB_RGB_COLORS);
      }

      mSecondSrcInfo.bmBits = mSecondSrcBytes;
    }
  }

  // calculate the metrics, no mask right now
  srcloc.x = aSX;
  srcloc.y = aSY;
  maskloc.x = 0;
  maskloc.y = 0;

  if (CalcAlphaMetrics(&mSrcInfo, &mDstInfo,
                       ((nsnull != mSecondSrcbinfo) || (PR_TRUE == secondsrcissurf)) ? &mSecondSrcInfo : nsnull,
                       &srcloc, NULL, &maskloc, aWidth, aHeight, &numlines, &numbytes,
                       &s1, &d1, &ssl, &m1, &slinespan, &dlinespan, &mlinespan)){
    if (mSrcInfo.bmBitsPixel == mDstInfo.bmBitsPixel){
      // now do the blend
      switch (mSrcInfo.bmBitsPixel){
        case 32:
          if (!mask){
            level = (PRInt32)(aSrcOpacity*100);
            Do32Blend(level,numlines,numbytes,s1,d1,ssl,slinespan,dlinespan,nsHighQual,aSrcBackColor,aSecondSrcBackColor,pixformat);
            result = NS_OK;
          }else
            result = NS_ERROR_FAILURE;
          break;

        case 24:
          if (mask){
            Do24BlendWithMask(numlines,numbytes,s1,d1,m1,slinespan,dlinespan,mlinespan,nsHighQual);
            result = NS_OK;
          }else{
            level = (PRInt32)(aSrcOpacity*100);
            Do24Blend(level,numlines,numbytes,s1,d1,ssl,slinespan,dlinespan,nsHighQual,aSrcBackColor,aSecondSrcBackColor,pixformat);
            result = NS_OK;
          }
          break;

        case 16:
          if (!mask){
            level = (PRInt32)(aSrcOpacity*100);
            Do16Blend(level,numlines,numbytes,s1,d1,ssl,slinespan,dlinespan,nsHighQual,aSrcBackColor,aSecondSrcBackColor,pixformat);
            result = NS_OK;
          }
          else
            result = NS_ERROR_FAILURE;
          break;

        case 8:
          if (mask){
            Do8BlendWithMask(numlines,numbytes,s1,d1,m1,slinespan,dlinespan,mlinespan,nsHighQual);
            result = NS_OK;
          }else{
            if( mTheDeviceCon->GetILColorSpace(thespace) == NS_OK){
              level = (PRInt32)(aSrcOpacity*100);
              Do8Blend(level,numlines,numbytes,s1,d1,ssl,slinespan,dlinespan,thespace,nsHighQual,aSrcBackColor,aSecondSrcBackColor);
              result = NS_OK;
              IL_ReleaseColorSpace(thespace);
            }
          }
          break;
      }

      if (PR_FALSE == dstissurf){
        // put the new bits in
        dstdc = ((nsDrawingSurfaceWin *)DstWinSurf)->mDC;
        dstbits = ::CreateDIBitmap(dstdc, mDstbinfo, CBM_INIT, mDstBytes, (LPBITMAPINFO)mDstbinfo, DIB_RGB_COLORS);
        tb1 = (HBITMAP)::SelectObject(dstdc, dstbits);
        ::DeleteObject(tb1);
      }
    } else
      result = NS_ERROR_FAILURE;
  }

#ifdef NGLAYOUT_DDRAW
  if (PR_TRUE == srcissurf)
    SrcWinSurf->mSurface->Unlock(mSrcSurf.lpSurface);

  if (PR_TRUE == dstissurf)
    DstWinSurf->mSurface->Unlock(mDstSurf.lpSurface);

  if (PR_TRUE == secondsrcissurf)
    SecondSrcWinSurf->mSurface->Unlock(mSecondSrcSurf.lpSurface);
#endif

  // get rid of the DIB's
  if (nsnull != mSrcbinfo)
    DeleteDIB(&mSrcbinfo, &mSrcBytes);

  if (nsnull != mDstbinfo)
    DeleteDIB(&mDstbinfo, &mDstBytes);

  if (nsnull != mSecondSrcbinfo)
    DeleteDIB(&mSecondSrcbinfo, &mSecondSrcBytes);

  return result;
}

#ifdef NGLAYOUT_DDRAW

/** --------------------------------------------------------------------------
 * Lock a surface down for Direct draw
 * @update mp - 10/01/98
 * @param IDirectDrawSurface -- 
 * @param DDSURFACEDESC -- 
 * @param BITMAP -- 
 * @param RECT -- 
 * @param DWORD -- 
 * @result PR_TRUE lock was succesful
 */
PRBool nsBlenderWin :: LockSurface(IDirectDrawSurface *aSurface, DDSURFACEDESC *aDesc, BITMAP *aBitmap, RECT *aRect, DWORD aLockFlags, nsPixelFormat *aPixFormat)
{
  if (nsnull != aSurface){
    aDesc->dwSize = sizeof(DDSURFACEDESC);

    if (DD_OK == aSurface->Lock(aRect, aDesc, DDLOCK_WAIT | DDLOCK_SURFACEMEMORYPTR | aLockFlags, NULL)){
      if ((aDesc->ddpfPixelFormat.dwFlags &
          (DDPF_ALPHA | DDPF_PALETTEINDEXED1 |
          DDPF_PALETTEINDEXED2 | DDPF_PALETTEINDEXED4 |
          DDPF_PALETTEINDEXEDTO8 | DDPF_YUV | DDPF_ZBUFFER)) ||
          (aDesc->ddpfPixelFormat.dwRGBBitCount < 8)){
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

      if ((nsnull != aPixFormat) && (aBitmap->bmBitsPixel > 8)) {
        DWORD btemp, shiftcnt;

        btemp = aDesc->ddpfPixelFormat.dwRBitMask;

        aPixFormat->mRedMask = btemp;

        shiftcnt = 32;

        if (!(btemp & 0xffff)) {
          aPixFormat->mRedShift = 16;
          btemp >>= 16;
          shiftcnt = 16;
        }
        else if (!(btemp & 0xff)) {
          aPixFormat->mRedShift = 8;
          btemp >>= 8;
          shiftcnt = 24;
        }
        else {
          aPixFormat->mRedShift = 0;
          shiftcnt = 32;
        }

        while (!(btemp & 1) && shiftcnt--) {
          btemp >>= 1;
          aPixFormat->mRedShift++;
        }

        aPixFormat->mRedZeroMask = btemp;
        aPixFormat->mRedCount = 0;

        while ((btemp & 1) && shiftcnt--) {
          btemp >>= 1;
          aPixFormat->mRedCount++;
        }

        btemp = aDesc->ddpfPixelFormat.dwGBitMask;

        aPixFormat->mGreenMask = btemp;

        shiftcnt = 32;

        if (!(btemp & 0xffff)) {
          aPixFormat->mGreenShift = 16;
          btemp >>= 16;
          shiftcnt = 16;
        }
        else if (!(btemp & 0xff)) {
          aPixFormat->mGreenShift = 8;
          btemp >>= 8;
          shiftcnt = 24;
        }
        else {
          aPixFormat->mGreenShift = 0;
          shiftcnt = 32;
        }

        while (!(btemp & 1) && shiftcnt--) {
          btemp >>= 1;
          aPixFormat->mGreenShift++;
        }

        aPixFormat->mGreenZeroMask = btemp;
        aPixFormat->mGreenCount = 0;

        while ((btemp & 1) && shiftcnt--) {
          btemp >>= 1;
          aPixFormat->mGreenCount++;
        }

        btemp = aDesc->ddpfPixelFormat.dwBBitMask;

        aPixFormat->mBlueMask = btemp;

        shiftcnt = 32;

        if (!(btemp & 0xffff)) {
          aPixFormat->mBlueShift = 16;
          btemp >>= 16;
          shiftcnt = 16;
        }
        else if (!(btemp & 0xff)) {
          aPixFormat->mBlueShift = 8;
          btemp >>= 8;
          shiftcnt = 24;
        }
        else {
          aPixFormat->mBlueShift = 0;
          shiftcnt = 32;
        }

        while (!(btemp & 1) && shiftcnt--) {
          btemp >>= 1;
          aPixFormat->mBlueShift++;
        }

        aPixFormat->mBlueZeroMask = btemp;
        aPixFormat->mBlueCount = 0;

        while ((btemp & 1) && shiftcnt--) {
          btemp >>= 1;
          aPixFormat->mBlueCount++;
        }

        aPixFormat->mAlphaCount = aDesc->ddpfPixelFormat.dwAlphaBitDepth;

        if (aPixFormat->mAlphaCount > 0) {
          btemp = aDesc->ddpfPixelFormat.dwRGBAlphaBitMask;

          aPixFormat->mAlphaMask = btemp;

          shiftcnt = 32;

          if (!(btemp & 0xffff)) {
            aPixFormat->mAlphaShift = 16;
            btemp >>= 16;
            shiftcnt = 16;
          }
          else if (!(btemp & 0xff)) {
            aPixFormat->mAlphaShift = 8;
            btemp >>= 8;
            shiftcnt = 24;
          }
          else {
            aPixFormat->mAlphaShift = 0;
            shiftcnt = 32;
          }

          while (!(btemp & 1) && shiftcnt--) {
            btemp >>= 1;
            aPixFormat->mAlphaShift++;
          }

          aPixFormat->mAlphaZeroMask = btemp;
        }
        else {
          aPixFormat->mAlphaMask = 0;
          aPixFormat->mAlphaShift = 0;
          aPixFormat->mAlphaZeroMask = 0;
        }
      }

      return PR_TRUE;
    }else
      return PR_FALSE;
  }
  else
    return PR_FALSE;
}

#endif

/** --------------------------------------------------------------------------
 * Calculate the metrics for the alpha layer before the blend
 * @update mp - 10/01/98
 * @param aSrcInfo -- a pointer to a source bitmap
 * @param aDestInfo -- a pointer to the destination bitmap
 * @param aSrcUL -- upperleft for the source blend
 * @param aMaskInfo -- a pointer to the mask bitmap
 * @param aMaskUL -- upperleft for the mask bitmap
 * @param aWidth -- width of the blend
 * @param aHeight -- heigth of the blend
 * @param aNumLines -- a pointer to number of lines to do for the blend
 * @param aNumbytes -- a pointer to the number of bytes per line for the blend
 * @param aSImage -- a pointer to a the bit pointer for the source
 * @param aDImage -- a pointer to a the bit pointer for the destination 
 * @param aMImage -- a pointer to a the bit pointer for the mask 
 * @param aSLSpan -- number of bytes per span for the source
 * @param aDLSpan -- number of bytes per span for the destination
 * @param aMLSpan -- number of bytes per span for the mask
 * @result PR_TRUE if calculation was succesful
 */
PRBool 
nsBlenderWin::CalcAlphaMetrics(BITMAP *aSrcInfo,BITMAP *aDestInfo, BITMAP *aSecondSrcInfo,
                              nsPoint *aSrcUL,
                              BITMAP  *aMaskInfo,nsPoint *aMaskUL,
                              PRInt32 aWidth,PRInt32 aHeight,
                              PRInt32 *aNumlines,
                              PRInt32 *aNumbytes,PRUint8 **aSImage,PRUint8 **aDImage,
                              PRUint8 **aSecondSImage,
                              PRUint8 **aMImage,PRInt32 *aSLSpan,PRInt32 *aDLSpan,PRInt32 *aMLSpan)
{
PRBool    doalpha = PR_FALSE;
nsRect    arect,srect,drect,irect;
PRInt32   startx,starty;

  if(aMaskInfo){
    arect.SetRect(0,0,aDestInfo->bmWidth,aDestInfo->bmHeight);
    srect.SetRect(aMaskUL->x,aMaskUL->y,aMaskInfo->bmWidth,aSrcInfo->bmHeight);
    arect.IntersectRect(arect,srect);
  }else{
    //arect.SetRect(0,0,aDestInfo->bmWidth,aDestInfo->bmHeight);
    //srect.SetRect(aMaskUL->x,aMaskUL->y,aWidth,aHeight);
    //arect.IntersectRect(arect,srect);

    arect.SetRect(0, 0,aDestInfo->bmWidth, aDestInfo->bmHeight);
  }

  srect.SetRect(aSrcUL->x, aSrcUL->y, aSrcInfo->bmWidth, aSrcInfo->bmHeight);
  drect = arect;

  if (irect.IntersectRect(srect, drect)){
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
    *aSImage = ((PRUint8*)aSrcInfo->bmBits) + (starty * (*aSLSpan)) + ((aSrcInfo->bmBitsPixel >> 3) * startx);

    if (nsnull != aSecondSrcInfo)
      *aSecondSImage = ((PRUint8*)aSecondSrcInfo->bmBits) + (starty * (*aSLSpan)) + ((aSrcInfo->bmBitsPixel >> 3) * startx);

    doalpha = PR_TRUE;

    if(aMaskInfo){
      *aMLSpan = aMaskInfo->bmWidthBytes;
      *aMImage = (PRUint8*)aMaskInfo->bmBits;
    }else{
      aMLSpan = 0;
      *aMImage = nsnull;
    }
  }

  return doalpha;
}

/** --------------------------------------------------------------------------
 * Build the device independent bitmap
 * @update mp - 10/01/98
 * @param aBHead -- a pointer DIB header we are filling in
 * @param aBits -- a pointer to the bits for this DIB
 * @param aWidth -- the width of the DIB
 * @param aHeight -- the height of the DIB
 * @param aDepth -- depth of the DIB
 * @result NS_OK if the build was succesful
 */
nsresult 
nsBlenderWin :: BuildDIB(LPBITMAPINFOHEADER  *aBHead,unsigned char **aBits,PRInt32 aWidth, PRInt32 aHeight, PRInt32 aDepth, nsPixelFormat *aPixFormat)
{
  PRInt32 palsize, imagesize, spanbytes, allocsize;
  PRUint8 *colortable;
  DWORD   bicomp, masks[3];

	switch (aDepth) {
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

      if (nsnull != aPixFormat) {
        aPixFormat->mRedZeroMask = 0x1f;
        aPixFormat->mGreenZeroMask = 0x3f;
        aPixFormat->mBlueZeroMask = 0x1f;
        aPixFormat->mAlphaZeroMask = 0;
        aPixFormat->mRedMask = masks[0];
        aPixFormat->mGreenMask = masks[1];
        aPixFormat->mBlueMask = masks[2];
        aPixFormat->mAlphaMask = 0;
        aPixFormat->mRedCount = 5;
        aPixFormat->mGreenCount = 6;
        aPixFormat->mBlueCount = 5;
        aPixFormat->mAlphaCount = 0;
        aPixFormat->mRedShift = 11;
        aPixFormat->mGreenShift = 5;
        aPixFormat->mBlueShift = 0;
        aPixFormat->mAlphaShift = 0;
      }

      break;

		case 24:
      palsize = 0;
			allocsize = 0;
      bicomp = BI_RGB;

      if (nsnull != aPixFormat) {
        aPixFormat->mRedZeroMask = 0xff;
        aPixFormat->mGreenZeroMask = 0xff;
        aPixFormat->mBlueZeroMask = 0xff;
        aPixFormat->mAlphaZeroMask = 0;
        aPixFormat->mRedMask = 0xff;
        aPixFormat->mGreenMask = 0xff00;
        aPixFormat->mBlueMask = 0xff0000;
        aPixFormat->mAlphaMask = 0;
        aPixFormat->mRedCount = 8;
        aPixFormat->mGreenCount = 8;
        aPixFormat->mBlueCount = 8;
        aPixFormat->mAlphaCount = 0;
        aPixFormat->mRedShift = 0;
        aPixFormat->mGreenShift = 8;
        aPixFormat->mBlueShift = 16;
        aPixFormat->mAlphaShift = 0;
      }

      break;

		case 32:
      palsize = 0;
			allocsize = 3;
      bicomp = BI_BITFIELDS;
      masks[0] = 0xff0000;
      masks[1] = 0x00ff00;
      masks[2] = 0x0000ff;

      if (nsnull != aPixFormat) {
        aPixFormat->mRedZeroMask = 0xff;
        aPixFormat->mGreenZeroMask = 0xff;
        aPixFormat->mBlueZeroMask = 0xff;
        aPixFormat->mAlphaZeroMask = 0xff;
        aPixFormat->mRedMask = masks[0];
        aPixFormat->mGreenMask = masks[1];
        aPixFormat->mBlueMask = masks[2];
        aPixFormat->mAlphaMask = 0xff000000;
        aPixFormat->mRedCount = 8;
        aPixFormat->mGreenCount = 8;
        aPixFormat->mBlueCount = 8;
        aPixFormat->mAlphaCount = 8;
        aPixFormat->mRedShift = 16;
        aPixFormat->mGreenShift = 8;
        aPixFormat->mBlueShift = 0;
        aPixFormat->mAlphaShift = 24;
      }

      break;

		default:
			palsize = -1;
      break;
  }

  if (palsize >= 0){
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
  }else
    return NS_ERROR_FAILURE;
}

/** --------------------------------------------------------------------------
 * Delete the device independent bitmap
 * @update mp - 10/01/98
 * @param aBHead -- a pointer DIB header we are filling in
 * @param aBits -- a pointer to the bits for this DIB
 * @result VOID
 */
void
nsBlenderWin::DeleteDIB(LPBITMAPINFOHEADER  *aBHead,unsigned char **aBits)
{
  delete[] *aBHead;
  *aBHead = nsnull;

  delete[] *aBits;
  *aBits = nsnull;
}
