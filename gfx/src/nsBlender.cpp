/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsBlender.h"
#include "nsIDeviceContext.h"
#include "il_util.h"

static NS_DEFINE_IID(kIBlenderIID, NS_IBLENDER_IID);

/** ---------------------------------------------------
 *  See documentation in nsBlender.h
 *	@update 2/25/00 dwc
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

/** ---------------------------------------------------
 *  See documentation in nsBlender.h
 *	@update 2/25/00 dwc
 */
nsBlender::~nsBlender() 
{
  NS_IF_RELEASE(mContext);
}


NS_IMPL_ISUPPORTS(nsBlender, kIBlenderIID);

//------------------------------------------------------------

/** ---------------------------------------------------
 *  See documentation in nsBlender.h
 *	@update 2/25/00 dwc
 */
NS_IMETHODIMP
nsBlender::Init(nsIDeviceContext *aContext)
{
  mContext = aContext;
  NS_IF_ADDREF(mContext);
  
  // need to set up some masks for 16 bit Mac or Unix
#ifdef XP_MAC
	mRedMask = 0x7c00;
	mGreenMask = 0x03e0;
	mBlueMask = 0x001f;
	mRedSetMask = 0xf8;
	mGreenSetMask = 0xf8;
	mBlueSetMask = 0xf8;	
 	mRedShift = 7;
  mGreenShift = 2;
  mBlueShift = 3;
#endif

#ifdef XP_WIN
	mRedMask = 0xf800;
	mGreenMask = 0x07e0;
	mBlueMask = 0x001f;
	mRedSetMask = 0xf8;
	mGreenSetMask = 0xfC;
	mBlueSetMask = 0xf8;	
 	mRedShift = 8;
  mGreenShift = 3;
  mBlueShift = 3;
#endif  
  
#ifdef XP_UNIX
	mRedMask = 0xf800;
	mGreenMask = 0x07e0;
	mBlueMask = 0x001f;
	mRedSetMask = 0xf8;
	mGreenSetMask = 0xfC;
	mBlueSetMask = 0xf8;	
 	mRedShift = 8;
  mGreenShift = 3;
  mBlueShift = 3;
#endif  
  
  return NS_OK;
}

static void rangeCheck(nsIDrawingSurface* surface, PRInt32& aX, PRInt32& aY, PRInt32& aWidth, PRInt32& aHeight)
{
  PRUint32 width, height;
  surface->GetDimensions(&width, &height);
  
  // ensure that the origin is within bounds of the drawing surface.
  if (aX < 0)
    aX = 0;
  else if (aX > (PRInt32)width)
    aX = width;
  if (aY < 0)
    aY = 0;
  else if (aY > (PRInt32)height)
    aY = height;
  
  // ensure that the dimensions are within bounds.
  if (aX + aWidth > (PRInt32)width)
    aWidth = width - aX;
  if (aY + aHeight > (PRInt32)height)
    aHeight = height - aY;
}

/** ---------------------------------------------------
 *  See documentation in nsBlender.h
 *	@update 2/25/00 dwc
 */
NS_IMETHODIMP
nsBlender::Blend(PRInt32 aSX, PRInt32 aSY, PRInt32 aWidth, PRInt32 aHeight, nsDrawingSurface aSrc,
                 nsDrawingSurface aDst, PRInt32 aDX, PRInt32 aDY, float aSrcOpacity,
                 nsDrawingSurface aSecondSrc, nscolor aSrcBackColor, nscolor aSecondSrcBackColor)
{
  nsresult result = NS_ERROR_FAILURE;

  nsIDrawingSurface* srcSurface = (nsIDrawingSurface *)aSrc;
  nsIDrawingSurface* destSurface = (nsIDrawingSurface *)aDst;
  nsIDrawingSurface* secondSrcSurface = (nsIDrawingSurface *)aSecondSrc;

  // range check the coordinates in both the source and destination buffers.
  rangeCheck(srcSurface, aSX, aSY, aWidth, aHeight);
  rangeCheck(destSurface, aDX, aDY, aWidth, aHeight);

  mSrcBytes = mSecondSrcBytes = mDestBytes = nsnull;

  if (NS_OK == srcSurface->Lock(aSX, aSY, aWidth, aHeight, (void **)&mSrcBytes, &mSrcRowBytes, &mSrcSpan, NS_LOCK_SURFACE_READ_ONLY)) {
    if (NS_OK == destSurface->Lock(aDX, aDY, aWidth, aHeight, (void **)&mDestBytes, &mDestRowBytes, &mDestSpan, 0)) {
      if (secondSrcSurface)
        secondSrcSurface->Lock(aSX, aSY, aWidth, aHeight, (void **)&mSecondSrcBytes, &mSecondSrcRowBytes, &mSecondSrcSpan, NS_LOCK_SURFACE_READ_ONLY);

      nsPixelFormat pixformat;
      srcSurface->GetPixelFormat(&pixformat);

      result = Blend(mSrcBytes, mSrcRowBytes, mSrcSpan,
                     mDestBytes, mDestRowBytes, mDestSpan,
                     mSecondSrcBytes, mSecondSrcRowBytes, mSecondSrcSpan,
                     aHeight, (PRInt32)(aSrcOpacity * 100), pixformat,
                     aSrcBackColor, aSecondSrcBackColor);

      destSurface->Unlock();

      if (secondSrcSurface)
        secondSrcSurface->Unlock();
    }

    srcSurface->Unlock();
  }

  return result;
}

