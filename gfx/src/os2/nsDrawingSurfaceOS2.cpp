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
 */

#include "nsGfxDefs.h"
#include "nsHashtable.h"
#include "nsIWidget.h"
#include "nsDrawingSurfaceOS2.h"
#include "nsFontMetricsOS2.h"

// Base class -- fonts, palette and xpcom -----------------------------------

NS_IMPL_ISUPPORTS(nsDrawingSurfaceOS2, NS_GET_IID(nsIDrawingSurface))

// We start allocated lCIDs at 2.  This leaves #1 for nsFontMetricsOS2 to
// do testing with, and 0 is, of course, LCID_DEFAULT.

nsDrawingSurfaceOS2::nsDrawingSurfaceOS2()
                    : mNextID(2), mTopID(1), mPS(0)
{
   NS_INIT_REFCNT();
   mHTFonts = new nsHashtable;
}

nsDrawingSurfaceOS2::~nsDrawingSurfaceOS2()
{
   DisposeFonts();
}

void nsDrawingSurfaceOS2::DisposeFonts()
{
   if( mHTFonts)
   {
      // free font things
      GpiSetCharSet( mPS, LCID_DEFAULT);
      for( int i = 2; i <= mTopID; i++)
      {
         if( !GpiDeleteSetId( mPS, i))
            PMERROR( "GpiDeleteSetId");
      }
      delete mHTFonts;
      mHTFonts = 0;
   }
}

// Key for the hashtable
typedef nsVoidKey FontHandleKey;

void nsDrawingSurfaceOS2::SelectFont( nsIFontMetrics *metrics)
{
   nsFontHandle fh = nsnull;
   metrics->GetFontHandle( fh);

   nsFontHandleOS2 *pHandle = (nsFontHandleOS2 *) fh;
   FontHandleKey    key((void*)pHandle->ulHashMe);

   if( !mHTFonts->Get( &key))
   {
      if( mNextID == 255)
         // ids used up, need to empty table and start again.
         FlushFontCache();

      GpiCreateLogFont( mPS, 0, mNextID, &pHandle->fattrs);
      mHTFonts->Put( &key, (void *) mNextID);
      mNextID++;
      if( mTopID < 254)
         mTopID++;
   }

   long lcid = (long) mHTFonts->Get( &key);
   pHandle->SelectIntoPS( mPS, lcid);
}

void nsDrawingSurfaceOS2::FlushFontCache()
{
   mHTFonts->Reset();
   mNextID = 2;
   // leave mTopID where it is.
}

// yuck.
nsresult nsDrawingSurfaceOS2::GetBitmap( HBITMAP &aBitmap)
{
   aBitmap = 0;
   return NS_OK;
}

nsresult nsDrawingSurfaceOS2::RequiresInvertedMask( PRBool *aBool)
{
   *aBool = (PRBool) (gModuleData.lDisplayDepth <= 8);
   return NS_OK;
}

// Offscreen surface --------------------------------------------------------

nsOffscreenSurface::nsOffscreenSurface() : mDC(0), mBitmap(0),
                                           mHeight(0), mWidth(0),
                                           mInfoHeader(0), mBits(0),
                                           mYPels(0), mScans(0)
{}

// Setup a new offscreen surface which is to be compatible with the
// passed-in presentation space.
nsresult nsOffscreenSurface::Init( HPS     aCompatiblePS,
                                   PRInt32 aWidth, PRInt32 aHeight)
{
   nsresult rc = NS_ERROR_FAILURE;

   // Find the compatible device context and create a memory one
   HDC hdcCompat = GpiQueryDevice( aCompatiblePS);
   DEVOPENSTRUC dop = { 0, 0, 0, 0, 0 };
   mDC = DevOpenDC( 0/*hab*/, OD_MEMORY, "*", 5,
                   (PDEVOPENDATA) &dop, hdcCompat);

   if( DEV_ERROR != mDC)
   {
      // create the PS
      SIZEL sizel = { 0, 0 };
      mPS = GpiCreatePS( 0/*hab*/, mDC, &sizel,
                         PU_PELS | GPIT_MICRO | GPIA_ASSOC);

      if( GPI_ERROR != mPS)
      {
         // now create a bitmap of the right size
         BITMAPINFOHEADER2 hdr = { 0 };
      
         hdr.cbFix = sizeof( BITMAPINFOHEADER2);
         hdr.cx = aWidth;
         hdr.cy = aHeight;
         hdr.cPlanes = 1;

         // find bitdepth
         LONG lBitCount = 0;
         DevQueryCaps( hdcCompat, CAPS_COLOR_BITCOUNT, 1, &lBitCount);
         hdr.cBitCount = (USHORT) lBitCount;
      
         mBitmap = GpiCreateBitmap( mPS, &hdr, 0, 0, 0);

         if( GPI_ERROR != mBitmap)
         {
            // set final stats & select bitmap into ps
            mHeight = aHeight;
            mWidth = aWidth;
            GpiSetBitmap( mPS, mBitmap);
            rc = NS_OK;
         }
         else
            PMERROR( "GpiCreateBitmap");
      }
      else
         PMERROR( "GpiCreatePS");
   }
   else
      PMERROR( "DevOpenDC");

   return rc;
}

