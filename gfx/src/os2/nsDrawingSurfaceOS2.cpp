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
#include "nsPaletteOS2.h"


// Base class -- fonts, palette and xpcom -----------------------------------

NS_IMPL_ISUPPORTS1(nsDrawingSurfaceOS2, nsIDrawingSurface)

// We start allocated lCIDs at 2.  This leaves #1 for nsFontMetricsOS2 to
// do testing with, and 0 is, of course, LCID_DEFAULT.

nsDrawingSurfaceOS2::nsDrawingSurfaceOS2()
                    : mNextID(2), mTopID(1), mPS(0),
                      mWidth (0), mHeight (0)
{
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
      GFX (::GpiSetCharSet (mPS, LCID_DEFAULT), FALSE);

      for( int i = 2; i <= mTopID; i++)
      {
         GFX (::GpiDeleteSetId (mPS, i), FALSE);
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

   nsFontOS2 *pHandle = (nsFontOS2 *) fh;
   FontHandleKey    key((void*)pHandle->mHashMe);

   if( !mHTFonts->Get( &key))
   {
      if( mNextID == 255)
         // ids used up, need to empty table and start again.
         FlushFontCache();

      GFX (::GpiCreateLogFont (mPS, 0, mNextID, &pHandle->mFattrs), GPI_ERROR);
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

// OS/2 - XP coord conversion ----------------------------------------------

// get inclusive-inclusive rect
void nsDrawingSurfaceOS2::NS2PM_ININ( const nsRect &in, RECTL &rcl)
{
   const static nscoord kBottomLeftLimit = -8192;
   const static nscoord kTopRightLimit   =  16384;

   PRInt32 ulHeight = GetHeight ();

   rcl.xLeft    = PR_MAX(kBottomLeftLimit, in.x);
   rcl.xRight   = PR_MIN(in.x+in.width-1, kTopRightLimit);
   rcl.yTop     = PR_MIN(ulHeight-in.y-1, kTopRightLimit);
   rcl.yBottom  = PR_MAX(rcl.yTop-in.height+1, kBottomLeftLimit);
   return;
}

void nsDrawingSurfaceOS2::PM2NS_ININ( const RECTL &in, nsRect &out)
{
   PRInt32 ulHeight = GetHeight ();

   out.x = in.xLeft;
   out.width = in.xRight - in.xLeft + 1;
   out.y = ulHeight - in.yTop - 1;
   out.height = in.yTop - in.yBottom + 1;
}

// get in-ex rect
void nsDrawingSurfaceOS2::NS2PM_INEX( const nsRect &in, RECTL &rcl)
{
   NS2PM_ININ( in, rcl);
   rcl.xRight++;
   rcl.yTop++;
}

void nsDrawingSurfaceOS2::NS2PM( PPOINTL aPointl, ULONG cPointls)
{
   PRInt32 ulHeight = GetHeight ();

   for( ULONG i = 0; i < cPointls; i++)
      aPointl[ i].y = ulHeight - aPointl[ i].y - 1;
}

nsresult nsDrawingSurfaceOS2::GetDimensions( PRUint32 *aWidth, PRUint32 *aHeight)
{
   if( !aWidth || !aHeight)
      return NS_ERROR_NULL_POINTER;

   *aWidth = mWidth;
   *aHeight = mHeight;

   return NS_OK;
}


// Offscreen surface --------------------------------------------------------

nsOffscreenSurface::nsOffscreenSurface() : mDC(0), mBitmap(0),
                                           mInfoHeader(0), mBits(0),
                                           mYPels(0), mScans(0)
{
}

NS_IMETHODIMP nsOffscreenSurface :: Init(HPS aPS)
{
  mPS = aPS;

  return NS_OK;
}

// Setup a new offscreen surface which is to be compatible with the
// passed-in presentation space.
nsresult nsOffscreenSurface::Init( HPS     aCompatiblePS,
                                   PRInt32 aWidth, PRInt32 aHeight, PRUint32 aFlags)
{
   nsresult rc = NS_ERROR_FAILURE;

   // Find the compatible device context and create a memory one
   HDC hdcCompat = GFX (::GpiQueryDevice (aCompatiblePS), HDC_ERROR);
   DEVOPENSTRUC dop = { 0, 0, 0, 0, 0 };
   mDC = GFX (::DevOpenDC( 0/*hab*/, OD_MEMORY, "*", 5, (PDEVOPENDATA) &dop, hdcCompat), DEV_ERROR);

   if( DEV_ERROR != mDC)
   {
      // create the PS
      SIZEL sizel = { 0, 0 };
      mPS = GFX (::GpiCreatePS (0/*hab*/, mDC, &sizel,
                 PU_PELS | GPIT_MICRO | GPIA_ASSOC), GPI_ERROR);

      if( GPI_ERROR != mPS)
      {
         nsPaletteOS2::SelectGlobalPalette(mPS);

         // now create a bitmap of the right size
         BITMAPINFOHEADER2 hdr = { 0 };
      
         hdr.cbFix = sizeof( BITMAPINFOHEADER2);
         hdr.cx = aWidth;
         hdr.cy = aHeight;
         hdr.cPlanes = 1;

         // find bitdepth
         LONG lBitCount = 0;
         GFX (::DevQueryCaps( hdcCompat, CAPS_COLOR_BITCOUNT, 1, &lBitCount), FALSE);
         hdr.cBitCount = (USHORT) lBitCount;

         mBitmap = GFX (::GpiCreateBitmap (mPS, &hdr, 0, 0, 0), GPI_ERROR);

         if( GPI_ERROR != mBitmap)
         {
            // set final stats & select bitmap into ps
            mHeight = aHeight;
            mWidth = aWidth;
            GFX (::GpiSetBitmap (mPS, mBitmap), HBM_ERROR);
            rc = NS_OK;
         }
      }
   }

   return rc;
}

nsOffscreenSurface::~nsOffscreenSurface()
{
   if( mPS)
   {
      DisposeFonts();
      GFX (::GpiSetBitmap (mPS, 0), HBM_ERROR);
      GFX (::GpiDeleteBitmap (mBitmap), FALSE);
      GFX (::GpiDestroyPS (mPS), FALSE);
      ::DevCloseDC( mDC);
   }
   if( mInfoHeader)
      free( mInfoHeader);
   delete [] mBits;
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
   
      rc = GFX (::GpiQueryBitmapInfoHeader (mBitmap, (PBITMAPINFOHEADER2) &bih), FALSE);
   
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

   rc = GFX (::GpiQueryBitmapBits (mPS, mYPels, mScans, (PBYTE)mBits,
                                   (PBITMAPINFO2)mInfoHeader), GPI_ALTERROR);
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
   GFX (::GpiSetBitmapBits (mPS, mYPels, mScans, (PBYTE)mBits,
                            (PBITMAPINFO2)mInfoHeader), GPI_ALTERROR);

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
   long rc = GFX (::GpiQueryBitmapInfoHeader (mBitmap, 
                                              (PBITMAPINFOHEADER2)&bih), FALSE);

   switch( bih.cBitCount)
   {
      case 8:
         memset( aFormat, 0, sizeof(nsPixelFormat));
         break;

      case 16:
         aFormat->mRedZeroMask   = 0x001F;
         aFormat->mGreenZeroMask = 0x003F;
         aFormat->mBlueZeroMask  = 0x001F;
         aFormat->mAlphaZeroMask = 0;
         aFormat->mRedMask       = 0xF800;
         aFormat->mGreenMask     = 0x07E0;
         aFormat->mBlueMask      = 0x001F;
         aFormat->mAlphaMask     = 0;
         aFormat->mRedCount      = 5;
         aFormat->mGreenCount    = 6;
         aFormat->mBlueCount     = 5;
         aFormat->mAlphaCount    = 0;
         aFormat->mRedShift      = 11;
         aFormat->mGreenShift    = 5;
         aFormat->mBlueShift     = 0;
         aFormat->mAlphaShift    = 0;
         break;

      case 24:
         aFormat->mRedZeroMask   = 0x0000FF;
         aFormat->mGreenZeroMask = 0x0000FF;
         aFormat->mBlueZeroMask  = 0x0000FF;
         aFormat->mAlphaZeroMask = 0;
         aFormat->mRedMask       = 0x0000FF;
         aFormat->mGreenMask     = 0x00FF00;
         aFormat->mBlueMask      = 0xFF0000;
         aFormat->mAlphaMask     = 0;
         aFormat->mRedCount      = 8;
         aFormat->mGreenCount    = 8;
         aFormat->mBlueCount     = 8;
         aFormat->mAlphaCount    = 0;
         aFormat->mRedShift      = 0;
         aFormat->mGreenShift    = 8;
         aFormat->mBlueShift     = 16;
         aFormat->mAlphaShift    = 0;
         break;

      default:
#ifdef DEBUG
         printf( "Bad bit-depth for GetPixelFormat (%d)\n", bih.cBitCount);
#endif
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
      if( NS_SUCCEEDED(mProxySurface->Init( mPS, width, height, NS_CREATEDRAWINGSURFACE_FOR_PIXEL_ACCESS)))
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

#ifdef DEBUG
   printf( "Locking through a proxy\n");
#endif

   // blit our 'real' bitmap to the proxy surface
   PRUint32 width, height;
   GetDimensions( &width, &height);
   POINTL pts[3] = { { 0, 0 }, { width, height }, { 0, 0 } };
   long lHits = GFX (::GpiBitBlt (mProxySurface->GetPS (), mPS, 3, pts,
                                  ROP_SRCCOPY, BBO_OR), GPI_ERROR);

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
   long lHits = GFX (::GpiBitBlt (mPS, mProxySurface->GetPS (), 3, pts,
                                  ROP_SRCCOPY, BBO_OR), GPI_ERROR);

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
{
}

nsWindowSurface::~nsWindowSurface()
{
   // palette will be deselected in superclass dtor

   // need to do this now because hps is invalid after subsequent free
   DisposeFonts();
   // release hps
   mWidget->FreeNativeData( (void*) mPS, NS_NATIVE_GRAPHIC);
   mPS = 0; // just for safety
}

NS_IMETHODIMP nsWindowSurface::Init(HPS aPS, nsIWidget *aWidget)
{
  mPS = aPS;
  mWidget = aWidget;

  return NS_OK;
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

PRUint32 nsWindowSurface::GetHeight () 
{ 
   nsRect rect;

   mWidget->GetClientBounds (rect);

   return rect.height;
}

// Printer surface.  A few minor differences, like the page size is fixed ---
nsPrintSurface::nsPrintSurface()
{
}

nsresult nsPrintSurface::Init( HPS aPS, PRInt32 aWidth, PRInt32 aHeight, PRUint32 aFlags)
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
