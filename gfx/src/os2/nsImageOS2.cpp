/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is John Fairhurst,
 * <john_fairhurst@iname.com>.  Portions created by John Fairhurst are
 * Copyright (C) 1999 John Fairhurst. All Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * This Original Code has been modified by IBM Corporation. Modifications made by IBM 
 * described herein are Copyright (c) International Business Machines Corporation, 2000.
 * Modifications to Mozilla code or documentation identified per MPL Section 3.3
 *
 * Date             Modified by     Description of modification
 * 05/11/2000       IBM Corp.      Make it look more like Windows.
 */

#include "nsGfxDefs.h"
#include <stdlib.h>

#include "nsImageOS2.h"
#include "nsIDeviceContext.h"
#include "nsRenderingContextOS2.h"

#define ROP_NOTSRCAND 0x22 // NOT(SRC) AND DST

NS_IMPL_ISUPPORTS(nsImageOS2, NS_GET_IID(nsIImage));

//------------------------------------------------------------
nsImageOS2::nsImageOS2()
{
   NS_INIT_REFCNT();
    
   mInfo      = 0;
   mRowBytes    = 0;
   mImageBits = 0;
   mBitmap    = 0;

   mARowBytes    = 0;
   mAlphaBits = 0;
   mABitmap    = 0;
   mAlphaDepth = 0;
   mAlphaLevel = 0;

   mColorMap = 0;

   mIsOptimized = PR_FALSE;
   mIsTopToBottom = PR_FALSE;   

   mDeviceDepth = 0;
   mNaturalWidth = 0;
   mNaturalHeight = 0;

}

nsImageOS2::~nsImageOS2()
{
   CleanUp(PR_TRUE);
}

nsresult nsImageOS2::Init( PRInt32 aWidth, PRInt32 aHeight, PRInt32 aDepth,
                           nsMaskRequirements aMaskRequirements)
{
   // Guard against memory leak in multiple init
   CleanUp(PR_TRUE);

   // (copying windows code - what about monochrome?  Oh well.)
   NS_ASSERTION( aDepth == 24 || aDepth == 8, "Bad image depth");

   // Work out size of bitmap to allocate
   mRowBytes = RASWIDTH(aWidth,aDepth);

// mRowBytes = aWidth * aDepth;
// if( aDepth < 8)
//    mRowBytes += 7;
// mRowBytes /= 8;
//
// // Make sure image width is 4byte aligned
// mRowBytes = (mRowBytes + 3) & ~0x3;

  SetDecodedRect(0,0,0,0);  //init
  SetNaturalWidth(0);
  SetNaturalHeight(0);

   mImageBits = new PRUint8 [ aHeight * mRowBytes ];

   // Set up bitmapinfo header
   int cols = -1;
   if( aDepth == 8) cols = COLOR_CUBE_SIZE;
   else if( aDepth <= 32) cols = 0;

   int szStruct = sizeof( BITMAPINFOHEADER2) + cols * sizeof( RGB2);

   mInfo = (PBITMAPINFO2) calloc( szStruct, 1);
   mInfo->cbFix = sizeof( BITMAPINFOHEADER2);
   mInfo->cx = aWidth;
   mInfo->cy = aHeight;
   mInfo->cPlanes = 1;
   mInfo->cBitCount = (USHORT) aDepth;

   // We can't set up the bitmap colour table yet.

   // init color map.
   // XP will update the color map & then call ImageUpdated(), at which
   // point we can change the color table in the bitmapinfo.
   if( aDepth == 8)
   {
      mColorMap = new nsColorMap;
      mColorMap->NumColors = COLOR_CUBE_SIZE;
      mColorMap->Index = new PRUint8[3 * mColorMap->NumColors];
   }

   // Allocate stuff for mask bitmap
   if( aMaskRequirements != nsMaskRequirements_kNoMask)
   {
      if( aMaskRequirements == nsMaskRequirements_kNeeds1Bit)
      {
         mARowBytes = (aWidth + 7) / 8;
         mAlphaDepth = 1;
      }
      else
      {
         NS_ASSERTION( nsMaskRequirements_kNeeds8Bit == aMaskRequirements,
                       "unexpected mask depth");
         mARowBytes = aWidth;
         mAlphaDepth = 8;
      }

      // 32-bit align each row
      mARowBytes = (mARowBytes + 3) & ~0x3;

      mAlphaBits = new PRUint8 [ aHeight * mARowBytes];
   }

   return NS_OK;
}