nsOffscreenSurface::~nsOffscreenSurface()
{
   if( mPS)
   {
      DisposeFonts();
      if( HBM_ERROR == GpiSetBitmap( mPS, 0))
         PMERROR( "GpiSetBitmap");
      if( !GpiDeleteBitmap( mBitmap))
         PMERROR( "GpiDeleteBitmap");
//
//    Don't need to do this because the PS is a micro-one.
//
//    if( !GpiAssociate( mPS, 0))
//       PMERROR( "GpiAssociate");
//
      if( !GpiDestroyPS( mPS))
         PMERROR( "GpiDestroyPS");
      if( DEV_ERROR == DevCloseDC( mDC))
         PMERROR( "DevCloseDC");
      mPS = 0;
   }
   if( mInfoHeader)
      free( mInfoHeader);
   delete [] mBits;
}

nsresult nsOffscreenSurface::GetBitmap( HBITMAP &aBitmap)
{
   aBitmap = mBitmap;
   return NS_OK;
}

// Okay; plan here is to get the bits and hope that the fact that we're
// returning an upside-down rectangle doesn't matter.
//
// If it does, then the following needs to be done:
//
//  * undefine the USD flag in libimg
//  * alter the draw code in nsImageOS2 to draw right-way-up
//  * fix the printing case (probably involving an ugly xform)
//
nsresult nsOffscreenSurface::Lock( PRInt32 aX, PRInt32 aY,
                                   PRUint32 aWidth, PRUint32 aHeight,
                                   void **aBits, PRInt32 *aStride,
                                   PRInt32 *aWidthBytes,
                                   PRUint32 aFlags)
{
   // Trust other platforms to ensure that we don't get nested!
   PRInt32 lStride = 0;
   ULONG   rc = 0;

   // Allocate buffers first time we get called.
   //
   // Need to look at the way in which this functionality is exercised:
   // may actually be more efficient to only grab the section of bitmap
   // required on each call, and to free up memory afterwards.
   //
   // Currently: * allocate once enough space for the entire bitmap
   //            * only grab & set the required scanlines
   //
   if( !mBits)
   {
      BITMAPINFOHEADER bih = { sizeof( BITMAPINFOHEADER), 0, 0, 0, 0 };
   
      rc = GpiQueryBitmapInfoHeader( mBitmap, (PBITMAPINFOHEADER2) &bih);
      if( !rc) PMERROR( "GpiQueryInfoHeader");
   
      // alloc space to query pel data into...
      lStride = RASWIDTH( bih.cx, bih.cBitCount);
      mBits = new PRUint8 [ lStride * bih.cy ];
   
      // ..and colour table too
      int cols = -1;
      if( bih.cBitCount >= 24) cols = 0;
      else cols = 1 << bih.cBitCount;
   
      int szStruct = sizeof( BITMAPINFOHEADER2) + cols * sizeof( RGB2);
   
      mInfoHeader = (PBITMAPINFOHEADER2) calloc( szStruct, 1);
      mInfoHeader->cbFix = sizeof( BITMAPINFOHEADER2);
      mInfoHeader->cx = bih.cx;
      mInfoHeader->cy = bih.cy;
      mInfoHeader->cPlanes = 1;
      mInfoHeader->cBitCount = (USHORT) bih.cBitCount;
      // GPI-Ref says these have to be set too...
      mInfoHeader->ulCompression = BCA_UNCOMP;
      mInfoHeader->usRecording = BRA_BOTTOMUP;
      mInfoHeader->usRendering = BRH_NOTHALFTONED; // ...hmm...
      mInfoHeader->ulColorEncoding = BCE_RGB;
   }
   else
      lStride = RASWIDTH( mInfoHeader->cx, mInfoHeader->cBitCount);

   // record starting scanline (bottom is 0)
   mYPels = mInfoHeader->cy - aY - aHeight;
   mScans = aHeight;

   rc = GpiQueryBitmapBits( mPS, mYPels, mScans, (PBYTE) mBits,
                            (PBITMAPINFO2) mInfoHeader);
   if( rc != mInfoHeader->cy) PMERROR( "GpiQueryBitmapBits");

#ifdef DEBUG
   printf( "Lock, requested %d x %d and got %d x %d\n",
           aWidth, aHeight, (int) mInfoHeader->cx, aHeight);
#endif

   // Okay.  Now have current state of bitmap in mBits.
   *aStride = lStride;
   *aBits = (void*) (mBits + (aX * (mInfoHeader->cBitCount >> 3)));
   *aWidthBytes = aWidth * (mInfoHeader->cBitCount >> 3);

   return NS_OK;
}

