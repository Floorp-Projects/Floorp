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

#include <gtk/gtk.h>
#include "nsBlenderGTK.h"
#include "nsRenderingContextGTK.h"
#include "il_util.h"


//static NS_DEFINE_IID(kIBlenderIID, NS_IBLENDER_IID);

/** --------------------------------------------------------------------------
 * Construct and set the initial values for this windows specific blender
 * @update dc - 10/29/98
 */
nsBlenderGTK :: nsBlenderGTK()
{
  mSrcBytes = nsnull;
  mSecondSrcBytes = nsnull;
  mDestBytes = nsnull;

  mSrcRowBytes = 0;
  mDestRowBytes = 0;
  mSecondSrcRowBytes = 0;

  mSrcSpan = 0;
  mDestSpan = 0;
  mSecondSrcSpan = 0;
}

/** --------------------------------------------------------------------------
 * Release and cleanup all the windows specific information for this blender
 * @update dc - 10/29/98
 */
nsBlenderGTK :: ~nsBlenderGTK()
{
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
nsBlenderGTK::Blend(PRInt32 aSX, PRInt32 aSY, PRInt32 aWidth, PRInt32 aHeight,nsDrawingSurface aSrc,
                    nsDrawingSurface aDst, PRInt32 aDX, PRInt32 aDY, float aSrcOpacity,
                    nsDrawingSurface aSecondSrc, nscolor aSrcBackColor, nscolor aSecondSrcBackColor)
{
nsresult      result = NS_ERROR_FAILURE;
nsPoint       srcloc, maskloc;
nsPixelFormat pixformat;

  nsDrawingSurfaceGTK   *SrcWinSurf, *DstWinSurf, *SecondSrcWinSurf;

  SrcWinSurf = (nsDrawingSurfaceGTK*)aSrc;
  DstWinSurf = (nsDrawingSurfaceGTK*)aDst;
  SecondSrcWinSurf = (nsDrawingSurfaceGTK*)aSecondSrc;

  mSrcBytes = mSecondSrcBytes = mDestBytes = nsnull;

  SrcWinSurf->Lock(aSX, aSY, aWidth, aHeight, (void **)&mSrcBytes, &mSrcRowBytes, &mSrcSpan, NS_LOCK_SURFACE_READ_ONLY);
  DstWinSurf->Lock(aSX, aSY, aWidth, aHeight, (void **)&mDestBytes, &mDestRowBytes, &mDestSpan, 0);

  if (nsnull != SecondSrcWinSurf)
    SecondSrcWinSurf->Lock(aSX, aSY, aWidth, aHeight, (void **)&mSecondSrcBytes, &mSecondSrcRowBytes, &mSecondSrcSpan, NS_LOCK_SURFACE_READ_ONLY);

  srcloc.x = 0;
  srcloc.y = 0;

  maskloc.x = 0;
  maskloc.y = 0;

  SrcWinSurf->GetPixelFormat(&pixformat);

  result = Blend(mSrcBytes, mSrcRowBytes, mSrcSpan,
                 mDestBytes, mDestRowBytes, mDestSpan,
                 mSecondSrcBytes, mSecondSrcRowBytes, mSecondSrcSpan,
                 aHeight, (PRInt32)(aSrcOpacity * 100), pixformat,
                 aSrcBackColor, aSecondSrcBackColor);

  SrcWinSurf->Unlock();
  DstWinSurf->Unlock();

  if (nsnull != SecondSrcWinSurf)
    SecondSrcWinSurf->Unlock();

  return result;
}

NS_IMETHODIMP nsBlenderGTK :: Blend(PRInt32 aSX, PRInt32 aSY, PRInt32 aWidth, PRInt32 aHeight, nsIRenderingContext *aSrc,
                   nsIRenderingContext *aDest, PRInt32 aDX, PRInt32 aDY, float aSrcOpacity,
                   nsIRenderingContext *aSecondSrc, nscolor aSrcBackColor,
                   nscolor aSecondSrcBackColor)
{
  nsresult      result = NS_ERROR_FAILURE;
  nsPoint       srcloc, maskloc;
  nsPixelFormat pixformat;
  nsDrawingSurface srcsurf;

  mSrcBytes = mSecondSrcBytes = mDestBytes = nsnull;

  aSrc->LockDrawingSurface(aSX, aSY, aWidth, aHeight, (void **)&mSrcBytes, &mSrcRowBytes, &mSrcSpan, NS_LOCK_SURFACE_READ_ONLY);
  aDest->LockDrawingSurface(aSX, aSY, aWidth, aHeight, (void **)&mDestBytes, &mDestRowBytes, &mDestSpan, 0);

  if (nsnull != aSecondSrc)
    aSecondSrc->LockDrawingSurface(aSX, aSY, aWidth, aHeight, (void **)&mSecondSrcBytes, &mSecondSrcRowBytes, &mSecondSrcSpan, NS_LOCK_SURFACE_READ_ONLY);

  srcloc.x = 0;
  srcloc.y = 0;

  maskloc.x = 0;
  maskloc.y = 0;

  aSrc->GetDrawingSurface(&srcsurf);
  ((nsIDrawingSurface *)srcsurf)->GetPixelFormat(&pixformat);

  result = Blend(mSrcBytes, mSrcRowBytes, mSrcSpan,
                 mDestBytes, mDestRowBytes, mDestSpan,
                 mSecondSrcBytes, mSecondSrcRowBytes, mSecondSrcSpan,
                 aHeight, (PRInt32)(aSrcOpacity * 100), pixformat,
                 aSrcBackColor, aSecondSrcBackColor);

  aSrc->UnlockDrawingSurface();
  aDest->UnlockDrawingSurface();

  if (nsnull != aSecondSrc)
    aSecondSrc->UnlockDrawingSurface();

  return result;
}

nsresult nsBlenderGTK :: Blend(PRUint8 *aSrcBits, PRInt32 aSrcStride, PRInt32 aSrcBytes,
                               PRUint8 *aDestBits, PRInt32 aDestStride, PRInt32 aDestBytes,
                               PRUint8 *aSecondSrcBits, PRInt32 aSecondSrcStride, PRInt32 aSecondSrcBytes,
                               PRInt32 aLines, PRInt32 aAlpha, nsPixelFormat &aPixFormat,
                               nscolor aSrcBackColor, nscolor aSecondSrcBackColor)
{
  nsresult  result = NS_OK;

//  if (CalcAlphaMetrics(&mSrcInfo, &mDstInfo,
//                       ((nsnull != mSecondSrcbinfo) || (PR_TRUE == secondsrcissurf)) ? &mSecondSrcInfo : nsnull,
//                       &srcloc, NULL, &maskloc, aWidth, aHeight, &numlines, &numbytes,
//                       &s1, &d1, &ssl, &m1, &slinespan, &dlinespan, &mlinespan))
if (1)
  {
//    if (mSrcInfo.bmBitsPixel == mDstInfo.bmBitsPixel)
if (1)
    {
      PRUint32 depth;
      mContext->GetDepth(depth);
      // now do the blend
      switch (depth)
      {
        case 32:
//          if (!mask){
            Do32Blend(aAlpha, aLines, aSrcBytes, aSrcBits, aDestBits,
                      aSecondSrcBits, aSrcStride, aDestStride, nsHighQual,
                      aSrcBackColor, aSecondSrcBackColor, aPixFormat);
            result = NS_OK;
//          }else
//            result = NS_ERROR_FAILURE;
          break;

        case 24:
//          if (mask){
//            Do24BlendWithMask(aHeight, mSrcSpan, mSrcBytes, mDestBytes,
//                              NULL, mSrcRowBytes, mDestRowBytes, 0, nsHighQual);
//            result = NS_OK;
//          }else{
            Do24Blend(aAlpha, aLines, aSrcBytes, aSrcBits, aDestBits,
                      aSecondSrcBits, aSrcStride, aDestStride, nsHighQual,
                      aSrcBackColor, aSecondSrcBackColor, aPixFormat);
//            result = NS_OK;
//          }
          break;

        case 16:
//          if (!mask){
            Do16Blend(aAlpha, aLines, aSrcBytes, aSrcBits, aDestBits,
                      aSecondSrcBits, aSrcStride, aDestStride, nsHighQual,
                      aSrcBackColor, aSecondSrcBackColor, aPixFormat);
//            result = NS_OK;
//          }
//          else
//            result = NS_ERROR_FAILURE;
          break;

        case 8:
        {
          IL_ColorSpace *thespace = nsnull;

//          if (mask){
//            Do8BlendWithMask(aHeight, mSrcSpan, mSrcBytes, mDestBytes,
//                             NULL, mSrcRowBytes, mDestRowBytes, 0, nsHighQual);
//            result = NS_OK;
//          }else{
            if ((result = mContext->GetILColorSpace(thespace)) == NS_OK){
              Do8Blend(aAlpha, aLines, aSrcBytes, aSrcBits, aDestBits,
                       aSecondSrcBits, aSrcStride, aDestStride, thespace,
                       nsHighQual, aSrcBackColor, aSecondSrcBackColor);
              IL_ReleaseColorSpace(thespace);
            }
//          }
          break;
        }
      }
    }
    else
      result = NS_ERROR_FAILURE;
  }

  return result;
}

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
#if 0
PRBool 
nsBlenderGTK::CalcAlphaMetrics(BITMAP *aSrcInfo,BITMAP *aDestInfo, BITMAP *aSecondSrcInfo,
                              nsPoint *aSrcUL,
                              BITMAP  *aMaskInfo,nsPoint *aMaskUL,
                              PRInt32 aWidth,PRInt32 aHeight,
                              PRInt32 *aNumlines,
                              PRInt32 *aNumbytes,PRUint8 **aSImage,PRUint8 **aDImage,
                              PRUint8 **aSecondSImage,
                              PRUint8 **aMImage,PRInt32 *aSLSpan,PRInt32 *aDLSpan,PRInt32 *aMLSpan)
{
PRBool    doalpha = PR_FALSE;
nsRect    srect,drect,irect;
PRInt32   startx,starty;
nsRect    trect;

  if(aMaskInfo){
    drect.SetRect(0,0,aDestInfo->bmWidth,aDestInfo->bmHeight);
    trect.SetRect(aMaskUL->x,aMaskUL->y,aMaskInfo->bmWidth,aSrcInfo->bmHeight);
    drect.IntersectRect(drect, trect);
  }else{
    //arect.SetRect(0,0,aDestInfo->bmWidth,aDestInfo->bmHeight);
    //srect.SetRect(aMaskUL->x,aMaskUL->y,aWidth,aHeight);
    //arect.IntersectRect(arect,srect);

    drect.SetRect(0, 0, aDestInfo->bmWidth, aDestInfo->bmHeight);
    trect.SetRect(aSrcUL->x, aSrcUL->y, aWidth, aHeight);
    drect.IntersectRect(drect, trect);
  }

  srect.SetRect(0, 0, aSrcInfo->bmWidth, aSrcInfo->bmHeight);
  trect.SetRect(aSrcUL->x, aSrcUL->y, aWidth, aHeight);
  srect.IntersectRect(srect, trect);

  if (irect.IntersectRect(srect, drect)){
    *aNumbytes = CalcBytesSpan(irect.width, aDestInfo->bmBitsPixel);
    *aNumlines = irect.height;

    startx = irect.x;
    starty = irect.y;

    // calculate destination information
    *aDLSpan = mDRowBytes;
    *aDImage = ((PRUint8*)aDestInfo->bmBits) + (starty * (*aDLSpan)) + ((aDestInfo->bmBitsPixel >> 3) * startx);

    *aSLSpan = mSRowBytes;
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
#endif
