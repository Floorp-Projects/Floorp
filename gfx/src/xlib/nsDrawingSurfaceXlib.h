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

#ifndef nsDrawingSurfaceXlib_h__
#define nsDrawingSurfaceXlib_h__

#include "nsIDrawingSurface.h"
#include "nsGCCache.h"
#include "xlibrgb.h"

class nsDrawingSurfaceXlib : public nsIDrawingSurface
{
public:
  nsDrawingSurfaceXlib();
  virtual ~nsDrawingSurfaceXlib();

  NS_DECL_ISUPPORTS
  
  NS_IMETHOD Lock(PRInt32 aX, PRInt32 aY, PRUint32 aWidth, PRUint32 aHeight,
                  void **aBits, PRInt32 *aStride, PRInt32 *aWidthBytes,
                  PRUint32 aFlags);
  NS_IMETHOD Unlock(void);
  NS_IMETHOD GetDimensions(PRUint32 *aWidth, PRUint32 *aHeight);
  NS_IMETHOD IsOffscreen(PRBool *aOffScreen);
  NS_IMETHOD IsPixelAddressable(PRBool *aAddressable);
  NS_IMETHOD GetPixelFormat(nsPixelFormat *aFormat);

  NS_IMETHOD Init (XlibRgbHandle *aHandle,
                   Drawable  aDrawable, 
                   xGC       * aGC);

  NS_IMETHOD Init (XlibRgbHandle *aHandle,
                   xGC      *  aGC, 
                   PRUint32  aWidth, 
                   PRUint32  aHeight, 
                   PRUint32  aFlags);

  Drawable   GetDrawable() { return mDrawable; }  
  XlibRgbHandle *GetXlibRgbHandle() { return mXlibRgbHandle; }
  Display *  GetDisplay() { return mDisplay; }
  Screen *   GetScreen() { return mScreen; }
  Visual *   GetVisual() { return mVisual; }
  int        GetDepth() { return mDepth; }
  int        GetScreenNumber() { return XScreenNumberOfScreen(mScreen); }
  xGC *      GetGC() { mGC->AddRef(); return mGC; }

private:
  void       CommonInit();

  XlibRgbHandle *mXlibRgbHandle;
  Display *      mDisplay;
  Screen *       mScreen;
  Visual *       mVisual;
  int            mDepth;
  xGC           * mGC;
  Drawable       mDrawable;
  XImage *       mImage;
  nsPixelFormat  mPixFormat;

  // for locking
  PRInt32	       mLockX;
  PRInt32	       mLockY;
  PRUint32	     mLockWidth;
  PRUint32	     mLockHeight;
  PRUint32	     mLockFlags;
  PRBool	       mLocked;

  // dimensions
  PRUint32       mWidth;
  PRUint32       mHeight;

  // are we offscreen
  PRBool         mIsOffscreen;
  PRBool         mDestroyDrawable;


private:

  static PRUint8 ConvertMaskToCount(unsigned long val);
  static PRUint8 GetShiftForMask(unsigned long val);
};

#endif