/** ---------------------------------------------------
 *  See documentation in nsBlender.h
 *	@update 2/25/00 dwc
 */
NS_IMETHODIMP nsBlender::Blend(PRInt32 aSX, PRInt32 aSY, PRInt32 aWidth, PRInt32 aHeight, nsIRenderingContext *aSrc,
                               nsIRenderingContext *aDest, PRInt32 aDX, PRInt32 aDY, float aSrcOpacity,
                               nsIRenderingContext *aSecondSrc, nscolor aSrcBackColor, nscolor aSecondSrcBackColor)
{
  // just hand off to the drawing surface blender, to make code easier to maintain.
  nsDrawingSurface srcSurface, destSurface, secondSrcSurface = nsnull;
  aSrc->GetDrawingSurface(&srcSurface);
  aDest->GetDrawingSurface(&destSurface);
  if (aSecondSrc != nsnull)
    aSecondSrc->GetDrawingSurface(&secondSrcSurface);
  return Blend(aSX, aSY, aWidth, aHeight, srcSurface, destSurface,
               aDX, aDY, aSrcOpacity, secondSrcSurface,
               aSrcBackColor, aSecondSrcBackColor);
}

/** ---------------------------------------------------
 *  See documentation in nsBlender.h
 *	@update 2/25/00 dwc
 */
nsresult nsBlender::Blend(PRUint8 *aSrcBits, PRInt32 aSrcStride, PRInt32 aSrcBytes,
                               PRUint8 *aDestBits, PRInt32 aDestStride, PRInt32 aDestBytes,
                               PRUint8 *aSecondSrcBits, PRInt32 aSecondSrcStride, PRInt32 aSecondSrcBytes,
                               PRInt32 aLines, PRInt32 aAlpha, nsPixelFormat &aPixFormat,
                               nscolor aSrcBackColor, nscolor aSecondSrcBackColor)
{
nsresult  result = NS_OK;
PRUint32 depth;
 
  mContext->GetDepth(depth);
  // now do the blend
  switch (depth){
    case 32:
        Do32Blend(aAlpha, aLines, aSrcBytes, aSrcBits, aDestBits,
                  aSecondSrcBits, aSrcStride, aDestStride, nsHighQual,
                  aSrcBackColor, aSecondSrcBackColor, aPixFormat);
        result = NS_OK;
      break;

    case 24:
        Do24Blend(aAlpha, aLines, aSrcBytes, aSrcBits, aDestBits,
                  aSecondSrcBits, aSrcStride, aDestStride, nsHighQual,
                  aSrcBackColor, aSecondSrcBackColor, aPixFormat);
      break;

    case 16:
        Do16Blend(aAlpha, aLines, aSrcBytes, aSrcBits, aDestBits,
                  aSecondSrcBits, aSrcStride, aDestStride, nsHighQual,
                  aSrcBackColor, aSecondSrcBackColor, aPixFormat);
      break;

    case 8:
    {
      IL_ColorSpace *thespace = nsnull;

        if ((result = mContext->GetILColorSpace(thespace)) == NS_OK){
          Do8Blend(aAlpha, aLines, aSrcBytes, aSrcBits, aDestBits,
                   aSecondSrcBits, aSrcStride, aDestStride, thespace,
                   nsHighQual, aSrcBackColor, aSecondSrcBackColor);
          IL_ReleaseColorSpace(thespace);
        }
      break;
    }
  }

  return result;
}


/** ---------------------------------------------------
 *  See documentation in nsBlender.h
 *	@update 2/25/00 dwc
 */
