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
         mAlphaDepth = 1;
      }
      else
      {
         NS_ASSERTION( nsMaskRequirements_kNeeds8Bit == aMaskRequirements,
                       "unexpected mask depth");
         mAlphaDepth = 8;
      }

      // 32-bit align each row
      mARowBytes = RASWIDTH (aWidth, mAlphaDepth);

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
      GFX (::GpiDeleteBitmap (mBitmap), FALSE);
      mBitmap = 0;
   }
   if( mABitmap) {
      GFX (::GpiDeleteBitmap (mABitmap), FALSE);
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
   if (aDW == 0 || aDH == 0 || aSW == 0 || aSH == 0)  // Nothing to draw
      return NS_OK;

   // Find target rect in OS/2 coords.
   nsRect trect( aDX, aDY, aDW, aDH);
   RECTL  rcl;
   ((nsRenderingContextOS2 &)aContext).NS2PM_ININ( trect, rcl); // !! !! !!

   nsDrawingSurfaceOS2 *surf = (nsDrawingSurfaceOS2*) aSurface;

   // limit the size of the blit to the amount of the image read in
   PRInt32 notLoadedDY = 0;
   if( mDecodedY2 < mInfo->cy)
   {
      notLoadedDY = aSH - PRInt32(float(mDecodedY2/float(mInfo->cy))*aSH);
   }

   // Set up blit coord array
   POINTL aptl[ 4] = { { rcl.xLeft, rcl.yBottom + notLoadedDY },
                       { rcl.xRight, rcl.yTop },
                       { aSX, mInfo->cy - aSY - aSH + notLoadedDY },
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
      DrawBitmap( surf->mPS, aptl, ROP_SRCCOPY, PR_FALSE);
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
      // > For monochrome bitmaps GPI replaces 1 with IMAGEBUNDLE foreground
      // > color and 0 with background color. 

      // Apply mask to target, clear pels we will fill in from the image
      DrawBitmap( surf->mPS, aptl, ROP_NOTSRCAND, PR_TRUE);
//      DrawBitmap( surf->mPS, aptl, ROP_SRCAND, PR_TRUE);
      // Now combine image with target
      DrawBitmap( surf->mPS, aptl, ROP_SRCPAINT, PR_FALSE);
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
   mBitmap = GFX (::GpiCreateBitmap (surf->mPS, (PBITMAPINFOHEADER2)mInfo,
                                     CBM_INIT, (PBYTE)mImageBits, mInfo),
                  GPI_ERROR);

   if( mAlphaBits)
   {
      if( mAlphaDepth == 1)
      {
         MASKBMPINFO maskInfo( mInfo);
         mABitmap = GFX (::GpiCreateBitmap (surf->mPS, maskInfo, CBM_INIT,
                                            (PBYTE)mAlphaBits, maskInfo),
                         GPI_ERROR);
      }
      else
         printf( "8 bit alpha mask, no chance...\n");
   }
}

void nsImageOS2::DrawBitmap (HPS hps, PPOINTL pPoints, LONG lRop, PRBool bIsMask)
{
   if (bIsMask)
   {
      MASKBMPINFO MaskBitmapInfo (mInfo);

      GFX (::GpiDrawBits (hps, mAlphaBits, MaskBitmapInfo, 4, pPoints, lRop, BBO_OR), GPI_ERROR);
   } else
   {
      GFX (::GpiDrawBits (hps, mImageBits, mInfo, 4, pPoints, lRop, BBO_OR), GPI_ERROR);
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

nscoord
nsImageOS2::BuildTile(HPS hpsTile, HBITMAP hBmpTile,
                              nscoord aWidth, nscoord aHeight,
                              LONG tileWidth, LONG tileHeight, PRBool bMsk)
{
   GFX (::GpiSetBitmap (hpsTile, hBmpTile), HBM_ERROR);
   PRInt32 notLoadedDY = 0;
   if( mDecodedY2 < mInfo->cy)
   {
      // If bitmap not fully loaded, fill unloaded area
      notLoadedDY = mInfo->cy - mDecodedY2;
      RECTL rect = { 0, 0, aWidth, notLoadedDY };
      ::WinFillRect( hpsTile, &rect, CLR_BACKGROUND);
   }

   // Set up blit coord array
   POINTL aptl[ 4] = { { 0, notLoadedDY },
                       { aWidth - 1, aHeight - 1 },
                       { 0, notLoadedDY },
                       { mInfo->cx, mInfo->cy } };

   // Draw bitmap once into temporary PS
   DrawBitmap( hpsTile, aptl, ROP_SRCCOPY, bMsk);

   // Copy bitmap horizontally, doubling each time
   while( aWidth < tileWidth)
   {
      POINTL aptlCopy[ 4] = { { aWidth, 0 },
                              { 2*aWidth, aHeight },
                              { 0, 0 },
                              { aWidth, aHeight } };

      GFX (::GpiBitBlt (hpsTile, hpsTile, 4, aptlCopy, ROP_SRCCOPY, 0L), GPI_ERROR);
      aWidth *= 2;
   }
   // Copy bitmap vertically, doubling each time
   while( aHeight < tileHeight)
   {
      POINTL aptlCopy[ 4] = { { 0, aHeight },
                              { aWidth, 2*aHeight },
                              { 0, 0 },
                              { aWidth, aHeight } };

      GFX (::GpiBitBlt (hpsTile, hpsTile, 4, aptlCopy, ROP_SRCCOPY, 0L), GPI_ERROR);
      aHeight *= 2;
   }

   return aHeight;
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
      HDC hdcCompat = GFX (::GpiQueryDevice (surf->mPS), HDC_ERROR);

      DEVOPENSTRUC dop = { 0, 0, 0, 0, 0 };
      HDC MemDC = ::DevOpenDC( (HAB)0, OD_MEMORY, "*", 5,
                               (PDEVOPENDATA) &dop, hdcCompat);

      if( DEV_ERROR != MemDC)
      {
         // create the PS
         SIZEL sizel = { 0, 0 };
         HPS MemPS = GFX (::GpiCreatePS (0, MemDC, &sizel,
                                         PU_PELS | GPIT_MICRO | GPIA_ASSOC),
                          GPI_ERROR);

         // Set image foreground and background colors. These are used in transparent images for blitting 1-bit masks.
         IMAGEBUNDLE ib;
         ib.lColor     = GFX (::GpiQueryColorIndex (MemPS, 0, MK_RGB (255, 255, 255)), GPI_ALTERROR);  // 1 in monochrome image maps to white
         ib.lBackColor = GFX (::GpiQueryColorIndex (MemPS, 0, MK_RGB (0, 0, 0)), GPI_ALTERROR);        // 0 in monochrome image maps to black
         ib.usMixMode  = FM_OVERPAINT;
         ib.usBackMixMode = BM_OVERPAINT;
         GFX (::GpiSetAttrs (MemPS, PRIM_IMAGE, IBB_COLOR | IBB_BACK_COLOR | IBB_MIX_MODE | IBB_BACK_MIX_MODE, 0, (PBUNDLE)&ib), FALSE);

         if( GPI_ERROR != MemPS)
         {
            // now create a bitmap of the right size
            HBITMAP hBmp;
            HBITMAP hBmpMask = 0;
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
            GFX (::DevQueryCaps( hdcCompat, CAPS_COLOR_BITCOUNT, 1, &lBitCount), FALSE);
            hdr.cBitCount = (USHORT) lBitCount;

            hBmp = GFX (::GpiCreateBitmap (MemPS, &hdr, 0, 0, 0), GPI_ERROR);

            if( GPI_ERROR != hBmp)
            {
               if (mAlphaDepth != 0)
               {
                  hBmpMask = GFX (::GpiCreateBitmap (MemPS, &hdr, 0, 0, 0), GPI_ERROR);
               }

               nsRect trect( aX0, aY0, tileWidth, tileHeight);
               RECTL  rcl;
               ((nsRenderingContextOS2 &)aContext).NS2PM_ININ( trect, rcl); // !! !! !!

               nscoord nHeight = BuildTile( MemPS, hBmp, aWidth, aHeight,
                                            tileWidth, tileHeight, PR_FALSE );
               if( hBmpMask)
               {
                  BuildTile( MemPS, hBmpMask, aWidth, aHeight,
                             tileWidth, tileHeight, PR_TRUE );
               }

               POINTL aptlTile[ 4] = { { rcl.xLeft, rcl.yBottom },
                                       { rcl.xRight + 1, rcl.yTop + 1 },
                                       { 0, nHeight - tileHeight },
                                       { tileWidth, nHeight } };

               // Copy tiled bitmap into destination PS
               if( mAlphaDepth == 0)
               {
                  // no transparency, just blit it
                  GFX (::GpiBitBlt (surf->mPS, MemPS, 4, aptlTile, ROP_SRCCOPY, 0L), GPI_ERROR);
               }
               else
               {
                  // Apply mask to target, clear pels we will fill in from the image
//                  GFX (::GpiSetBitmap (MemPS, hBmpMask), HBM_ERROR);
                  GFX (::GpiBitBlt (surf->mPS, MemPS, 4, aptlTile, ROP_NOTSRCAND, 0L), GPI_ERROR);
                  // Now combine image with target
                  GFX (::GpiSetBitmap (MemPS, hBmp), HBM_ERROR);
                  GFX (::GpiBitBlt (surf->mPS, MemPS, 4, aptlTile, ROP_SRCPAINT, 0L), GPI_ERROR);
               }

               // Tiling successful
               didTile = PR_TRUE;

               // Must deselect bitmap from PS before freeing bitmap and PS.
               GFX (::GpiSetBitmap (MemPS, NULLHANDLE), HBM_ERROR);
               GFX (::GpiDeleteBitmap (hBmp), FALSE);

               if ( hBmpMask)
               {
                  GFX (::GpiDeleteBitmap (hBmpMask), FALSE);
               }
            }
            GFX (::GpiDestroyPS (MemPS), FALSE);
         }
         ::DevCloseDC (MemDC);
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

