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


// Max size of image that could be tiled
#define MAX_BUFFER_WIDTH   200        // For screen resolution 1600x1200
#define MAX_BUFFER_HEIGHT  150        //   3 x 3 = 9 times


struct MONOBITMAPINFO
{
   BITMAPINFOHEADER2 bmpInfo;
   RGB2 argbColor [2];

   operator PBITMAPINFO2 ()       { return (PBITMAPINFO2) &bmpInfo; }
   operator PBITMAPINFOHEADER2 () { return &bmpInfo; }

   MONOBITMAPINFO( PBITMAPINFO2 pBI)
   {
      memcpy( &bmpInfo, pBI, sizeof( BITMAPINFOHEADER2));
      bmpInfo.cBitCount = 1;

      argbColor [0].bRed      = 0;
      argbColor [0].bGreen    = 0;
      argbColor [0].bBlue     = 0;
      argbColor [0].fcOptions = 0;
      argbColor [1].bRed      = 255;
      argbColor [1].bGreen    = 255;
      argbColor [1].bBlue     = 255;
      argbColor [1].fcOptions = 0;
   }
};


PRUint8 nsImageOS2::gBlenderLookup [65536];    // Global table for fast alpha blending
PRBool  nsImageOS2::gBlenderReady = PR_FALSE;



NS_IMPL_ISUPPORTS1(nsImageOS2, nsIImage)

//------------------------------------------------------------
nsImageOS2::nsImageOS2()
{
   NS_INIT_REFCNT();
    
   mInfo      = 0;
   mRowBytes  = 0;
   mImageBits = 0;

   mARowBytes  = 0;
   mAlphaBits  = 0;
   mAlphaDepth = 0;
   mAlphaLevel = 0;

   mColorMap = 0;

   mIsOptimized = PR_FALSE;
   mIsTopToBottom = PR_FALSE;   

   mDeviceDepth = 0;
   mNaturalWidth = 0;
   mNaturalHeight = 0;

   if (gBlenderReady != PR_TRUE)
     BuildBlenderLookup ();
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
}