void nsImageOS2::CleanUp(PRBool aCleanUpAll)
{
   // OS2TODO to handle aCleanUpAll param

   if( mImageBits) {
      delete [] mImageBits; 
      mImageBits = 0;
   }
   if( mInfo) {
      free( mInfo);
      mInfo = 0;
   }
   if( mColorMap) {
      if( mColorMap->Index)
         delete [] mColorMap->Index;
      delete mColorMap;
      mColorMap = 0;
   }
   if( mAlphaBits) {
      delete [] mAlphaBits; 
      mAlphaBits = 0;
   }
   if( mBitmap) {
      GpiDeleteBitmap( mBitmap);
      mBitmap = 0;
   }
   if( mABitmap) {
      GpiDeleteBitmap( mABitmap);
      mABitmap = 0;
   }
}

void nsImageOS2::ImageUpdated( nsIDeviceContext *aContext,
                               PRUint8 aFlags, nsRect *aUpdateRect)
{
   // This is where we can set the bitmap colour table, as the XP code
   // has filled in the colour map.  It would be cute to be able to alias
   // the bitmap colour table as the mColorMap->Index thing, but the formats
   // are unfortunately different.  Rats.

   aContext->GetDepth( mDeviceDepth);

   if( aFlags & nsImageUpdateFlags_kColorMapChanged && mInfo->cBitCount == 8)
   {
      PRGB2 pBmpEntry  = mInfo->argbColor;
      PRUint8 *pMapByte = mColorMap->Index;

      for( PRInt32 i = 0; i < mColorMap->NumColors; i++, pBmpEntry++)
      {
         pBmpEntry->bRed   = *pMapByte++;
         pBmpEntry->bGreen = *pMapByte++;
         pBmpEntry->bBlue  = *pMapByte++;
      }
   }
   else if( aFlags & nsImageUpdateFlags_kBitsChanged)
   {
      // jolly good...
   }
}

nsresult nsImageOS2::Draw( nsIRenderingContext &aContext,
                           nsDrawingSurface aSurface,
                           PRInt32 aX, PRInt32 aY,
                           PRInt32 aWidth, PRInt32 aHeight)
{
   return Draw( aContext, aSurface,
                0, 0, mInfo->cx, mInfo->cy,
                aX, aY, aWidth, aHeight);
}

nsresult nsImageOS2::Draw( nsIRenderingContext &aContext,
                           nsDrawingSurface aSurface,
                           PRInt32 aSX, PRInt32 aSY, PRInt32 aSW, PRInt32 aSH,
                           PRInt32 aDX, PRInt32 aDY, PRInt32 aDW, PRInt32 aDH)
{
   // Find target rect in OS/2 coords.
   nsRect trect( aDX, aDY, aDW, aDH);
   RECTL  rcl;
   ((nsRenderingContextOS2 &)aContext).NS2PM_ININ( trect, rcl); // !! !! !!

   nsDrawingSurfaceOS2 *surf = (nsDrawingSurfaceOS2*) aSurface;

   // Set up blit coord array
   POINTL aptl[ 4] = { { rcl.xLeft, rcl.yBottom },
                       { rcl.xRight, rcl.yTop },
                       { aSX, mInfo->cy - aSY - aSH},
                       { aSX + aSW, mInfo->cy - aSY } };

   // Don't bother creating HBITMAPs, just use the pel data to GpiDrawBits
   // at all times.  This (a) makes printing work 'cos bitmaps are
   //                         device-independent.
   //                     (b) is allegedly more efficient...
   //
#if 0
   if( mBitmap == 0 && mIsOptimized)
   {
      // moz has asked us to optimize this image, but we haven't got
      // round to actually doing it yet.  So do it now.
      CreateBitmaps( surf);
   }
#endif

   if( mAlphaDepth == 0)
   {
      // no transparency, just blit it
      DrawBitmap( surf->mPS, 4, aptl, ROP_SRCCOPY, PR_FALSE);
   }
   else
   {
      // from os2fe/cxdc1.cpp:
      // > the transparent areas of the pixmap are coloured black.
      // > Note this does *not* mean that all black pels are transparent!
      // >
      // > Thus all we need to do is AND the mask onto the target, taking
      // > out pels that are not transparent, and then OR the image onto
      // > the target.
      // >
      // > Note that GPI *ignores* the colour in monochrome bitmaps when
      // > blitting, but uses the actual pel values (indices into cmap)
      // > to do things with.  For 8bpp palette surface, the XP mask is
      // > backwards, so we need a custom ROP.
      // > 
      // > There's probably a really good reason why ROP_SRCAND does the
      // > right thing in true colour...

      long lRop = (mDeviceDepth <= 8) ? ROP_NOTSRCAND : ROP_SRCAND;

      // Apply mask to target, clear pels we will fill in from the image
      DrawBitmap( surf->mPS, 4, aptl, lRop, PR_TRUE);
      // Now combine image with target
      DrawBitmap( surf->mPS, 4, aptl, ROP_SRCPAINT, PR_FALSE);
   }

   return NS_OK;
}

nsresult nsImageOS2::Optimize( nsIDeviceContext* aContext)
{
   // Defer this until we have a PS...
   mIsOptimized = PR_TRUE;
   return NS_OK;
}

