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
 *   David Smith <david@igelaus.com.au>
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

#include "nsDrawingSurfaceXlib.h"
#include "prlog.h"
#include "nsGCCache.h"

#include "xlibrgb.h"      // for xxlib_rgb_get_visual_info

#ifdef PR_LOGGING 
static PRLogModuleInfo *DrawingSurfaceXlibLM = PR_NewLogModule("DrawingSurfaceXlib");
#endif /* PR_LOGGING */ 

nsDrawingSurfaceXlibImpl::nsDrawingSurfaceXlibImpl()
{
  NS_INIT_REFCNT();
  PR_LOG(DrawingSurfaceXlibLM, PR_LOG_DEBUG, ("nsDrawingSurfaceXlibImpl::nsDrawingSurfaceXlibImpl()\n"));
  mDrawable = 0;
  mDestroyDrawable = PR_FALSE;
  mImage = nsnull;
  mXlibRgbHandle = nsnull;
  mDisplay = nsnull;
  mScreen = nsnull;
  mVisual = nsnull;
  mDepth = 0;
  mGC = nsnull;
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
}

nsDrawingSurfaceXlibImpl::~nsDrawingSurfaceXlibImpl()
{
  PR_LOG(DrawingSurfaceXlibLM, PR_LOG_DEBUG, ("nsDrawingSurfaceXlibImpl::~nsDrawingSurfaceXlibImpl()\n"));
  // if it's been labeled as destroy, it's a pixmap.
  if (mDestroyDrawable) {
    XFreePixmap(mDisplay, mDrawable);
  }
  if (mImage) {
    XDestroyImage(mImage);
  }

  // We are freeing the GC here now [See nsWidget::CreateGC()]
  if(mGC) {
    mGC->Release();
    mGC = nsnull;
  }
}

NS_IMPL_ISUPPORTS1(nsDrawingSurfaceXlibImpl, nsIDrawingSurfaceXlib)

NS_IMETHODIMP
nsDrawingSurfaceXlibImpl::Init(XlibRgbHandle *aXlibRgbHandle,
                               Drawable       aDrawable, 
                               xGC           *aGC) 
{
  PR_LOG(DrawingSurfaceXlibLM, PR_LOG_DEBUG, ("nsDrawingSurfaceXlibImpl::Init()\n"));

  mXlibRgbHandle = aXlibRgbHandle;
  mDrawable      = aDrawable;

  CommonInit();
  
  if (mGC)
    mGC->Release();
  mGC = aGC;
  mGC->AddRef();
  
  mIsOffscreen = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsDrawingSurfaceXlibImpl::Init(XlibRgbHandle *aXlibRgbHandle,
                               xGC     * aGC,
                               PRUint32  aWidth, 
                               PRUint32  aHeight, 
                               PRUint32  aFlags) 
{
  mXlibRgbHandle = aXlibRgbHandle;
  mWidth = aWidth;
  mHeight = aHeight;
  mLockFlags = aFlags;

  CommonInit();
  
  if (mGC)
    mGC->Release();
  mGC = aGC;
  mGC->AddRef();
  
  mIsOffscreen = PR_TRUE;

  mDrawable = XCreatePixmap(mDisplay, 
                            XRootWindow(mDisplay, XScreenNumberOfScreen(mScreen)),
                            mWidth, 
                            mHeight, 
                            mDepth);
  return NS_OK;
}

void 
nsDrawingSurfaceXlibImpl::CommonInit()
{
  mDisplay = xxlib_rgb_get_display(mXlibRgbHandle);
  mScreen  = xxlib_rgb_get_screen(mXlibRgbHandle);
  mVisual  = xxlib_rgb_get_visual(mXlibRgbHandle);
  mDepth   = xxlib_rgb_get_depth(mXlibRgbHandle);

  XVisualInfo *x_visual_info = xxlib_rgb_get_visual_info(mXlibRgbHandle);
  NS_ASSERTION(nsnull != x_visual_info, "Visual info from xlibrgb is null.");

  if (x_visual_info)
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

NS_IMETHODIMP
nsDrawingSurfaceXlibImpl::Lock(PRInt32 aX, PRInt32 aY,
                               PRUint32 aWidth, PRUint32 aHeight,
                               void **aBits, PRInt32 *aStride,
                               PRInt32 *aWidthBytes, PRUint32 aFlags)
{
  PR_LOG(DrawingSurfaceXlibLM, PR_LOG_DEBUG, ("nsDrawingSurfaceXlibImpl::Lock()\n"));
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
nsDrawingSurfaceXlibImpl::Unlock(void)
{
  PR_LOG(DrawingSurfaceXlibLM, PR_LOG_DEBUG, ("nsDrawingSurfaceXlibImpl::UnLock()\n"));
  if (!mLocked) {
    NS_ASSERTION(0, "attempting to unlock an DS that isn't locked");
    return NS_ERROR_FAILURE;
  }
  
  // If the lock was not read only, put the bits back on the pixmap
  if (!(mLockFlags & NS_LOCK_SURFACE_READ_ONLY)) {
    XPutImage(mDisplay, mDrawable, *mGC, mImage,
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
nsDrawingSurfaceXlibImpl::GetDimensions(PRUint32 *aWidth, PRUint32 *aHeight)
{
  *aWidth = mWidth;
  *aHeight = mHeight;
  return NS_OK;
}

NS_IMETHODIMP
nsDrawingSurfaceXlibImpl::IsOffscreen(PRBool *aOffScreen)
{
  *aOffScreen = mIsOffscreen;
  return NS_OK;
}

NS_IMETHODIMP
nsDrawingSurfaceXlibImpl::IsPixelAddressable(PRBool *aAddressable)
{
  *aAddressable = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsDrawingSurfaceXlibImpl::GetPixelFormat(nsPixelFormat *aFormat)
{
  *aFormat = mPixFormat;
  return NS_OK;
}

PRUint8 
nsDrawingSurfaceXlibImpl::ConvertMaskToCount(unsigned long val)
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
nsDrawingSurfaceXlibImpl::GetShiftForMask(unsigned long val)
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