void nsImageOS2::ImageUpdated( nsIDeviceContext *aContext,
                               PRUint8 aFlags, nsRect *aUpdateRect)
{
   if (!aContext) {
      return;
   } /* endif */
   // This is where we can set the bitmap colour table, as the XP code
   // has filled in the colour map.  It would be cute to be able to alias
   // the bitmap colour table as the mColorMap->Index thing, but the formats
   // are unfortunately different.  Rats.

   aContext->GetDepth( mDeviceDepth);

   if( (aFlags & nsImageUpdateFlags_kColorMapChanged) && mInfo->cBitCount == 8)
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

void nsImageOS2::BuildBlenderLookup (void)
{
  for (int y = 0 ; y < 256 ; y++)
    for (int x = 0 ; x < 256 ; x++)
      gBlenderLookup [y * 256 + x] = y * x / 255;

  gBlenderReady = PR_TRUE;
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

NS_IMETHODIMP 
nsImageOS2 :: Draw(nsIRenderingContext &aContext, nsDrawingSurface aSurface,
                  PRInt32 aSX, PRInt32 aSY, PRInt32 aSWidth, PRInt32 aSHeight,
                  PRInt32 aDX, PRInt32 aDY, PRInt32 aDWidth, PRInt32 aDHeight)
{
   PRInt32 origSHeight = aSHeight, origDHeight = aDHeight;
   PRInt32 origSWidth = aSWidth, origDWidth = aDWidth;

   if (mInfo == nsnull || aSWidth < 0 || aDWidth < 0 || aSHeight < 0 || aDHeight < 0) 
      return NS_ERROR_FAILURE;

   if (0 == aSWidth || 0 == aDWidth || 0 == aSHeight || 0 == aDHeight)
      return NS_OK;

   // limit the size of the blit to the amount of the image read in
   PRInt32 aSX2 = aSX + aSWidth;

   if (aSX2 > mDecodedX2) 
      aSX2 = mDecodedX2;

   if (aSX < mDecodedX1) {
      aDX += (mDecodedX1 - aSX) * origDWidth / origSWidth;
      aSX = mDecodedX1;
   }
  
   aSWidth = aSX2 - aSX;
   aDWidth -= (origSWidth - aSWidth) * origDWidth / origSWidth;
  
   if (aSWidth <= 0 || aDWidth <= 0)
      return NS_OK;

   PRInt32 aSY2 = aSY + aSHeight;

   if (aSY2 > mDecodedY2)
      aSY2 = mDecodedY2;

   if (aSY < mDecodedY1) {
      aDY += (mDecodedY1 - aSY) * origDHeight / origSHeight;
      aSY = mDecodedY1;
   }

   aSHeight = aSY2 - aSY;
   aDHeight -= (origSHeight - aSHeight) * origDHeight / origSHeight;

   if (aSHeight <= 0 || aDHeight <= 0)
      return NS_OK;

   nsDrawingSurfaceOS2 *surf = (nsDrawingSurfaceOS2*) aSurface;
 

   nsRect trect( aDX, aDY, aDWidth, aDHeight);
   RECTL  rcl;
   surf->NS2PM_ININ (trect, rcl);

   // Set up blit coord array
   POINTL aptl[ 4] = { { rcl.xLeft, rcl.yBottom },              // TLL
                       { rcl.xRight, rcl.yTop },                // TUR
                       { aSX, mInfo->cy - (aSY + aSHeight) },   // SLL
                       { aSX + aSWidth, mInfo->cy - aSY } };    // SUR

   if( mAlphaDepth == 0)
   {
      // no transparency, just blit it
      GFX (::GpiDrawBits (surf->GetPS (), mImageBits, mInfo, 4, aptl, ROP_SRCCOPY, BBO_IGNORE), GPI_ERROR);
   }
   else if( mAlphaDepth == 1)
   {
      // > The transparent areas of the mask are coloured black (0).
      // > the transparent areas of the pixmap are coloured black (0).
      // > Note this does *not* mean that all black pels are transparent!
      // >
      // > Thus all we need to do is AND the mask onto the target, taking
      // > out pels that are not transparent, and then OR the image onto
      // > the target.
      // >
      // > For monochrome bitmaps GPI replaces 1 with IMAGEBUNDLE foreground
      // > color and 0 with background color. To make this work with ROP_SRCAND
      // > we set foreground to black and background to white. Thus AND with 
      // > 1 (opaque) in mask maps to AND with IMAGEBUNDLE foreground (which is 0)
      // > always gives 0 - clears opaque region to zeros. 

      // Apply mask to target, clear pels we will fill in from the image
      MONOBITMAPINFO MaskBitmapInfo (mInfo);
      GFX (::GpiDrawBits (surf->GetPS (), mAlphaBits, MaskBitmapInfo, 4, aptl, ROP_SRCAND, BBO_IGNORE), GPI_ERROR);

      // Now combine image with target
      GFX (::GpiDrawBits (surf->GetPS (), mImageBits, mInfo, 4, aptl, ROP_SRCPAINT, BBO_IGNORE), GPI_ERROR);
   } else
   {
      // Find the compatible device context and create a memory one
      HDC hdcCompat = GFX (::GpiQueryDevice (surf->GetPS ()), HDC_ERROR);

//DJ - This is temporary hack. Printer drivers can't get info that is already drawn on drawing surface.
LONG Technology = 0;
::DevQueryCaps (hdcCompat, CAPS_TECHNOLOGY, 1, &Technology);
  
if (Technology == CAPS_TECH_RASTER_DISPLAY)
{
//DJ - End of temporary hack

      DEVOPENSTRUC dop = { 0, 0, 0, 0, 0 };
      HDC MemDC = ::DevOpenDC( (HAB)0, OD_MEMORY, "*", 5, (PDEVOPENDATA) &dop, hdcCompat);

      if (DEV_ERROR != MemDC)
      {
         // create the PS
         SIZEL sizel = { 0, 0 };
         HPS MemPS = GFX (::GpiCreatePS (0, MemDC, &sizel, PU_PELS | GPIT_MICRO | GPIA_ASSOC), GPI_ERROR);

         if (GPI_ERROR != MemPS)
         {
            // now create a bitmap of the right size
            HBITMAP hMemBmp;
            BITMAPINFOHEADER2 bihMem = { 0 };

            bihMem.cbFix = sizeof (BITMAPINFOHEADER2);
            bihMem.cx = rcl.xRight - rcl.xLeft + 1;
            bihMem.cy = rcl.yTop - rcl.yBottom + 1;
            bihMem.cPlanes = 1;
            LONG lBitCount = 0;
            GFX (::DevQueryCaps( hdcCompat, CAPS_COLOR_BITCOUNT, 1, &lBitCount), FALSE);
            bihMem.cBitCount = (USHORT) lBitCount;

            hMemBmp = GFX (::GpiCreateBitmap (MemPS, &bihMem, 0, 0, 0), GPI_ERROR);

            if (GPI_ERROR != hMemBmp)
            {
               GFX (::GpiSetBitmap (MemPS, hMemBmp), HBM_ERROR);

               POINTL aptlDevToMem [3] = { 0, 0,                                    // TLL - mem bitmap (0, 0)
                                           bihMem.cx, bihMem.cy,                    // TUR - mem bitmap (cx, cy)
                                           rcl.xLeft, rcl.yBottom };                // SLL - device (Dx1, Dy2)

               GFX (::GpiBitBlt (MemPS, surf->GetPS (), 3, aptlDevToMem, ROP_SRCCOPY, BBO_IGNORE), GPI_ERROR);

               // Now we want direct access to bitmap raw data. 
               // Must copy data again because GpiSetBitmap doesn't provide pointer to start of raw bit data ?? (DJ)
               
               BITMAPINFOHEADER2 bihDirect = { 0 };
               bihDirect.cbFix   = sizeof (BITMAPINFOHEADER2);
               bihDirect.cPlanes = 1;
               bihDirect.cBitCount = 24;

               int RawDataSize = bihMem.cy * RASWIDTH (bihMem.cx, 24);
               PRUint8* pRawBitData = (PRUint8*)malloc (RawDataSize);

               if (pRawBitData)
               {
                 ULONG rc = GFX (::GpiQueryBitmapBits (MemPS, 0, bihMem.cy, (PBYTE)pRawBitData, (PBITMAPINFO2)&bihDirect), GPI_ALTERROR);

                 if (rc != GPI_ALTERROR)
                 {
                   // Do manual alpha blending
                   PRUint8* pDest  = pRawBitData;
                   PRUint8* pSrc   = mImageBits;
                   PRUint8* pAlpha = mAlphaBits;

                   if (mInfo->cBitCount == 8)
                   {
                     for (int y = 0 ; y < bihDirect.cy ; y++)
                     {
                       for (int x = 0 ; x < bihDirect.cx ; x++)
                       {
                         pDest [0] = FAST_BLEND (mColorMap->Index [3 * (*pSrc) + 0], pDest [0], *pAlpha);
                         pDest [1] = FAST_BLEND (mColorMap->Index [3 * (*pSrc) + 1], pDest [1], *pAlpha);
                         pDest [2] = FAST_BLEND (mColorMap->Index [3 * (*pSrc) + 2], pDest [2], *pAlpha);
                         pSrc++;
                         pDest += 3;
                         pAlpha++;
                       }

                       pSrc   = (PRUint8*) ((ULONG)(pSrc + 3) & ~3);    // For next scanline align pointers on DWORD boundary
                       pDest  = (PRUint8*) ((ULONG)(pDest + 3) & ~3);
                       pAlpha = (PRUint8*) ((ULONG)(pAlpha + 3) & ~3);
                     }
                   } else
                   {
                     for (int y = 0 ; y < bihDirect.cy ; y++)
                     {
                       for (int x = 0 ; x < bihDirect.cx ; x++)
                       {
                         pDest [0] = FAST_BLEND (pSrc [0], pDest [0], *pAlpha);
                         pDest [1] = FAST_BLEND (pSrc [1], pDest [1], *pAlpha);
                         pDest [2] = FAST_BLEND (pSrc [2], pDest [2], *pAlpha);
                         pSrc  +=3;
                         pDest += 3;
                         pAlpha++;
                       }

                       pSrc   = (PRUint8*) ((ULONG)(pSrc + 3) & ~3);    // For next scanline align pointers on DWORD boundary
                       pDest  = (PRUint8*) ((ULONG)(pDest + 3) & ~3);
                       pAlpha = (PRUint8*) ((ULONG)(pAlpha + 3) & ~3);
                     }
                   }
                   // Copy modified memory back to memory bitmap
                   GFX (::GpiSetBitmapBits (MemPS, 0, bihMem.cy, (PBYTE)pRawBitData, (PBITMAPINFO2)&bihDirect), GPI_ALTERROR);

                   // Transfer bitmap from memory bitmap back to device
                   POINTL aptlMemToDev [3] = { rcl.xLeft, rcl.yBottom,                  // TLL - device (Dx1, Dy2)
                                               rcl.xRight, rcl.yTop,                    // TUR - device (Dx2, Dy1)
                                               0, 0 };                                  // SLL - mem bitmap (0, 0)

                   GFX (::GpiBitBlt (surf->GetPS (), MemPS, 3, aptlMemToDev, ROP_SRCCOPY, BBO_IGNORE), GPI_ERROR);
                 }

                 free (pRawBitData);
               }

               GFX (::GpiSetBitmap (MemPS, NULLHANDLE), HBM_ERROR);
               GFX (::GpiDeleteBitmap (hMemBmp), FALSE);
            }

            GFX (::GpiDestroyPS (MemPS), FALSE);
         }
         ::DevCloseDC (MemDC);
      }

//DJ - temporary hack
} else  // This is not display driver. Can't get info that is already drawn. Output only image without mask.
{
  GFX (::GpiDrawBits (surf->GetPS (), mImageBits, mInfo, 4, aptl, ROP_SRCCOPY, BBO_IGNORE), GPI_ERROR);
}
//DJ - end of temporary hack

   }

   return NS_OK;
}

nsresult nsImageOS2::Optimize( nsIDeviceContext* aContext)
{
   // Defer this until we have a PS...
   mIsOptimized = PR_TRUE;
   return NS_OK;
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

void
nsImageOS2::BuildTile (HPS hpsTile, PRUint8* pImageBits, PBITMAPINFO2 pBitmapInfo,
                       nscoord aWidth, nscoord aHeight,
                       LONG tileWidth, LONG tileHeight)
{
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
   GFX (::GpiDrawBits (hpsTile, (PBYTE)pImageBits, pBitmapInfo, 4, aptl, ROP_SRCCOPY, BBO_IGNORE), GPI_ERROR);

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
}

#ifdef PAVLOV_REALLY_SUCKS
PRBool 
nsImageOS2::DrawTile(nsIRenderingContext &aContext, nsDrawingSurface aSurface,
                              nscoord aX0,nscoord aY0,nscoord aX1,nscoord aY1,
                              nscoord aWidth, nscoord aHeight)
{
   PRBool didTile = PR_FALSE;
   LONG tileWidth = aX1-aX0;
   LONG tileHeight = aY1-aY0;

   // Don't bother tiling if we only have to draw the bitmap a couple of times
   // Can't tile with 8bit alpha masks because need access destination bitmap values
   if( (aWidth > tileWidth / 2) || (aHeight > tileHeight / 2) ||
       (aWidth > MAX_BUFFER_WIDTH) || (aHeight > MAX_BUFFER_HEIGHT) ||
        mAlphaDepth > 1)
      return SlowTile (aContext, aSurface, aX0, aY0, aX1, aY1, aWidth, aHeight);   


   nsDrawingSurfaceOS2 *surf = (nsDrawingSurfaceOS2*) aSurface;

   // Find the compatible device context and create a memory one
   HDC hdcCompat = GFX (::GpiQueryDevice (surf->GetPS ()), HDC_ERROR);

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

      if( GPI_ERROR != MemPS)
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
         GFX (::DevQueryCaps( hdcCompat, CAPS_COLOR_BITCOUNT, 1, &lBitCount), FALSE);
         hdr.cBitCount = (USHORT) lBitCount;

         nsRect trect( aX0, aY0, tileWidth, tileHeight);
         RECTL  rcl;
         surf->NS2PM_ININ (trect, rcl); // !! !! !!

         POINTL aptlTile[ 4] = { { rcl.xLeft, rcl.yBottom },
                                 { rcl.xRight + 1, rcl.yTop + 1 },
                                 { 0, endHeight - tileHeight },
                                 { tileWidth, endHeight } };

         HBITMAP hMemBmp = GFX (::GpiCreateBitmap (MemPS, &hdr, 0, 0, 0), GPI_ERROR);
         if (hMemBmp != GPI_ERROR)
         {
            LONG ImageROP = ROP_SRCCOPY;

            GFX (::GpiSetBitmap (MemPS, hMemBmp), HBM_ERROR);

            if (mAlphaDepth == 1)
            {
               LONG BlackColor = GFX (::GpiQueryColorIndex (MemPS, 0, MK_RGB (0x00, 0x00, 0x00)), GPI_ALTERROR);    // CLR_BLACK;
               LONG WhiteColor = GFX (::GpiQueryColorIndex (MemPS, 0, MK_RGB (0xFF, 0xFF, 0xFF)), GPI_ALTERROR);    // CLR_WHITE;

               // WORKAROUND:
               // PostScript drivers up to version 30.732 have problems with GpiQueryColorIndex.
               // If we are in LCOL_RGB mode it doesn't return passed RGB color but returns error instead.

               if (BlackColor == GPI_ALTERROR) BlackColor = MK_RGB (0x00, 0x00, 0x00);
               if (WhiteColor == GPI_ALTERROR) WhiteColor = MK_RGB (0xFF, 0xFF, 0xFF);

               // Set image foreground and background colors. These are used in transparent images for blitting 1-bit masks.
               // To invert colors on ROP_SRCAND we map 1 to black and 0 to white
               IMAGEBUNDLE ib;
               ib.lColor     = BlackColor;        // map 1 in mask to 0x000000 (black) in destination
               ib.lBackColor = WhiteColor;        // map 0 in mask to 0xFFFFFF (white) in destination
               ib.usMixMode  = FM_OVERPAINT;
               ib.usBackMixMode = BM_OVERPAINT;
               GFX (::GpiSetAttrs (MemPS, PRIM_IMAGE, IBB_COLOR | IBB_BACK_COLOR | IBB_MIX_MODE | IBB_BACK_MIX_MODE, 0, (PBUNDLE)&ib), FALSE);


               MONOBITMAPINFO MaskBitmapInfo (mInfo);
               BuildTile (MemPS, mAlphaBits, MaskBitmapInfo, aWidth, aHeight, tileWidth, tileHeight);

               // Apply mask to target, clear pels we will fill in from the image
               GFX (::GpiBitBlt (surf->GetPS (), MemPS, 4, aptlTile, ROP_SRCAND, 0L), GPI_ERROR);

               ImageROP = ROP_SRCPAINT;    // Original image must be combined with mask
            }


            BuildTile (MemPS, mImageBits, mInfo, aWidth, aHeight, tileWidth, tileHeight);

            GFX (::GpiBitBlt (surf->GetPS (), MemPS, 4, aptlTile, ImageROP, 0L), GPI_ERROR);

            didTile = PR_TRUE;

            // Must deselect bitmap from PS before freeing bitmap and PS.
            GFX (::GpiSetBitmap (MemPS, NULLHANDLE), HBM_ERROR);
            GFX (::GpiDeleteBitmap (hMemBmp), FALSE);
         }
         GFX (::GpiDestroyPS (MemPS), FALSE);
      }
      ::DevCloseDC (MemDC);
   }

   // If we failed to tile the bitmap, then use the old, slow, reliable way
   if( didTile == PR_FALSE)
      SlowTile (aContext, aSurface, aX0, aY0, aX1, aY1, aWidth, aHeight);

   return(PR_TRUE);
}
#else
/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *  @update 3/16/00 dwc
 */
NS_IMETHODIMP nsImageOS2::DrawTile(nsIRenderingContext &aContext,
                                   nsDrawingSurface aSurface,
                                   PRInt32 aSXOffset, PRInt32 aSYOffset,
                                   const nsRect &aTileRect)
{
   return(PR_FALSE);
}
#endif

PRBool nsImageOS2::SlowTile (nsIRenderingContext& aContext, nsDrawingSurface aSurface, 
                             nscoord aX0, nscoord aY0, nscoord aX1, nscoord aY1, 
                             nscoord aWidth, nscoord aHeight)
{
  for (int y = aY0 ; y < aY1 ; y += aHeight)
     for (int x = aX0 ; x < aX1 ; x += aWidth)
        Draw (aContext, aSurface, x, y, aWidth, aHeight);

  return PR_TRUE;
}

#ifdef USE_IMG2
void nsImageOS2::NS2PM_ININ( const nsRect &in, RECTL &rcl)
{
  PRUint32 ulHeight = GetHeight ();

  rcl.xLeft = in.x;
  rcl.xRight = in.x + in.width - 1;
  rcl.yTop = ulHeight - in.y - 1;
  rcl.yBottom = rcl.yTop - in.height + 1;
}


// ---------------------------------------------------
//	Update mImageBits with content of new mBitmap
//
NS_IMETHODIMP nsImageOS2::UpdateImageBits( HPS aPS )
{
  BITMAPINFOHEADER2 rawInfo = { 0 };
  rawInfo.cbFix   = sizeof (BITMAPINFOHEADER2);
  rawInfo.cPlanes = 1;
  rawInfo.cBitCount = mInfo->cBitCount;

  int RawDataSize = mInfo->cy * RASWIDTH (mInfo->cx, mInfo->cBitCount);
  PRUint8* pRawBitData = new PRUint8 [RawDataSize];

  if (pRawBitData)
  {
    GFX (::GpiQueryBitmapBits (aPS, 0, mInfo->cy, (PBYTE)pRawBitData, (PBITMAPINFO2)&rawInfo), GPI_ALTERROR);
    delete [] mImageBits;
    mImageBits = pRawBitData;
    return NS_OK;
  }
  else
    return NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP nsImageOS2::DrawToImage(nsIImage* aDstImage,
                                      nscoord aDX, nscoord aDY,
                                      nscoord aDWidth, nscoord aDHeight)
{
  nsresult rc = NS_OK;

  DEVOPENSTRUC dop = { 0, 0, 0, 0, 0 };
  SIZEL sizel = { 0, 0 };

  if (mInfo == nsnull || aDWidth < 0 || aDHeight < 0) 
    return NS_ERROR_FAILURE;

  if (0 == aDWidth || 0 == aDHeight)
    return NS_OK;

  // Create a memory DC that is compatible with the screen
  HDC MemDC = GFX (::DevOpenDC( 0/*hab*/, OD_MEMORY, "*", 5,
                   (PDEVOPENDATA) &dop, (HDC)0), DEV_ERROR);

  // create the PS
  HPS MemPS = GFX (::GpiCreatePS (0/*hab*/, MemDC, &sizel,
                                  PU_PELS | GPIT_MICRO | GPIA_ASSOC), GPI_ERROR);

  nsImageOS2* destImg = NS_STATIC_CAST(nsImageOS2*, aDstImage); 

  HBITMAP hTmpBitmap = GFX (::GpiCreateBitmap (MemPS, (PBITMAPINFOHEADER2)destImg->mInfo,
                                               CBM_INIT, (PBYTE)destImg->mImageBits,
                                               destImg->mInfo), GPI_ERROR);
  GFX (::GpiSetBitmap (MemPS, hTmpBitmap), HBM_ERROR);
  
  nsRect trect( aDX, aDY, aDWidth, aDHeight);
  RECTL  rcl;
  destImg->NS2PM_ININ (trect, rcl);

  // Set up blit coord array
  POINTL aptl[ 4] = { { rcl.xLeft, rcl.yBottom },
                      { rcl.xRight, rcl.yTop },
                      { 0, mInfo->cy - mNaturalHeight },
                      { mNaturalWidth, mInfo->cy } };

  if( 1==mAlphaDepth && mAlphaBits){
    // Apply mask to target, clear pels we will fill in from the image
    MONOBITMAPINFO MaskBitmapInfo (mInfo);
    GFX (::GpiDrawBits (MemPS, mAlphaBits, MaskBitmapInfo, 4, aptl, ROP_SRCAND,
                        BBO_IGNORE), GPI_ERROR);

    // Now combine image with target
    GFX (::GpiDrawBits (MemPS, mImageBits, mInfo, 4, aptl, ROP_SRCPAINT, 
                        BBO_IGNORE), GPI_ERROR);
  } else {
    // alpha depth of 8 not used (yet?)
    NS_ASSERTION( mAlphaDepth != 8, "Alpha depth of 8 not implemented in DrawToImage" );

    // no transparency, just blit it
    GFX (::GpiDrawBits (MemPS, mImageBits, mInfo, 4, aptl, ROP_SRCCOPY,
                        BBO_IGNORE), GPI_ERROR);
  }

  rc = destImg->UpdateImageBits (MemPS);

  GFX (::GpiSetBitmap (MemPS, NULLHANDLE), HBM_ERROR);
  GFX (::GpiDeleteBitmap (hTmpBitmap), FALSE);
  GFX (::GpiDestroyPS (MemPS), FALSE);
  GFX (::DevCloseDC (MemDC), DEV_ERROR);

  return rc;
}
#endif