// From os2fe/cxdc1.cpp

static RGB2 rgb2White = { 0xff, 0xff, 0xff, 0 };
static RGB2 rgb2Black = { 0, 0, 0, 0 };

struct MASKBMPINFO
{
   BITMAPINFOHEADER2 bmpInfo;
   RGB2              rgbZero;
   RGB2              rgbOne;

   operator PBITMAPINFO2 ()       { return (PBITMAPINFO2) &bmpInfo; }
   operator PBITMAPINFOHEADER2 () { return &bmpInfo; }

   MASKBMPINFO( PBITMAPINFO2 pBI)
   {
      memcpy( &bmpInfo, pBI, sizeof( BITMAPINFOHEADER2));
      bmpInfo.cBitCount = 1;
      rgbZero = rgb2Black;
      rgbOne = rgb2White;
   }
};

void nsImageOS2::CreateBitmaps( nsDrawingSurfaceOS2 *surf)
{
   mBitmap = GpiCreateBitmap( surf->mPS,
                              (PBITMAPINFOHEADER2) mInfo,
                              CBM_INIT,
                              (PBYTE) mImageBits,
                              mInfo);
   if( mBitmap == GPI_ERROR)
      PMERROR("GpiCreateBitmap");

   if( mAlphaBits)
   {
      if( mAlphaDepth == 1)
      {
         MASKBMPINFO maskInfo( mInfo);
         mABitmap = GpiCreateBitmap( surf->mPS,
                                     maskInfo,
                                     CBM_INIT,
                                     (PBYTE) mAlphaBits,
                                     maskInfo);
         if( mABitmap == GPI_ERROR)
            PMERROR( "GpiCreateBitmap (mask)");
      }
      else
         printf( "8 bit alpha mask, no chance...\n");
   }
}

void nsImageOS2::DrawBitmap( HPS hps, LONG lCount, PPOINTL pPoints,
                             LONG lRop, PRBool bIsMask)
{
   HBITMAP hBmp = bIsMask ? mABitmap : mBitmap;

#if 0
   if( hBmp)
   {
      if( GPI_ERROR == GpiWCBitBlt( hps, hBmp, lCount, pPoints, lRop, BBO_OR))
         PMERROR( "GpiWCBitBlt");
   }
   else
#endif
   {
      MASKBMPINFO *pMaskInfo = 0;
      PBITMAPINFO2 pBmp2 = mInfo;

      if( PR_TRUE == bIsMask)
      {
         pMaskInfo = new MASKBMPINFO( pBmp2);
         pBmp2 = *pMaskInfo;
      }

      void *pBits = bIsMask ? mAlphaBits : mImageBits;

      if( GPI_ERROR == GpiDrawBits( hps, pBits, pBmp2,
                                    lCount, pPoints, lRop, BBO_OR))
         PMERROR( "GpiDrawBits");

      delete pMaskInfo;
   }
}

//------------------------------------------------------------
// lock the image pixels. implement this if you need it
NS_IMETHODIMP
nsImageOS2::LockImagePixels(PRBool aMaskPixels)
{
  return NS_OK;
}

//------------------------------------------------------------
// unlock the image pixels. implement this if you need it
NS_IMETHODIMP
nsImageOS2::UnlockImagePixels(PRBool aMaskPixels)
{
  return NS_OK;
} 

// ---------------------------------------------------
//	Set the decoded dimens of the image
//
NS_IMETHODIMP
nsImageOS2::SetDecodedRect(PRInt32 x1, PRInt32 y1, PRInt32 x2, PRInt32 y2 )
{
    
  mDecodedX1 = x1; 
  mDecodedY1 = y1; 
  mDecodedX2 = x2; 
  mDecodedY2 = y2; 
  return NS_OK;
}