nsresult nsOffscreenSurface::Unlock()
{
   long rc = GpiSetBitmapBits( mPS, mYPels, mScans, (PBYTE) mBits,
                               (PBITMAPINFO2) mInfoHeader);
   if( rc == GPI_ALTERROR) PMERROR( "GpiSetBitmapBits");

   return NS_OK;
}

nsresult nsOffscreenSurface::GetDimensions( PRUint32 *aWidth, PRUint32 *aHeight)
{
   if( !aWidth || !aHeight)
      return NS_ERROR_NULL_POINTER;

   *aWidth = mWidth;
   *aHeight = mHeight;

   return NS_OK;
}

nsresult nsOffscreenSurface::IsOffscreen( PRBool *aOffScreen)
{
   if( !aOffScreen)
      return NS_ERROR_NULL_POINTER;

   *aOffScreen = PR_TRUE;

   return NS_OK;
}

nsresult nsOffscreenSurface::IsPixelAddressable( PRBool *aAddressable)
{
   if( !aAddressable)
      return NS_ERROR_NULL_POINTER;

   *aAddressable = PR_TRUE;

   return NS_OK;
}

nsresult nsOffscreenSurface::GetPixelFormat( nsPixelFormat *aFormat)
{
   if( !aFormat)
      return NS_ERROR_NULL_POINTER;

   // Okay.  Who knows what's going on here - we (as wz) currently support
   // only 8 and 24 bpp bitmaps; dunno what should be done for 32 bpp,
   // even if os/2 supports them.
   //
   // (prob'ly need to get the FOURCC stuff into the act for 16bpp?)
   //
   BITMAPINFOHEADER bih = { sizeof( BITMAPINFOHEADER), 0, 0, 0, 0 };
   long rc = GpiQueryBitmapInfoHeader( mBitmap, (PBITMAPINFOHEADER2) &bih);

   switch( bih.cBitCount)
   {
      case 8:
         memset( aFormat, 0, sizeof(nsPixelFormat));
         break;

      case 24:
         aFormat->mRedZeroMask = 0xff;
         aFormat->mGreenZeroMask = 0xff;
         aFormat->mBlueZeroMask = 0xff;
         aFormat->mAlphaZeroMask = 0;
         aFormat->mRedMask = 0xff;
         aFormat->mGreenMask = 0xff00;
         aFormat->mBlueMask = 0xff0000;
         aFormat->mAlphaMask = 0;
         aFormat->mRedCount = 8;
         aFormat->mGreenCount = 8;
         aFormat->mBlueCount = 8;
         aFormat->mAlphaCount = 0;
         aFormat->mRedShift = 0;
         aFormat->mGreenShift = 8;
         aFormat->mBlueShift = 16;
         aFormat->mAlphaShift = 0;
         break;

      default:
         printf( "Bad bit-depth for GetPixelFormat (%d)\n", bih.cBitCount);
         break;
   }

   return NS_OK;
}

// Non-offscreen surfaces, base for window & print --------------------------
nsOnscreenSurface::nsOnscreenSurface() : mProxySurface(nsnull)
{
}

nsOnscreenSurface::~nsOnscreenSurface()
{
   NS_IF_RELEASE(mProxySurface);
}

void nsOnscreenSurface::EnsureProxy()
{
   if( !mProxySurface)
   {
      PRUint32 width, height;
      GetDimensions( &width, &height);

      mProxySurface = new nsOffscreenSurface;
      if( NS_SUCCEEDED(mProxySurface->Init( mPS, width, height)))
      {
         NS_ADDREF(mProxySurface);
      }
      else
      {
         delete mProxySurface;
         mProxySurface = nsnull;
      }
   }
}

