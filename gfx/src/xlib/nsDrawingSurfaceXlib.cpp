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

#include "nsDrawingSurfaceXlib.h"

static NS_DEFINE_IID(kIDrawingSurfaceIID, NS_IDRAWING_SURFACE_IID);

extern Display         *gDisplay;
extern Screen          *gScreen;
extern int              gScreenNum;
extern int              gDepth;
extern Visual          *gVisual;
extern XVisualInfo     *gVisualInfo;

extern PRUint32  gRedZeroMask;     //red color mask in zero position
extern PRUint32  gGreenZeroMask;   //green color mask in zero position
extern PRUint32  gBlueZeroMask;    //blue color mask in zero position
extern PRUint32  gAlphaZeroMask;   //alpha data mask in zero position
extern PRUint32  gRedMask;         //red color mask
extern PRUint32  gGreenMask;       //green color mask
extern PRUint32  gBlueMask;        //blue color mask
extern PRUint32  gAlphaMask;       //alpha data mask
extern PRUint8   gRedCount;        //number of red color bits
extern PRUint8   gGreenCount;      //number of green color bits
extern PRUint8   gBlueCount;       //number of blue color bits
extern PRUint8   gAlphaCount;      //number of alpha data bits
extern PRUint8   gRedShift;        //number to shift value into red position
extern PRUint8   gGreenShift;      //number to shift value into green position
extern PRUint8   gBlueShift;       //number to shift value into blue position
extern PRUint8   gAlphaShift;      //number to shift value into alpha position

nsDrawingSurfaceXlib::nsDrawingSurfaceXlib()
{
  NS_INIT_REFCNT();
  printf("nsDrawingSurfaceXlib::nsDrawingSurfaceXlib()\n");
  mPixmap = 0;
  mImage = nsnull;
  mGC = 0;
  // set up lock info
  mLocked = PR_FALSE;
  mLockX = 0;
  mLockY = 0;
  mLockWidth = 0;
  mLockHeight = 0;
  mLockFlags = 0;
  // dimensions...
  mWidth = 0;
  mHeight = 0;
  mIsOffscreen = PR_FALSE;
  // set up the masks for the pix formats
  mPixFormat.mRedMask = gRedMask;
  mPixFormat.mGreenMask = gGreenMask;
  mPixFormat.mBlueMask = gBlueMask;
  mPixFormat.mAlphaMask = gAlphaMask;
  mPixFormat.mRedCount = gRedCount;
  mPixFormat.mGreenCount = gGreenCount;
  mPixFormat.mBlueCount = gBlueCount;
  mPixFormat.mAlphaCount = gAlphaCount;
  mPixFormat.mRedShift = gRedShift;
  mPixFormat.mGreenShift = gGreenShift;
  mPixFormat.mBlueShift = gBlueShift;
  mPixFormat.mAlphaShift = gAlphaShift;
}

nsDrawingSurfaceXlib::~nsDrawingSurfaceXlib()
{
  if (mPixmap) {
    XFreePixmap(gDisplay, mPixmap);
  }
  if (mImage) {
    XDestroyImage(mImage);
  }
}

NS_IMPL_QUERY_INTERFACE(nsDrawingSurfaceXlib, kIDrawingSurfaceIID)
NS_IMPL_ADDREF(nsDrawingSurfaceXlib)
NS_IMPL_RELEASE(nsDrawingSurfaceXlib)


NS_IMETHODIMP
nsDrawingSurfaceXlib::Lock(PRInt32 aX, PRInt32 aY,
                           PRUint32 aWidth, PRUint32 aHeight,
                           void **aBits, PRInt32 *aStride,
                           PRInt32 *aWidthBytes, PRUint32 aFlags)
{
  if (mLocked)
  {
    NS_ASSERTION(0, "nested lock attempt");
    return NS_ERROR_FAILURE;
  }
  mLocked = PR_TRUE;

  mLockX = aX;
  mLockY = aY;
  mLockWidth = aWidth;
  mLockHeight = aHeight;
  mLockFlags = aFlags;

  mImage = XGetImage(gDisplay, mPixmap,
                     mLockX, mLockY,
                     mLockWidth, mLockHeight,
                     0xFFFFFFFF,
                     ZPixmap);
  
  *aBits = mImage->data;
  
  *aWidthBytes = mImage->bytes_per_line;
  *aStride = mImage->bytes_per_line;

  return NS_OK;
}

NS_IMETHODIMP
nsDrawingSurfaceXlib::Unlock(void)
{
  if (!mLocked) {
    NS_ASSERTION(0, "attempting to unlock an DS that isn't locked");
    return NS_ERROR_FAILURE;
  }
  
  // If the lock was not read only, put the bits back on the pixmap
  if (!(mLockFlags & NS_LOCK_SURFACE_READ_ONLY)) {
    XPutImage(gDisplay, mPixmap, mGC, mImage,
              0, 0, mLockX, mLockY,
              mLockWidth, mLockHeight);
  }
  if (mImage) {
    XDestroyImage(mImage);
    mImage = nsnull;
  }
  mLocked = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsDrawingSurfaceXlib::GetDimensions(PRUint32 *aWidth, PRUint32 *aHeight)
{
  *aWidth = mWidth;
  *aHeight = mHeight;
  return NS_OK;
}

NS_IMETHODIMP
nsDrawingSurfaceXlib::IsOffscreen(PRBool *aOffScreen)
{
  *aOffScreen = mIsOffscreen;
  return NS_OK;
}

NS_IMETHODIMP
nsDrawingSurfaceXlib::IsPixelAddressable(PRBool *aAddressable)
{
  *aAddressable = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsDrawingSurfaceXlib::GetPixelFormat(nsPixelFormat *aFormat)
{
  *aFormat = mPixFormat;
  return NS_OK;
}
