/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsDrawingSurfaceXlib_h__
#define nsDrawingSurfaceXlib_h__

#include "nsIDrawingSurface.h"
#include "nsGCCache.h"
#include "xlibrgb.h"

/* common interface for both nsDrawingSurfaceXlibImpl (drawing surface for
 * normal displays) and nsXPrintContext (drawing surface for printers) 
 */
class nsIDrawingSurfaceXlib : public nsIDrawingSurface
{
public:
  NS_IMETHOD Lock(PRInt32 aX, PRInt32 aY, PRUint32 aWidth, PRUint32 aHeight,
                  void **aBits, PRInt32 *aStride, PRInt32 *aWidthBytes,
                  PRUint32 aFlags) = 0;
  NS_IMETHOD Unlock(void) = 0;
  NS_IMETHOD GetDimensions(PRUint32 *aWidth, PRUint32 *aHeight) = 0;
  NS_IMETHOD IsOffscreen(PRBool *aOffScreen) = 0;
  NS_IMETHOD IsPixelAddressable(PRBool *aAddressable) = 0;
  NS_IMETHOD GetPixelFormat(nsPixelFormat *aFormat) = 0;

  NS_IMETHOD GetDrawable(Drawable &aDrawable) = 0;
  NS_IMETHOD GetXlibRgbHandle(XlibRgbHandle *&aHandle) = 0;
  NS_IMETHOD GetGC(xGC *&aXGC) = 0;
};


class nsDrawingSurfaceXlibImpl : public nsIDrawingSurfaceXlib
{
public:
  nsDrawingSurfaceXlibImpl();
  virtual ~nsDrawingSurfaceXlibImpl();

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

  NS_IMETHOD GetDrawable(Drawable &aDrawable) { aDrawable = mDrawable; return NS_OK; }
  NS_IMETHOD GetXlibRgbHandle(XlibRgbHandle *&aHandle) { aHandle = mXlibRgbHandle; return NS_OK; }
  NS_IMETHOD GetGC(xGC *&aXGC) { mGC->AddRef(); aXGC = mGC; return NS_OK; }

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

#endif /* !nsDrawingSurfaceXlib_h__ */

