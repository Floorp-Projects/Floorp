/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is
 * John Fairhurst, <john_fairhurst@iname.com>.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK *****
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
class nsFontOS2;

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

class nsDrawingSurfaceOS2 : public nsIDrawingSurface
{
   nsHashtable   *mHTFonts; // cache of fonthandle to lcid
   long           mNextID;  // next lcid to allocate
   long           mTopID;   // highest used lcid

protected:
   HPS            mPS;      // presentation space for this surface
   PRBool         mOwnPS;   // did we instantiate PS or was it passed in?
   PRInt32        mWidth;   // dimensions of drawing surface
   PRInt32        mHeight;


   nsDrawingSurfaceOS2();
   virtual ~nsDrawingSurfaceOS2();

   NS_DECL_ISUPPORTS

   void DisposeFonts();     // MUST be called before disposing of PS


 public:
   NS_IMETHOD GetDimensions( PRUint32 *aWidth, PRUint32 *aHeight);

   // os/2 methods
   HPS  GetPS() { return mPS; }
   void SelectFont(nsFontOS2* aFont);
   void FlushFontCache();
   virtual PRUint32 GetHeight() { return mHeight; }

   // Convertion between XP and OS/2 coordinate space.
   void NS2PM_ININ (const nsRect &in, RECTL &rcl);
   void NS2PM_INEX (const nsRect &in, RECTL &rcl);
   void PM2NS_ININ (const RECTL &in, nsRect &out);
   void NS2PM      (PPOINTL aPointl, ULONG cPointls);
};

// Offscreen surface. Others depend on this.
class nsOffscreenSurface : public nsDrawingSurfaceOS2
{
   HDC     mDC;
   HBITMAP mBitmap;

   PBITMAPINFOHEADER2  mInfoHeader;
   PRUint8            *mBits;

   PRInt32  mYPels;
   PRUint32 mScans;

 public:
   nsOffscreenSurface();
   virtual ~nsOffscreenSurface();

   // os/2 methods
   NS_IMETHOD Init( HPS aCompatiblePS, PRInt32 aWidth, PRInt32 aHeight, PRUint32 aFlags);
   NS_IMETHOD Init(HPS aPS);

   // nsIDrawingSurface methods
   NS_IMETHOD Lock( PRInt32 aX, PRInt32 aY, PRUint32 aWidth, PRUint32 aHeight,
                    void **aBits, PRInt32 *aStride, PRInt32 *aWidthBytes,
                    PRUint32 aFlags);
   NS_IMETHOD Unlock();
   NS_IMETHOD IsOffscreen( PRBool *aOffScreen);
   NS_IMETHOD IsPixelAddressable( PRBool *aAddressable);
   NS_IMETHOD GetPixelFormat( nsPixelFormat *aFormat);
};

// Onscreen surface - uses an offscreen to implement bitlevel access
class nsOnscreenSurface : public nsDrawingSurfaceOS2
{
   nsOffscreenSurface *mProxySurface;
   void                EnsureProxy();

 protected:
   nsOnscreenSurface();
   virtual ~nsOnscreenSurface();

 public:
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

   NS_IMETHOD Init(HPS aPS, nsIWidget *aWidget);
   NS_IMETHOD Init( nsIWidget *aOwner);
   NS_IMETHOD GetDimensions( PRUint32 *aWidth, PRUint32 *aHeight);

   virtual PRUint32 GetHeight ();
};

// Surface for a printer-page
class nsPrintSurface : public nsOnscreenSurface
{
 public:
   nsPrintSurface();
   virtual ~nsPrintSurface();

   NS_IMETHOD Init( HPS aPS, PRInt32 aWidth, PRInt32 aHeight, PRUint32 aFlags);
};

#endif
