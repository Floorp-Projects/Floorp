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

#include "nsBlender.h"
#include "nsIDeviceContext.h"
#include "il_util.h"

static NS_DEFINE_IID(kIBlenderIID, NS_IBLENDER_IID);

/** --------------------------------------------------------------------------
* General constructor for a nsBlender object
*/
nsBlender :: nsBlender()
{
  NS_INIT_REFCNT();

  mContext = nsnull;

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

nsBlender::~nsBlender() 
{
  NS_IF_RELEASE(mContext);
}


NS_IMPL_ISUPPORTS(nsBlender, kIBlenderIID);

//------------------------------------------------------------

NS_IMETHODIMP
nsBlender::Init(nsIDeviceContext *aContext)
{
  mContext = aContext;
  NS_IF_ADDREF(mContext);

  return NS_OK;
}

NS_IMETHODIMP
nsBlender::Blend(PRInt32 aSX, PRInt32 aSY, PRInt32 aWidth, PRInt32 aHeight,nsDrawingSurface aSrc,
                    nsDrawingSurface aDst, PRInt32 aDX, PRInt32 aDY, float aSrcOpacity,
                    nsDrawingSurface aSecondSrc, nscolor aSrcBackColor, nscolor aSecondSrcBackColor)
{
  nsresult      result = NS_ERROR_FAILURE;
  nsPoint       srcloc, maskloc;
  PRInt32       dlinespan, slinespan, mlinespan, numbytes, numlines, level;
  PRUint8       *s1, *d1, *m1, *mask = NULL, *ssl = NULL;
  PRBool        srcissurf = PR_FALSE;
  PRBool        secondsrcissurf = PR_FALSE;
  PRBool        dstissurf = PR_FALSE;
  nsPixelFormat pixformat;
  nsIDrawingSurface   *SrcSurf, *DstSurf, *SecondSrcSurf;

  SrcSurf = (nsIDrawingSurface *)aSrc;
  DstSurf = (nsIDrawingSurface *)aDst;
  SecondSrcSurf = (nsIDrawingSurface *)aSecondSrc;

  mSrcBytes = mSecondSrcBytes = mDestBytes = nsnull;

  if (NS_OK == SrcSurf->Lock(aSX, aSY, aWidth, aHeight, (void **)&mSrcBytes, &mSrcRowBytes, &mSrcSpan, NS_LOCK_SURFACE_READ_ONLY))
  {
    if (NS_OK == DstSurf->Lock(aSX, aSY, aWidth, aHeight, (void **)&mDestBytes, &mDestRowBytes, &mDestSpan, 0))
    {
      if (SecondSrcSurf)
        SecondSrcSurf->Lock(aSX, aSY, aWidth, aHeight, (void **)&mSecondSrcBytes, &mSecondSrcRowBytes, &mSecondSrcSpan, NS_LOCK_SURFACE_READ_ONLY);

      srcloc.x = 0;
      srcloc.y = 0;

      maskloc.x = 0;
      maskloc.y = 0;

      SrcSurf->GetPixelFormat(&pixformat);

      result = Blend(mSrcBytes, mSrcRowBytes, mSrcSpan,
                     mDestBytes, mDestRowBytes, mDestSpan,
                     mSecondSrcBytes, mSecondSrcRowBytes, mSecondSrcSpan,
                     aHeight, (PRInt32)(aSrcOpacity * 100), pixformat,
                     aSrcBackColor, aSecondSrcBackColor);

      DstSurf->Unlock();

      if (SecondSrcSurf)
        SecondSrcSurf->Unlock();
    }

    SrcSurf->Unlock();
  }

  return result;
}

NS_IMETHODIMP nsBlender::Blend(PRInt32 aSX, PRInt32 aSY, PRInt32 aWidth, PRInt32 aHeight, nsIRenderingContext *aSrc,
                   nsIRenderingContext *aDest, PRInt32 aDX, PRInt32 aDY, float aSrcOpacity,
                   nsIRenderingContext *aSecondSrc, nscolor aSrcBackColor,
                   nscolor aSecondSrcBackColor)
{
  nsresult      result = NS_ERROR_FAILURE;
  nsPoint       srcloc, maskloc;
  PRInt32       dlinespan, slinespan, mlinespan, numbytes, numlines, level;
  PRUint8       *s1, *d1, *m1, *mask = NULL, *ssl = NULL;
  PRBool        srcissurf = PR_FALSE;
  PRBool        secondsrcissurf = PR_FALSE;
  PRBool        dstissurf = PR_FALSE;
  nsPixelFormat pixformat;
  nsDrawingSurface srcsurf;

  mSrcBytes = mSecondSrcBytes = mDestBytes = nsnull;

  if (NS_OK == aSrc->LockDrawingSurface(aSX, aSY, aWidth, aHeight, (void **)&mSrcBytes, &mSrcRowBytes, &mSrcSpan, NS_LOCK_SURFACE_READ_ONLY))
  {
    if (NS_OK == aDest->LockDrawingSurface(aSX, aSY, aWidth, aHeight, (void **)&mDestBytes, &mDestRowBytes, &mDestSpan, 0))
    {
      if (aSecondSrc)
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

      aDest->UnlockDrawingSurface();

      if (aSecondSrc)
        aSecondSrc->UnlockDrawingSurface();
    }

    aSrc->UnlockDrawingSurface();
  }

  return result;
}

nsresult nsBlender::Blend(PRUint8 *aSrcBits, PRInt32 aSrcStride, PRInt32 aSrcBytes,
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
PRBool nsBlender::CalcAlphaMetrics(BITMAP *aSrcInfo,BITMAP *aDestInfo, BITMAP *aSecondSrcInfo,
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

/** --------------------------------------------------------------------------
 * Blend two 32 bit image arrays
 * @param aNumlines  Number of lines to blend
 * @param aNumberBytes Number of bytes per line to blend
 * @param aSImage Pointer to beginning of the source bytes
 * @param aDImage Pointer to beginning of the destination bytes
 * @param aMImage Pointer to beginning of the mask bytes
 * @param aSLSpan number of bytes per line for the source bytes
 * @param aDLSpan number of bytes per line for the destination bytes
 * @param aMLSpan number of bytes per line for the Mask bytes
 * @param aBlendQuality The quality of this blend, this is for tweening if neccesary
 */
void
nsBlender::Do32Blend(PRUint8 aBlendVal,PRInt32 aNumlines,PRInt32 aNumbytes,PRUint8 *aSImage,PRUint8 *aDImage,PRUint8 *aSecondSImage,PRInt32 aSLSpan,PRInt32 aDLSpan,nsBlendQuality aBlendQuality,nscolor aSrcBackColor, nscolor aSecondSrcBackColor, nsPixelFormat &aPixFormat)
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

  for (y = 0; y < aNumlines; y++){
    s2 = s1;
    d2 = d1;

    for (x = 0; x < aNumbytes; x++) {
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

/** --------------------------------------------------------------------------
 * Blend two 24 bit image arrays using an 8 bit alpha mask
 * @param aNumlines  Number of lines to blend
 * @param aNumberBytes Number of bytes per line to blend
 * @param aSImage Pointer to beginning of the source bytes
 * @param aDImage Pointer to beginning of the destination bytes
 * @param aMImage Pointer to beginning of the mask bytes
 * @param aSLSpan number of bytes per line for the source bytes
 * @param aDLSpan number of bytes per line for the destination bytes
 * @param aMLSpan number of bytes per line for the Mask bytes
 * @param aBlendQuality The quality of this blend, this is for tweening if neccesary
 */
void
nsBlender::Do24BlendWithMask(PRInt32 aNumlines,PRInt32 aNumbytes,PRUint8 *aSImage,PRUint8 *aDImage,PRUint8 *aMImage,PRInt32 aSLSpan,PRInt32 aDLSpan,PRInt32 aMLSpan,nsBlendQuality aBlendQuality)
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

  for (y = 0; y < aNumlines; y++){
    s2 = s1;
    d2 = d1;
    m2 = m1;

    for(x=0;x<aNumbytes;x++){
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

/** --------------------------------------------------------------------------
 * Blend two 24 bit image arrays using a passed in blend value
 * @param aNumlines  Number of lines to blend
 * @param aNumberBytes Number of bytes per line to blend
 * @param aSImage Pointer to beginning of the source bytes
 * @param aDImage Pointer to beginning of the destination bytes
 * @param aMImage Pointer to beginning of the mask bytes
 * @param aSLSpan number of bytes per line for the source bytes
 * @param aDLSpan number of bytes per line for the destination bytes
 * @param aMLSpan number of bytes per line for the Mask bytes
 * @param aBlendQuality The quality of this blend, this is for tweening if neccesary
 */
void
nsBlender::Do24Blend(PRUint8 aBlendVal,PRInt32 aNumlines,PRInt32 aNumbytes,PRUint8 *aSImage,PRUint8 *aDImage,PRUint8 *aSecondSImage,PRInt32 aSLSpan,PRInt32 aDLSpan,nsBlendQuality aBlendQuality,nscolor aSrcBackColor, nscolor aSecondSrcBackColor, nsPixelFormat &aPixFormat)
{
PRUint8   *d1,*d2,*s1,*s2,*ss1;
PRUint32  val1,val2;
PRInt32   x,y,temp1,numlines,xinc,yinc;
PRUint16  srccolor,secsrccolor;

  aBlendVal = (aBlendVal*255)/100;
  val2 = aBlendVal;
  val1 = 255-val2;

  // now go thru the image and blend (remember, its bottom upwards)
  s1 = aSImage;
  d1 = aDImage;

  numlines = aNumlines;  
  xinc = 1;
  yinc = 1;

  if (nsnull != aSecondSImage)
  {
    ss1 = (PRUint8 *)aSecondSImage;
    srccolor = ((NS_GET_R(aSrcBackColor) & 0xf8) << 8) |
               ((NS_GET_G(aSrcBackColor) & 0xfc) << 3) |
               ((NS_GET_B(aSrcBackColor) & 0xf8) >> 3);
    secsrccolor = ((NS_GET_R(aSecondSrcBackColor) & 0xf8) << 8) |
                  ((NS_GET_G(aSecondSrcBackColor) & 0xfc) << 3) |
                  ((NS_GET_B(aSecondSrcBackColor) & 0xf8) >> 3);
  }
  else
    ss1 = nsnull;

  for(y = 0; y < aNumlines; y++){
    s2 = s1;
    d2 = d1;

    for(x = 0; x < aNumbytes; x++){
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

//------------------------------------------------------------

#define RED16(x)    (((x) & 0xf800) >> 8)
#define GREEN16(x)  (((x) & 0x07e0) >> 3)
#define BLUE16(x)   (((x) & 0x001f) << 3)

/** --------------------------------------------------------------------------
 * Blend two 16 bit image arrays using a passed in blend value
 * @param aNumlines  Number of lines to blend
 * @param aNumberBytes Number of bytes per line to blend
 * @param aSImage Pointer to beginning of the source bytes
 * @param aDImage Pointer to beginning of the destination bytes
 * @param aMImage Pointer to beginning of the mask bytes
 * @param aSLSpan number of bytes per line for the source bytes
 * @param aDLSpan number of bytes per line for the destination bytes
 * @param aMLSpan number of bytes per line for the Mask bytes
 * @param aBlendQuality The quality of this blend, this is for tweening if neccesary
 */
void
nsBlender::Do16Blend(PRUint8 aBlendVal,PRInt32 aNumlines,PRInt32 aNumbytes,PRUint8 *aSImage,PRUint8 *aDImage,PRUint8 *aSecondSImage,PRInt32 aSLSpan,PRInt32 aDLSpan,nsBlendQuality aBlendQuality,nscolor aSrcBackColor, nscolor aSecondSrcBackColor, nsPixelFormat &aPixFormat)
{
PRUint16    *d1,*d2,*s1,*s2,*ss1,*ss2;
PRUint32    val1,val2,red,green,blue,stemp,dtemp,sstemp;
PRInt32     x,y,numlines,xinc,yinc;
PRUint16    srccolor,secsrccolor;
PRInt16     dspan,sspan,span;

  // since we are using 16 bit pointers, the num bytes need to be cut by 2
  aBlendVal = (aBlendVal * 255) / 100;
  val2 = aBlendVal;
  val1 = 255-val2;

  // now go thru the image and blend (remember, its bottom upwards)
  s1 = (PRUint16*)aSImage;
  d1 = (PRUint16*)aDImage;
  dspan = aDLSpan >> 1;
  sspan = aSLSpan >> 1;
  span = aNumbytes >> 1;
  numlines = aNumlines;
  xinc = 1;
  yinc = 1;

  if (nsnull != aSecondSImage)
  {
    ss1 = (PRUint16 *)aSecondSImage;
    srccolor = ((NS_GET_R(aSrcBackColor) & 0xf8) << 8) |
               ((NS_GET_G(aSrcBackColor) & 0xfc) << 3) |
               ((NS_GET_B(aSrcBackColor) & 0xf8) >> 3);
    secsrccolor = ((NS_GET_R(aSecondSrcBackColor) & 0xf8) << 8) |
                  ((NS_GET_G(aSecondSrcBackColor) & 0xfc) << 3) |
                  ((NS_GET_B(aSecondSrcBackColor) & 0xf8) >> 3);
  }
  else
    ss1 = nsnull;

  if (nsnull != ss1){
    for (y = 0; y < aNumlines; y++){
      s2 = s1;
      d2 = d1;
      ss2 = ss1;

      for (x = 0; x < span; x++){
        stemp = *s2;
        sstemp = *ss2;

        if ((stemp != srccolor) || (sstemp != secsrccolor))
        {
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
        }

        d2++;
        s2++;
        ss2++;
      }

      s1 += sspan;
      d1 += dspan;
      ss1 += sspan;
    }
  }
  else
  {
    for (y = 0; y < aNumlines; y++){
      s2 = s1;
      d2 = d1;

      for (x = 0; x < span; x++){
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

/** --------------------------------------------------------------------------
 * Blend two 8 bit image arrays using an 8 bit alpha mask
 * @param aNumlines  Number of lines to blend
 * @param aNumberBytes Number of bytes per line to blend
 * @param aSImage Pointer to beginning of the source bytes
 * @param aDImage Pointer to beginning of the destination bytes
 * @param aMImage Pointer to beginning of the mask bytes
 * @param aSLSpan number of bytes per line for the source bytes
 * @param aDLSpan number of bytes per line for the destination bytes
 * @param aMLSpan number of bytes per line for the Mask bytes
 * @param aBlendQuality The quality of this blend, this is for tweening if neccesary
 */
void
nsBlender::Do8BlendWithMask(PRInt32 aNumlines,PRInt32 aNumbytes,PRUint8 *aSImage,PRUint8 *aDImage,PRUint8 *aMImage,PRInt32 aSLSpan,PRInt32 aDLSpan,PRInt32 aMLSpan,nsBlendQuality aBlendQuality)
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

  for (y = 0; y < aNumlines; y++){
    s2 = s1;
    d2 = d1;
    m2 = m1;

    for(x=0;x<aNumbytes;x++){
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

/** --------------------------------------------------------------------------
 * Blend two 8 bit image arrays using a passed in blend value
 * @param aNumlines  Number of lines to blend
 * @param aNumberBytes Number of bytes per line to blend
 * @param aSImage Pointer to beginning of the source bytes
 * @param aDImage Pointer to beginning of the destination bytes
 * @param aMImage Pointer to beginning of the mask bytes
 * @param aSLSpan number of bytes per line for the source bytes
 * @param aDLSpan number of bytes per line for the destination bytes
 * @param aMLSpan number of bytes per line for the Mask bytes
 * @param aBlendQuality The quality of this blend, this is for tweening if neccesary
 */
void
nsBlender::Do8Blend(PRUint8 aBlendVal,PRInt32 aNumlines,PRInt32 aNumbytes,PRUint8 *aSImage,PRUint8 *aDImage,PRUint8 *aSecondSImage,PRInt32 aSLSpan,PRInt32 aDLSpan,IL_ColorSpace *aColorMap,nsBlendQuality aBlendQuality,nscolor aSrcBackColor, nscolor aSecondSrcBackColor)
{
PRUint32   r,g,b,r1,g1,b1,i;
PRUint8   *d1,*d2,*s1,*s2;
PRInt32   x,y,val1,val2,numlines,xinc,yinc;;
PRUint8   *mapptr,*invermap;
PRUint32  *distbuffer;
PRUint32  quantlevel,tnum,num,shiftnum;
NI_RGB    *map;

  aBlendVal = (aBlendVal*255)/100;
  val2 = aBlendVal;
  val1 = 255-val2;

  // build a colormap we can use to get an inverse map
  map = aColorMap->cmap.map+10;
  mapptr = new PRUint8[256*3];
  invermap = mapptr;
  for(i=0;i<216;i++){
    *invermap=map->blue;
    invermap++;
    *invermap=map->green;
    invermap++;
    *invermap=map->red;
    invermap++;
    map++;
  }
  for(i=216;i<256;i++){
    *invermap=0;
    invermap++;
    *invermap=0;
    invermap++;
    *invermap=0;
    invermap++;
  }

  quantlevel = aBlendQuality+2;
  quantlevel = 4;                   // 4                                          5
  shiftnum = (8-quantlevel)+8;      // 12                                         11 
  tnum = 2;                         // 2                                          2
  for(i=1;i<=quantlevel;i++)
    tnum = 2*tnum;                  // 2, 4, 8, tnum = 16                         32

  num = tnum;                       // num = 16                                   32
  for(i=1;i<3;i++)
    num = num*tnum;                 // 256, 4096                                  32768

  distbuffer = new PRUint32[num];   // new PRUint[4096]                           32768
   invermap = new PRUint8[num];
  inv_colormap(216,mapptr,quantlevel,distbuffer,invermap );  // 216,mapptr[],4,distbuffer[4096],invermap[12288])

  // now go thru the image and blend (remember, its bottom upwards)
  s1 = aSImage;
  d1 = aDImage;

  numlines = aNumlines;  
  xinc = 1;
  yinc = 1;

 
  for(y = 0; y < aNumlines; y++){
    s2 = s1;
    d2 = d1;
 
    for(x = 0; x < aNumbytes; x++){
      i = (*d2);
      i-=10;
      r = mapptr[(3 * i) + 2];
      g = mapptr[(3 * i) + 1];
      b = mapptr[(3 * i)];
 
      i =(*s2);
      i-=10;
      r1 = mapptr[(3 * i) + 2];
      g1 = mapptr[(3 * i) + 1];
      b1 = mapptr[(3 * i)];

      r = ((r*val1)+(r1*val2))>>shiftnum;
      //r=r>>quantlevel;
      if(r>tnum)
        r = tnum;

      g = ((g*val1)+(g1*val2))>>shiftnum;
      //g=g>>quantlevel;
      if(g>tnum)
        g = tnum;

      b = ((b*val1)+(b1*val2))>>shiftnum;
      //b=b>>quantlevel;
      if(b>tnum)
        b = tnum;

      r = (r<<(2*quantlevel))+(g<<quantlevel)+b;
      r = (invermap[r]+10);
      (*d2)=r;
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

/** --------------------------------------------------------------------------
 * Create an inverse colormap for a given color table
 * @param colors -- Number of colors
 * @param aCMap -- The color map
 * @param aBits -- Resolution in bits for the inverse map
 * @param dist_buf -- a buffer to hold the temporary colors in while we calculate the map
 * @param aRGBMap -- the map to put the inverse colors into for the color table
 * @return VOID
 */
void
inv_colormap(PRInt16 aNumColors,PRUint8 *aCMap,PRInt16 aBits,PRUint32 *dist_buf,PRUint8 *aRGBMap )
{
PRInt32         nbits = 8 - aBits;               // 3
PRUint32        r,g,b;

  colormax = 1 << aBits;                        // 32
  x = 1 << nbits;                               // 8
  xsqr = 1 << (2 * nbits);                      // 64   

  // Compute "strides" for accessing the arrays. */
  gstride = colormax;                           // 32
  rstride = colormax * colormax;                // 1024

  maxfill( dist_buf, colormax );

  for (cindex = 0;cindex < aNumColors;cindex++ ){
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
	  cdp = dist_buf + rcenter * rstride + gcenter * gstride + bcenter;   // dist 01533120 .. 
	  crgbp = aRGBMap + rcenter * rstride + gcenter * gstride + bcenter;  // aRGBMap 01553168

	  (void)redloop();
  }
}

/** --------------------------------------------------------------------------
 * find a red max or min value in a color cube
 * @return  a red component in a certain span
 */
static PRInt32
redloop()
{
PRInt32        detect,r,first;
PRInt32        txsqr = xsqr + xsqr;
static PRInt32 rxx;

  detect = 0;

  rdist = cdist;   // 48
  rxx = crinc;     // 128
  rdp = cdp;
  rrgbp = crgbp;   // 01553168
  first = 1;
  for (r=rcenter;r<colormax;r++,rdp+=rstride,rrgbp+=rstride,rdist+=rxx,rxx+=txsqr,first=0){
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
  for (r=rcenter-1;r>=0;r--,rdp-=rstride,rrgbp-=rstride,rxx-=txsqr,rdist-=rxx,first=0){
    if ( greenloop( first ) )
      detect = 1;
    else 
      if ( detect )
        break;
  }
    
  return detect;
}

/** --------------------------------------------------------------------------
 * find a green max or min value in a color cube
 * @return  a red component in a certain span
 */
static PRInt32
greenloop(PRInt32 aRestart)
{
PRInt32          detect,g,first;
PRInt32          txsqr = xsqr + xsqr;
static  PRInt32  here, min, max;
static  PRInt32  ginc, gxx, gcdist;	
static  PRUint32  *gcdp;
static  PRUint8   *gcrgbp;	

  if(aRestart){
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
      gdist+=gxx,gcdist+=gxx,gxx+=txsqr,first=0){
    if(blueloop(first)){
      if (!detect){
        if (g>here){
	        here = g;
	        rdp = gcdp;
	        rrgbp = gcrgbp;
	        rdist = gcdist;
	        ginc = gxx;
        }
        detect=1;
      }
    }else 
      if (detect){
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
      gxx-=txsqr,gdist-=gxx,gcdist-=gxx,first=0){
    if (blueloop(first)){
      if (!detect){
        here = g;
        rdp = gcdp;
        rrgbp = gcrgbp;
        rdist = gcdist;
        ginc = gxx;
        detect = 1;
      }
    } else 
      if ( detect ){
        break;
      }
  }
  return detect;
}

/** --------------------------------------------------------------------------
 * find a blue max or min value in a color cube
 * @return  a red component in a certain span
 */
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

  if (aRestart){
    here = bcenter;
    min = 0;
    max = colormax - 1;
    binc = cbinc;
  }

  detect = 0;
  bdist = gdist;

// Basic loop, finds first applicable cell.
  for (b=here,bxx=binc,dp=gdp,rgbp=grgbp,lim=max;b<=lim;
      b++,dp++,rgbp++,bdist+=bxx,bxx+=txsqr){
    if(*dp>bdist){
      if(b>here){
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
  for (;b<=lim;b++,dp++,rgbp++,bdist+=bxx,bxx+=txsqr){
    if (*dp>bdist){
      *dp = bdist;
      *rgbp = i;
    }
    else{
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
    for (;b>=lim;b--,dp--,rgbp--,bxx-=txsqr,bdist-=bxx){
      if (*dp>bdist){
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
  for (;b>=lim;b--,dp--,rgbp--,bxx-=txsqr,bdist-=bxx){
    if (*dp>bdist){
      *dp = bdist;
      *rgbp = i;
    } else{
      break;
      }
  }

  // If we saw something, update the edge trackers.
  return detect;
}

/** --------------------------------------------------------------------------
 * fill in a span with a max value
 * @return  VOID
 */
static void
maxfill(PRUint32 *buffer,PRInt32 side )
{
register PRInt32 maxv = ~0L;
register PRInt32 i;
register PRUint32 *bp;

  for (i=side*side*side,bp=buffer;i>0;i--,bp++)         // side*side*side = 32768
    *bp = maxv;
}

//------------------------------------------------------------