nsresult nsOnscreenSurface::Lock( PRInt32 aX, PRInt32 aY,
                                  PRUint32 aWidth, PRUint32 aHeight,
                                  void **aBits, PRInt32 *aStride,
                                  PRInt32 *aWidthBytes,
                                  PRUint32 aFlags)
{
   EnsureProxy();

   printf( "Locking through a proxy\n");

   // blit our 'real' bitmap to the proxy surface
   PRUint32 width, height;
   GetDimensions( &width, &height);
   POINTL pts[3] = { { 0, 0 }, { width, height }, { 0, 0 } };
   long lHits = GpiBitBlt( mProxySurface->mPS, mPS, 3, pts,
                           ROP_SRCCOPY, BBO_OR);
   if( GPI_ERROR == lHits) PMERROR( "GpiBitBlt/DSL");

   return mProxySurface->Lock( aX, aY, aWidth, aHeight,
                               aBits, aStride, aWidthBytes, aFlags);
}

nsresult nsOnscreenSurface::Unlock()
{
   nsresult rc = mProxySurface->Unlock();

   // blit proxy bitmap back to ours
   PRUint32 width, height;
   GetDimensions( &width, &height);
   POINTL pts[3] = { { 0, 0 }, { width, height }, { 0, 0 } };
   long lHits = GpiBitBlt( mPS, mProxySurface->mPS, 3, pts,
                           ROP_SRCCOPY, BBO_OR);
   if( GPI_ERROR == lHits) PMERROR( "GpiBitBlt/DSUL");

   return rc;
}

nsresult nsOnscreenSurface::GetPixelFormat( nsPixelFormat *aFormat)
{
   EnsureProxy();
   return mProxySurface->GetPixelFormat( aFormat);
}

nsresult nsOnscreenSurface::IsOffscreen( PRBool *aOffScreen)
{
   if( !aOffScreen)
      return NS_ERROR_NULL_POINTER;

   *aOffScreen = PR_FALSE;

   return NS_OK;
}

nsresult nsOnscreenSurface::IsPixelAddressable( PRBool *aAddressable)
{
   if( !aAddressable)
      return NS_ERROR_NULL_POINTER;

   *aAddressable = PR_FALSE;

   return NS_OK;
}

// Surface for a PM window --------------------------------------------------
nsWindowSurface::nsWindowSurface() : mWidget(nsnull)
{}

nsWindowSurface::~nsWindowSurface()
{
   // palette will be deselected in superclass dtor

   // need to do this now because hps is invalid after subsequent free
   DisposeFonts();
   // release hps
   mWidget->FreeNativeData( (void*) mPS, NS_NATIVE_GRAPHIC);
   mPS = 0; // just for safety
}

nsresult nsWindowSurface::Init( nsIWidget *aOwner)
{
   mWidget = aOwner;
   mPS = (HPS) mWidget->GetNativeData( NS_NATIVE_GRAPHIC);
   return NS_OK;
}

nsresult nsWindowSurface::GetDimensions( PRUint32 *aWidth, PRUint32 *aHeight)
{
   // I don't think we can be more efficient than this, except perhaps by
   // doing some kind of `push' of height from the window to us.
   nsRect rect;
   mWidget->GetClientBounds( rect);
   *aHeight = rect.height;
   *aWidth = rect.width;
   return NS_OK;
}

// Printer surface.  A few minor differences, like the page size is fixed ---
nsPrintSurface::nsPrintSurface() : mHeight(0), mWidth(0)
{}

nsresult nsPrintSurface::Init( HPS aPS, PRInt32 aWidth, PRInt32 aHeight)
{
   mPS = aPS;
   mHeight = aHeight;
   mWidth = aWidth;
   return NS_OK;
}

nsPrintSurface::~nsPrintSurface()
{
   // PS is owned by the DC; superclass dtor will deselect palette.
}

nsresult nsPrintSurface::GetDimensions( PRUint32 *aWidth, PRUint32 *aHeight)
{
   if( !aWidth || !aHeight)
      return NS_ERROR_NULL_POINTER;

   *aWidth = mWidth;
   *aHeight = mHeight;

   return NS_OK;
}

nsresult nsPrintSurface::RequiresInvertedMask( PRBool *aBool)
{
   *aBool = PR_FALSE;
   return NS_OK;
}