void
nsBlender::Do32Blend(PRUint8 aBlendVal,PRInt32 aNumlines,PRInt32 aNumbytes,PRUint8 *aSImage,PRUint8 *aDImage,PRUint8 *aSecondSImage,PRInt32 aSLSpan,PRInt32 aDLSpan,nsBlendQuality aBlendQuality,nscolor aSrcBackColor, nscolor aSecondSrcBackColor, nsPixelFormat &aPixFormat)
{
PRUint8   *d1,*d2,*s1,*s2,*ss1,*ss2;
PRUint32  val1,val2;
PRInt32   x,y,temp1,numlines,numPixels,xinc,yinc;
PRUint32  srccolor,secsrccolor,i;
PRUint32  pixSColor,pixSSColor;

  aBlendVal = (aBlendVal*255)/100;
  val2 = aBlendVal;
  val1 = 255-val2;

  // now go thru the image and blend (remember, its bottom upwards)
  s1 = aSImage;
  d1 = aDImage;

  numlines = aNumlines;  
  xinc = 1;
  yinc = 1;

  if (nsnull != aSecondSImage){
    ss1 = (PRUint8 *)aSecondSImage;
    srccolor = ((NS_GET_R(aSrcBackColor)<<16)) | ((NS_GET_G(aSrcBackColor) <<8)) | ((NS_GET_B(aSrcBackColor)));
    secsrccolor = ((NS_GET_R(aSecondSrcBackColor)<<16)) | ((NS_GET_G(aSecondSrcBackColor) <<8)) | ((NS_GET_B(aSecondSrcBackColor)));
  }else {
    ss1 = nsnull;
  }
  
  if(nsnull == ss1){
    for(y = 0; y < aNumlines; y++){
      s2 = s1;
      d2 = d1;
      for(x = 0; x < aNumbytes; x++){
        temp1 = (((*d2)*val1)+((*s2)*val2))>>8;
        if(temp1>255){
          temp1 = 255;
        }

        *d2 = (PRUint8)temp1; 

        d2++;
        s2++;
      }

      s1 += aSLSpan;
      d1 += aDLSpan;
    }
  } else {
    numPixels = aNumbytes/4;
    for(y = 0; y < aNumlines; y++){
      s2 = s1;
      d2 = d1;
      ss2=ss1;
      for(x=0;x<numPixels;x++){
        pixSColor =  *((PRUint32*)(s2))&0xFFFFFF;
        pixSSColor = *((PRUint32*)(ss2))&0xFFFFFF ;

        if((pixSColor!=srccolor) || (pixSSColor!=secsrccolor)) {
          for(i=0;i<4;i++){
            temp1 = (((*d2)*val1)+((*s2)*val2))>>8;
            if(temp1>255){
              temp1 = 255;
            }
            *d2 = (PRUint8)temp1; 
            d2++;
            s2++;
            ss2++;
          }
        } else {
          d2+=4;
          s2+=4;
          ss2+=4;
        }
      }
     s1 += aSLSpan;
     d1 += aDLSpan;
     ss1+= aDLSpan;
    }
  }
}


/** ---------------------------------------------------
 *  See documentation in nsBlender.h
 *	@update 2/25/00 dwc
 */
void
nsBlender::Do24Blend(PRUint8 aBlendVal,PRInt32 aNumlines,PRInt32 aNumbytes,PRUint8 *aSImage,PRUint8 *aDImage,PRUint8 *aSecondSImage,PRInt32 aSLSpan,PRInt32 aDLSpan,nsBlendQuality aBlendQuality,nscolor aSrcBackColor, nscolor aSecondSrcBackColor, nsPixelFormat &aPixFormat)
{
PRUint8   *d1,*d2,*s1,*s2,*ss1,*ss2;
PRUint32  val1,val2;
PRInt32   x,y,temp1,numlines,numPixels,xinc,yinc;
PRUint32  srccolor,secsrccolor,i;
PRUint32  pixSColor,pixSSColor;

  aBlendVal = (aBlendVal*255)/100;
  val2 = aBlendVal;
  val1 = 255-val2;

  // now go thru the image and blend (remember, its bottom upwards)
  s1 = aSImage;
  d1 = aDImage;

  numlines = aNumlines;  
  xinc = 1;
  yinc = 1;

  if (nsnull != aSecondSImage){
    ss1 = (PRUint8 *)aSecondSImage;
    srccolor = ((NS_GET_R(aSrcBackColor)<<16)) | ((NS_GET_G(aSrcBackColor) <<8)) | ((NS_GET_B(aSrcBackColor)));
    secsrccolor = ((NS_GET_R(aSecondSrcBackColor)<<16)) | ((NS_GET_G(aSecondSrcBackColor) <<8)) | ((NS_GET_B(aSecondSrcBackColor)));
  }else {
    ss1 = nsnull;
  }
  
  if(nsnull == ss1){
    for(y = 0; y < aNumlines; y++){
      s2 = s1;
      d2 = d1;
      for(x = 0; x < aNumbytes; x++){
        temp1 = (((*d2)*val1)+((*s2)*val2))>>8;
        if(temp1>255){
          temp1 = 255;
        }

        *d2 = (PRUint8)temp1; 

        d2++;
        s2++;
      }

      s1 += aSLSpan;
      d1 += aDLSpan;
    }
  } else {
    numPixels = aNumbytes/3;
    for(y = 0; y < aNumlines; y++){
      s2 = s1;
      d2 = d1;
      ss2=ss1;
      for(x=0;x<numPixels;x++){
        pixSColor =  *((PRUint32*)(s2))&0xFFFFFF;
        pixSSColor = *((PRUint32*)(ss2))&0xFFFFFF ;

        if((pixSColor!=srccolor) || (pixSSColor!=secsrccolor)) {
          for(i=0;i<3;i++){
            temp1 = (((*d2)*val1)+((*s2)*val2))>>8;
            if(temp1>255){
              temp1 = 255;
            }
            *d2 = (PRUint8)temp1; 
            d2++;
            s2++;
            ss2++;
          }
        } else {
          d2+=3;
          s2+=3;
          ss2+=3;
        }
      }
     s1 += aSLSpan;
     d1 += aDLSpan;
     ss1+= aDLSpan;
    }
  }
}

