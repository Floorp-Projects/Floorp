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
  NS_IMETHOD GetTECHNOLOGY(PRInt32  *aTechnology); 
  NS_IMETHOD ReleaseDC(void);
  NS_IMETHOD IsReleaseDCDestructive(PRBool *aDestructive);

  // locals
#ifdef NGLAYOUT_DDRAW
  nsresult GetDDraw(IDirectDraw2 **aDDraw);
#endif

private:
  ~nsDrawingSurfaceWin();

  BITMAPINFO *CreateBitmapInfo(PRInt32 aWidth, PRInt32 aHeight, PRInt32 aDepth,
                               void **aBits = nsnull);

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
  PRInt32       mTechnology;


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
