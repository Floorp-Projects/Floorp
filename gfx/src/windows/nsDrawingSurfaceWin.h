/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef nsDrawingSurfaceWin_h___
#define nsDrawingSurfaceWin_h___

#include "nsIDrawingSurface.h"
#include "nsIDrawingSurfaceWin.h"

#ifdef NGLAYOUT_DDRAW
#include "ddraw.h"
#endif

class nsDrawingSurfaceWin : public nsIDrawingSurface,
                            nsIDrawingSurfaceWin
{
public:
  nsDrawingSurfaceWin();

  NS_DECL_ISUPPORTS

  //nsIDrawingSurface interface

  NS_IMETHOD Lock(PRInt32 aX, PRInt32 aY, PRUint32 aWidth, PRUint32 aHeight,
                  void **aBits, PRInt32 *aStride, PRInt32 *aWidthBytes,
                  PRUint32 aFlags);
  NS_IMETHOD Unlock(void);
  NS_IMETHOD GetDimensions(PRUint32 *aWidth, PRUint32 *aHeight);
  NS_IMETHOD IsOffscreen(PRBool *aOffScreen);
  NS_IMETHOD IsPixelAddressable(PRBool *aAddressable);
  NS_IMETHOD GetPixelFormat(nsPixelFormat *aFormat);

  //nsIDrawingSurfaceWin interface

  NS_IMETHOD Init(HDC aDC);
  NS_IMETHOD Init(HDC aDC, PRUint32 aWidth, PRUint32 aHeight,
                  PRUint32 aFlags);
  NS_IMETHOD GetDC(HDC *aDC);
  NS_IMETHOD ReleaseDC(void);
  NS_IMETHOD IsReleaseDCDestructive(PRBool *aDestructive);

  // locals
#ifdef NGLAYOUT_DDRAW
  nsresult GetDDraw(IDirectDraw2 **aDDraw);
#endif

private:
  ~nsDrawingSurfaceWin();

  BITMAPINFO *CreateBitmapInfo(PRInt32 aWidth, PRInt32 aHeight, PRInt32 aDepth,
                               void **aBits = nsnull, nsPixelFormat *aPixFormat = nsnull);

#ifdef NGLAYOUT_DDRAW
  nsresult CreateDDraw(void);
  PRBool LockSurface(IDirectDrawSurface *aSurface, DDSURFACEDESC *aDesc,
                     BITMAP *aBitmap, RECT *aRect, DWORD aLockFlags, nsPixelFormat *aPixFormat);
#endif

  HDC           mDC;
  HBITMAP       mOrigBitmap;
  HBITMAP       mSelectedBitmap;
  PRBool        mKillDC;
  BITMAPINFO    *mBitmapInfo;
  PRUint8       *mDIBits;
  BITMAP        mBitmap;
  nsPixelFormat mPixFormat;
  HBITMAP       mLockedBitmap;
  PRUint32      mWidth;
  PRUint32      mHeight;
  PRInt32       mLockOffset;
  PRInt32       mLockHeight;
  PRUint32      mLockFlags;

#ifdef NGLAYOUT_DDRAW
  IDirectDrawSurface  *mSurface;
  PRInt32             mSurfLockCnt;
  DDSURFACEDESC       mSurfDesc;

  static IDirectDraw  *mDDraw;
  static IDirectDraw2 *mDDraw2;
  static nsresult     mDDrawResult;
#endif
};

#endif
