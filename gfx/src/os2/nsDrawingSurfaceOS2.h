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
 * This Original Code has been modified by IBM Corporation.
 * Modifications made by IBM described herein are
 * Copyright (c) International Business Machines
 * Corporation, 2000
 *
 * Modifications to Mozilla code or documentation
 * identified per MPL Section 3.3
 *
 * Date           Modified by     Description of modification
 * 03/28/2000   IBM Corp.        Changes to make os2.h file similar to windows.h file
 */

#ifndef _nsDrawingSurfaceOS2_h
#define _nsDrawingSurfaceOS2_h

#include "nsIDrawingSurface.h"

class nsHashtable;
class nsIWidget;
class nsIPaletteOS2;

// These were called `drawables' in os2fe.
//
// Note that each surface has a ref to the palette of the device context
// from which it was spun off.  This is because drawing surfaces have
// lifetimes which extend further than the rendering context which
// created them.  The creating object must select the palette into the
// surface's presentation space; all the drawing surface does is deselect
// the palette when it's dying.
//
// This is rather unwieldy...
//
// Note: |nsDrawingSurface| == (void*)(nsDrawingSurfaceOS2*)surf

class nsDrawingSurfaceOS2 : public nsIDrawingSurface
{
   nsHashtable   *mHTFonts; // cache of fonthandle to lcid
   long           mNextID;  // next lcid to allocate
   long           mTopID;   // highest used lcid
   nsIPaletteOS2 *mPalette; // palette

 public:
   nsDrawingSurfaceOS2();
   virtual ~nsDrawingSurfaceOS2();

   // nsISupports
   NS_DECL_ISUPPORTS

   // nsIDrawingSurface actually implemented in subclasses

   HPS mPS;               // presentation space for this surface

   void SelectFont( nsIFontMetrics *metrics);
   void FlushFontCache();

   void SetPalette( nsIPaletteOS2 *aPalette);

   NS_IMETHOD GetBitmap( HBITMAP &aBitmap); // yuck (for blender, may go)
   NS_IMETHOD RequiresInvertedMask( PRBool *aBool); // double yuck (images)

 protected:
   void DeselectPalette();
   void DisposeFonts();     // MUST be called before disposing of PS
};

// Offscreen surface. Others depend on this.
class nsOffscreenSurface : public nsDrawingSurfaceOS2
{
   HDC     mDC;
   HBITMAP mBitmap;
   PRInt32 mHeight;
   PRInt32 mWidth;

   PBITMAPINFOHEADER2  mInfoHeader;
   PRUint8            *mBits;

   PRInt32  mYPels;
   PRUint32 mScans;

 public:
   nsOffscreenSurface();
   virtual ~nsOffscreenSurface();

   // os/2 methods
   NS_IMETHOD Init( HPS aCompatiblePS, PRInt32 aWidth, PRInt32 aHeight);
   NS_IMETHOD GetBitmap( HBITMAP &aBitmap);

   // nsIDrawingSurface methods
   NS_IMETHOD Lock( PRInt32 aX, PRInt32 aY, PRUint32 aWidth, PRUint32 aHeight,
                    void **aBits, PRInt32 *aStride, PRInt32 *aWidthBytes,
                    PRUint32 aFlags);
   NS_IMETHOD Unlock();
   NS_IMETHOD GetDimensions( PRUint32 *aWidth, PRUint32 *aHeight);
   NS_IMETHOD IsOffscreen( PRBool *aOffScreen);
   NS_IMETHOD IsPixelAddressable( PRBool *aAddressable);
   NS_IMETHOD GetPixelFormat( nsPixelFormat *aFormat);
};

// Onscreen surface - uses an offscreen to implement bitlevel access
class nsOnscreenSurface : public nsDrawingSurfaceOS2
{
   nsOffscreenSurface *mProxySurface;
   void                EnsureProxy();

 public:
   nsOnscreenSurface();
   virtual ~nsOnscreenSurface();

   // nsIDrawingSurface methods
   NS_IMETHOD Lock( PRInt32 aX, PRInt32 aY, PRUint32 aWidth, PRUint32 aHeight,
                    void **aBits, PRInt32 *aStride, PRInt32 *aWidthBytes,
                    PRUint32 aFlags);
   NS_IMETHOD Unlock();
   NS_IMETHOD IsOffscreen( PRBool *aOffScreen);
   NS_IMETHOD IsPixelAddressable( PRBool *aAddressable);
   NS_IMETHOD GetPixelFormat( nsPixelFormat *aFormat);
};

// Surface for an onscreen window
class nsWindowSurface : public nsOnscreenSurface
{
   nsIWidget *mWidget; // window who owns the surface

 public:
   nsWindowSurface();
   virtual ~nsWindowSurface();

   NS_IMETHOD Init( nsIWidget *aOwner);
   NS_IMETHOD GetDimensions( PRUint32 *aWidth, PRUint32 *aHeight);
};

// Surface for a printer-page
class nsPrintSurface : public nsOnscreenSurface
{
   PRInt32 mHeight;
   PRInt32 mWidth;

 public:
   nsPrintSurface();
   virtual ~nsPrintSurface();

   NS_IMETHOD Init( HPS aPS, PRInt32 aWidth, PRInt32 aHeight);
   NS_IMETHOD RequiresInvertedMask( PRBool *aNeedsIMask);
   NS_IMETHOD GetDimensions( PRUint32 *aWidth, PRUint32 *aHeight);
};

#endif