PRBool 
nsImageOS2::DrawTile(nsIRenderingContext &aContext, nsDrawingSurface aSurface,
                              nscoord aX0,nscoord aY0,nscoord aX1,nscoord aY1,
                              nscoord aWidth, nscoord aHeight)
{
   PRBool didTile = PR_FALSE;
   LONG tileWidth = aX1-aX0;
   LONG tileHeight = aY1-aY0;

   // Don't bother tiling if we only have to draw the bitmap a couple of times
   if( (aWidth <= tileWidth/2) || (aHeight <= tileHeight/2))
   {
      nsDrawingSurfaceOS2 *surf = (nsDrawingSurfaceOS2*) aSurface;
   
      // Find the compatible device context and create a memory one
      HDC hdcCompat = GpiQueryDevice( surf->mPS);
      HBITMAP hBmp;
      DEVOPENSTRUC dop = { 0, 0, 0, 0, 0 };
      HDC mDC = DevOpenDC( (HAB)0, OD_MEMORY, "*", 5,
                           (PDEVOPENDATA) &dop, hdcCompat);
   
      if( DEV_ERROR != mDC)
      {
         // create the PS
         SIZEL sizel = { 0, 0 };
         HPS mPS = GpiCreatePS( (HAB)0, mDC, &sizel,
                                PU_PELS | GPIT_MICRO | GPIA_ASSOC);
   
         if( GPI_ERROR != mPS)
         {
            // now create a bitmap of the right size
            BITMAPINFOHEADER2 hdr = { 0 };
         
            hdr.cbFix = sizeof( BITMAPINFOHEADER2);
            // Maximum size of tiled area (could do this better)
            LONG endWidth = aWidth * 2;
            while( endWidth < tileWidth)
            {
               endWidth *= 2;
            } 
            LONG endHeight = aHeight * 2;
            while( endHeight < tileHeight)
            {
               endHeight *= 2;
            } 
            hdr.cx = endWidth;
            hdr.cy = endHeight;
            hdr.cPlanes = 1;
   
            // find bitdepth
            LONG lBitCount = 0;
            DevQueryCaps( hdcCompat, CAPS_COLOR_BITCOUNT, 1, &lBitCount);
            hdr.cBitCount = (USHORT) lBitCount;
         
            hBmp = GpiCreateBitmap( mPS, &hdr, 0, 0, 0);
   
            if( GPI_ERROR != hBmp)
            {
               nsRect trect( aX0, aY0, tileWidth, tileHeight);
               RECTL  rcl;
               ((nsRenderingContextOS2 &)aContext).NS2PM_ININ( trect, rcl); // !! !! !!
   
               GpiSetBitmap( mPS, hBmp);
   
               // Set up blit coord array
               POINTL aptl[ 4] = { { 0, 0 },
                                   { aWidth - 1, aHeight - 1 },
                                   { 0, 0 },
                                   { mInfo->cx, mInfo->cy } };
   
               // Draw bitmap once into temporary PS
               DrawBitmap( mPS, 4, aptl, ROP_SRCCOPY, PR_FALSE);
            
               // Copy bitmap horizontally, doubling each time
               while( aWidth < tileWidth)
               {
                  POINTL aptlCopy[ 4] = { { aWidth, 0 },
                                          { 2*aWidth, aHeight },
                                          { 0, 0 },
                                          { aWidth, aHeight } };
            
               
                  GpiBitBlt( mPS, mPS, 4, aptlCopy, ROP_SRCCOPY, 0L);
                  aWidth *= 2;
               } 
               // Copy bitmap vertically, doubling each time
               while( aHeight < tileHeight)
               {
                  POINTL aptlCopy[ 4] = { { 0, aHeight },
                                          { aWidth, 2*aHeight },
                                          { 0, 0 },
                                          { aWidth, aHeight } };
               
                  GpiBitBlt( mPS, mPS, 4, aptlCopy, ROP_SRCCOPY, 0L);
                  aHeight *= 2;
               } 
   
               POINTL aptlTile[ 4] = { { rcl.xLeft, rcl.yBottom },
                                       { rcl.xRight + 1, rcl.yTop + 1 },
                                       { 0, aHeight - tileHeight },
                                       { tileWidth, aHeight } };
   
               // Copy tiled bitmap into destination PS
               if( mAlphaDepth == 0)
               {
                  // no transparency, just blit it
                  GpiBitBlt( surf->mPS, mPS, 4, aptlTile, ROP_SRCCOPY, 0L);
               }
               else
               {
                  // For some reason, only ROP_NOTSRCAND seems to work here....
//                  long lRop = (mDeviceDepth <= 8) ? ROP_NOTSRCAND : ROP_SRCAND;
                  long lRop = ROP_NOTSRCAND;

                  // Apply mask to target, clear pels we will fill in from the image
                  GpiBitBlt( surf->mPS, mPS, 4, aptlTile, lRop, 0L);
                  // Now combine image with target
                  GpiBitBlt( surf->mPS, mPS, 4, aptlTile, ROP_SRCPAINT, 0L);
               }
   
               // Tiling successful
               didTile = PR_TRUE;
   
               // Must deselect bitmap from PS before freeing bitmap and PS.
               GpiSetBitmap( mPS, NULLHANDLE);
               GpiDeleteBitmap( hBmp);
            }
            GpiDestroyPS( mPS);
         }
         DevCloseDC( mDC);
      }
   }

   // If we failed to tile the bitmap, then use the old, slow, reliable way
   if( didTile == PR_FALSE)
   {
      PRInt32 x,y;

      for(y=aY0;y<aY1;y+=aHeight){
         for(x=aX0;x<aX1;x+=aWidth){
            this->Draw(aContext,aSurface,x,y,aWidth,aHeight);
         }
      } 
   }

   return(PR_TRUE);
}