//------------------------------------------------------------



#define RED16(x)    (((x) & mRedMask) >> mRedShift)
#define GREEN16(x)  (((x) & mGreenMask) >> mGreenShift)
#define BLUE16(x)   (((x) & mBlueMask) << mBlueShift)


/** ---------------------------------------------------
 *  See documentation in nsBlender.h
 *	@update 2/25/00 dwc
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

  if (nsnull != aSecondSImage) {
    ss1 = (PRUint16 *)aSecondSImage;
    srccolor = ((NS_GET_R(aSrcBackColor) & mRedSetMask) << mRedShift) |
               ((NS_GET_G(aSrcBackColor) & mGreenSetMask) << mGreenShift) |
               ((NS_GET_B(aSrcBackColor) & mBlueSetMask) >> mBlueShift);
    secsrccolor = ((NS_GET_R(aSecondSrcBackColor) & mRedSetMask) << mRedShift) |
                  ((NS_GET_G(aSecondSrcBackColor) & mGreenSetMask) << mGreenShift) |
                  ((NS_GET_B(aSecondSrcBackColor) & mBlueSetMask) >> mBlueShift);
  } else {
    ss1 = nsnull;
  }

  if (nsnull != ss1){
    for (y = 0; y < aNumlines; y++){
      s2 = s1;
      d2 = d1;
      ss2 = ss1;

      for (x = 0; x < span; x++){
        stemp = *s2;
        sstemp = *ss2;

        if ((stemp != srccolor) || (sstemp != secsrccolor)) {
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

          *d2 = (PRUint16)((red & mRedSetMask) << mRedShift) | ((green & mGreenSetMask) << mGreenShift) | ((blue & mBlueSetMask) >> mBlueShift);
        }

        d2++;
        s2++;
        ss2++;
      }

      s1 += sspan;
      d1 += dspan;
      ss1 += sspan;
    }
  } else {
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


//------------------------------------------------------------

extern void inv_colormap(PRInt16 colors,PRUint8 *aCMap,PRInt16 bits,PRUint32 *dist_buf,PRUint8 *aRGBMap );

/** ---------------------------------------------------
 *  See documentation in nsBlender.h
 *	@update 2/25/00 dwc
 */
void
nsBlender::Do8Blend(PRUint8 aBlendVal,PRInt32 aNumlines,PRInt32 aNumbytes,PRUint8 *aSImage,PRUint8 *aDImage,PRUint8 *aSecondSImage,PRInt32 aSLSpan,PRInt32 aDLSpan,IL_ColorSpace *aColorMap,nsBlendQuality aBlendQuality,nscolor aSrcBackColor, nscolor aSecondSrcBackColor)
{
PRUint32  r,g,b,r1,g1,b1,i;
PRUint8   *d1,*d2,*s1,*s2;
PRInt32   x,y,val1,val2,numlines,xinc,yinc;;
PRUint8   *mapptr,*invermap;
PRUint32  *distbuffer;
PRUint32  quantlevel,tnum,num,shiftnum;
NI_RGB    *map;

  if (nsnull == aColorMap->cmap.map) {
    NS_ASSERTION(PR_FALSE, "NULL Colormap during 8-bit blend");
    return;
  } 

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

  // Compute "strides" for accessing the arrays
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

