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
 *   David Smith <david@igelaus.com.au>
 */

#include "nsDrawingSurfaceXlib.h"
#include "prlog.h"

#include "xlibrgb.h"      // for xlib_rgb_get_visual_info
#include <X11/Xutil.h>    // for XVisualInfo.

static NS_DEFINE_IID(kIDrawingSurfaceIID, NS_IDRAWING_SURFACE_IID);

static PRLogModuleInfo *DrawingSurfaceXlibLM = PR_NewLogModule("DrawingSurfaceXlib");

nsDrawingSurfaceXlib::nsDrawingSurfaceXlib()
{
  NS_INIT_REFCNT();
  PR_LOG(DrawingSurfaceXlibLM, PR_LOG_DEBUG, ("nsDrawingSurfaceXlib::nsDrawingSurfaceXlib()\n"));
  mDrawable = 0;
  mDestroyDrawable = PR_FALSE;
  mImage = nsnull;
  mDisplay = nsnull;
  mScreen = nsnull;
  mVisual = nsnull;
  mDepth = 0;
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
  XVisualInfo * x_visual_info = xlib_rgb_get_visual_info();

  NS_ASSERTION(nsnull != x_visual_info,"Visual info from xlibrgb is null.");

  if (nsnull != x_visual_info)
  {
    mPixFormat.mRedMask = x_visual_info->red_mask;
    mPixFormat.mGreenMask = x_visual_info->green_mask;;
    mPixFormat.mBlueMask = x_visual_info->blue_mask;;
    mPixFormat.mAlphaMask = 0;

    mPixFormat.mRedCount = ConvertMaskToCount(x_visual_info->red_mask);
    mPixFormat.mGreenCount = ConvertMaskToCount(x_visual_info->green_mask);
    mPixFormat.mBlueCount = ConvertMaskToCount(x_visual_info->blue_mask);;
    mPixFormat.mAlphaCount = 0;

    mPixFormat.mRedShift = GetShiftForMask(x_visual_info->red_mask);
    mPixFormat.mGreenShift = GetShiftForMask(x_visual_info->green_mask);
    mPixFormat.mBlueShift = GetShiftForMask(x_visual_info->blue_mask);
    mPixFormat.mAlphaShift = 0;
  }
}

nsDrawingSurfaceXlib::~nsDrawingSurfaceXlib()
{
  PR_LOG(DrawingSurfaceXlibLM, PR_LOG_DEBUG, ("nsDrawingSurfaceXlib::~nsDrawingSurfaceXlib()\n"));
  // if it's been labeled as destroy, it's a pixmap.
  if (mDestroyDrawable) {
    XFreePixmap(mDisplay, mDrawable);
  }
  if (mImage) {
    XDestroyImage(mImage);
  }

  // We are freeing the GC here now [See nsWidget::CreateGC()]
  if (mGC)
    XFreeGC(mDisplay, mGC);
}

NS_IMPL_QUERY_INTERFACE(nsDrawingSurfaceXlib, kIDrawingSurfaceIID)
NS_IMPL_ADDREF(nsDrawingSurfaceXlib)
NS_IMPL_RELEASE(nsDrawingSurfaceXlib)

NS_IMETHODIMP
nsDrawingSurfaceXlib::Init(Display * aDisplay,
                           Screen *  aScreen,
                           Visual *  aVisual,
                           int       aDepth,
                           Drawable  aDrawable, 
                           GC        aGC) 
{
  PR_LOG(DrawingSurfaceXlibLM, PR_LOG_DEBUG, ("nsDrawingSurfaceXlib::Init()\n"));

  mDisplay = aDisplay;
  mScreen = aScreen;
  mVisual = aVisual;
  mDepth = aDepth;
  // We Ignore the GC we are given, cos it is bad. [See nsWidget::CreateGC()]
#if 0
  mGC = aGC;
#endif
  mDrawable = aDrawable;

  // Create a new GC
  mGC = XCreateGC(mDisplay, mDrawable, 0, NULL);

  mIsOffscreen = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsDrawingSurfaceXlib::Init (Display * aDisplay,
                            Screen *  aScreen,
                            Visual *  aVisual,
                            int       aDepth,
                            GC        aGC,
                            PRUint32  aWidth, 
                            PRUint32  aHeight, 
                            PRUint32  aFlags) 
{
  mDisplay = aDisplay;
  mScreen = aScreen;
  mVisual = aVisual;
  mDepth = aDepth;
  // We Ignore the GC we are given, cos it is bad. [See nsWidget::Create()]
#if 0
  mGC = aGC;
#endif

  mWidth = aWidth;
  mHeight = aHeight;
  mLockFlags = aFlags;

  mIsOffscreen = PR_TRUE;

  mDrawable = XCreatePixmap(mDisplay, 
                            RootWindow(mDisplay, GetScreenNumber()),
                            mWidth, 
                            mHeight, 
                            mDepth);
  // Create a new GC
  mGC = XCreateGC(mDisplay, mDrawable, 0, NULL);

  return NS_OK;
}

NS_IMETHODIMP
nsDrawingSurfaceXlib::Lock(PRInt32 aX, PRInt32 aY,
                           PRUint32 aWidth, PRUint32 aHeight,
                           void **aBits, PRInt32 *aStride,
                           PRInt32 *aWidthBytes, PRUint32 aFlags)
{
  PR_LOG(DrawingSurfaceXlibLM, PR_LOG_DEBUG, ("nsDrawingSurfaceXlib::Lock()\n"));
  if (mLocked)
  {
    NS_ASSERTION(0, "nested lock attempt");
    return NS_ERROR_FAILURE;
  }
  if (aWidth == 0 || aHeight == 0)
  {
    NS_ASSERTION(0, "Width or Height is 0");
    return NS_ERROR_FAILURE;
  }

  mLocked = PR_TRUE;

  mLockX = aX;
  mLockY = aY;
  mLockWidth = aWidth;
  mLockHeight = aHeight;
  mLockFlags = aFlags;

  mImage = XGetImage(mDisplay, mDrawable,
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
  PR_LOG(DrawingSurfaceXlibLM, PR_LOG_DEBUG, ("nsDrawingSurfaceXlib::UnLock()\n"));
  if (!mLocked) {
    NS_ASSERTION(0, "attempting to unlock an DS that isn't locked");
    return NS_ERROR_FAILURE;
  }
  
  // If the lock was not read only, put the bits back on the pixmap
  if (!(mLockFlags & NS_LOCK_SURFACE_READ_ONLY)) {
    XPutImage(mDisplay, mDrawable, mGC, mImage,
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

PRUint8 
nsDrawingSurfaceXlib::ConvertMaskToCount(unsigned long val)
{
  PRUint8 retval = 0;
  PRUint8 cur_bit = 0;
  // walk through the number, incrementing the value if
  // the bit in question is set.
  while (cur_bit < (sizeof(unsigned long) * 8)) {
    if ((val >> cur_bit) & 0x1) {
      retval++;
    }
    cur_bit++;
  }
  return retval;
}

PRUint8 
nsDrawingSurfaceXlib::GetShiftForMask(unsigned long val)
{
  PRUint8 cur_bit = 0;
  // walk through the number, looking for the first 1
  while (cur_bit < (sizeof(unsigned long) * 8)) {
    if ((val >> cur_bit) & 0x1) {
      return cur_bit;
    }
    cur_bit++;
  }
  return cur_bit;
}
