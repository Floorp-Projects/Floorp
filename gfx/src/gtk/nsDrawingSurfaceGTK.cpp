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


#include <gdk/gdkx.h>
#include <gdk/gdkprivate.h>
#include "nsDrawingSurfaceGTK.h"

NS_IMPL_ISUPPORTS2(nsDrawingSurfaceGTK, nsIDrawingSurface, nsIDrawingSurfaceGTK)

//#define CHEAP_PERFORMANCE_MEASUREMENT

#ifdef CHEAP_PERFORMANCE_MEASUREMENT
static PRTime mLockTime, mUnlockTime;
#endif

nsDrawingSurfaceGTK :: nsDrawingSurfaceGTK()
{
  NS_INIT_REFCNT();
  GdkVisual *v;

  mPixmap = nsnull;
  mGC = nsnull;
  mDepth = 0;
  mWidth = 0;
  mHeight = 0;
  mFlags = 0;

  mImage = nsnull;
  mLockWidth = 0;
  mLockHeight = 0;
  mLockFlags = 0;
  mLockX = 0;
  mLockY = 0;
  mLocked = PR_FALSE;

  v = ::gdk_rgb_get_visual();

  mPixFormat.mRedMask = v->red_mask;
  mPixFormat.mGreenMask = v->green_mask;
  mPixFormat.mBlueMask = v->blue_mask;
  // FIXME
  mPixFormat.mAlphaMask = 0;

  mPixFormat.mRedCount = ConvertMaskToCount(v->red_mask);
  mPixFormat.mGreenCount = ConvertMaskToCount(v->green_mask);
  mPixFormat.mBlueCount = ConvertMaskToCount(v->blue_mask);;


  mPixFormat.mRedShift = v->red_shift;
  mPixFormat.mGreenShift = v->green_shift;
  mPixFormat.mBlueShift = v->blue_shift;
  // FIXME
  mPixFormat.mAlphaShift = 0;

  mDepth = v->depth;
}

nsDrawingSurfaceGTK :: ~nsDrawingSurfaceGTK()
{
  if (mPixmap)
    ::gdk_pixmap_unref(mPixmap);

  if (mImage)
    ::gdk_image_destroy(mImage);

  if (mGC)
    gdk_gc_unref(mGC);
}

/**
 * Lock a rect of a drawing surface and return a
 * pointer to the upper left hand corner of the
 * bitmap.
 * @param  aX x position of subrect of bitmap
 * @param  aY y position of subrect of bitmap
 * @param  aWidth width of subrect of bitmap
 * @param  aHeight height of subrect of bitmap
 * @param  aBits out parameter for upper left hand
 *         corner of bitmap
 * @param  aStride out parameter for number of bytes
 *         to add to aBits to go from scanline to scanline
 * @param  aWidthBytes out parameter for number of
 *         bytes per line in aBits to process aWidth pixels
 * @return error status
 *
 **/
NS_IMETHODIMP nsDrawingSurfaceGTK :: Lock(PRInt32 aX, PRInt32 aY,
                                          PRUint32 aWidth, PRUint32 aHeight,
                                          void **aBits, PRInt32 *aStride,
                                          PRInt32 *aWidthBytes, PRUint32 aFlags)
{
#ifdef CHEAP_PERFORMANCE_MEASUREMENT
  mLockTime = PR_Now();
  //  MOZ_TIMER_RESET(mLockTime);
  //  MOZ_TIMER_START(mLockTime);
#endif

#if 0
  g_print("nsDrawingSurfaceGTK::Lock() called\n" \
          "  aX = %i, aY = %i,\n" \
          "  aWidth = %i, aHeight = %i,\n" \
          "  aBits, aStride, aWidthBytes,\n" \
          "  aFlags = %i\n", aX, aY, aWidth, aHeight, aFlags);
#endif

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

  // Obtain an ximage from the pixmap.
  mImage = ::gdk_image_get(mPixmap, mLockX, mLockY, mLockWidth, mLockHeight);

  *aBits = GDK_IMAGE_XIMAGE(mImage)->data;

  *aWidthBytes = GDK_IMAGE_XIMAGE(mImage)->bytes_per_line;
  *aStride = GDK_IMAGE_XIMAGE(mImage)->bytes_per_line;

#ifdef CHEAP_PERFORMANCE_MEASUREMENT
  //  MOZ_TIMER_STOP(mLockTime);
  //  MOZ_TIMER_LOG(("Time taken to lock: "));
  //  MOZ_TIMER_PRINT(mLockTime);
  printf("Time taken to lock:   %d\n", PR_Now() - mLockTime);
#endif

  return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfaceGTK :: Unlock(void)
{

#ifdef CHEAP_PERFORMANCE_MEASUREMENT
  mUnlockTime = PR_Now();
#endif

  //  g_print("nsDrawingSurfaceGTK::UnLock() called\n");
  if (!mLocked)
  {
    NS_ASSERTION(0, "attempting to unlock an DS that isn't locked");
    return NS_ERROR_FAILURE;
  }

  // If the lock was not read only, put the bits back on the pixmap
  if (!(mLockFlags & NS_LOCK_SURFACE_READ_ONLY))
  {
#if 0
    g_print("%p gdk_draw_image(pixmap=%p,lockx=%d,locky=%d,lockw=%d,lockh=%d)\n",
            this,
            mPixmap,
            mLockX, mLockY,
            mLockWidth, mLockHeight);
#endif

    gdk_draw_image(mPixmap,
                   mGC,
                   mImage,
                   0, 0,
                   mLockX, mLockY,
                   mLockWidth, mLockHeight);
  }

  if (mImage)
    ::gdk_image_destroy(mImage);
  mImage = nsnull;

  mLocked = PR_FALSE;


#ifdef CHEAP_PERFORMANCE_MEASUREMENT
  printf("Time taken to unlock: %d\n", PR_Now() - mUnlockTime);
#endif

  return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfaceGTK :: GetDimensions(PRUint32 *aWidth, PRUint32 *aHeight)
{
  *aWidth = mWidth;
  *aHeight = mHeight;

  return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfaceGTK :: IsOffscreen(PRBool *aOffScreen)
{
  *aOffScreen = mIsOffscreen;
  return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfaceGTK :: IsPixelAddressable(PRBool *aAddressable)
{
// FIXME
  *aAddressable = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfaceGTK :: GetPixelFormat(nsPixelFormat *aFormat)
{
  *aFormat = mPixFormat;

  return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfaceGTK :: Init(GdkDrawable *aDrawable, GdkGC *aGC)
{
  if (mGC)
    gdk_gc_unref(mGC);

  mGC = gdk_gc_ref(aGC);
  mPixmap = aDrawable;

  mWidth  = ((GdkWindowPrivate*)aDrawable)->width;
  mHeight = ((GdkWindowPrivate*)aDrawable)->height;

  // XXX was i smoking crack when i wrote this comment?
  // this is definatly going to be on the screen, as it will be the window of a
  // widget or something.
  mIsOffscreen = PR_FALSE;

  if (mImage)
    gdk_image_destroy(mImage);
  mImage = nsnull;

  g_return_val_if_fail(mPixmap != nsnull, NS_ERROR_FAILURE);
  
  return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfaceGTK :: Init(GdkGC *aGC, PRUint32 aWidth,
                                          PRUint32 aHeight, PRUint32 aFlags)
{
  //  ::g_return_val_if_fail (aGC != nsnull, NS_ERROR_FAILURE);
  //  ::g_return_val_if_fail ((aWidth > 0) && (aHeight > 0), NS_ERROR_FAILURE);
  if (mGC)
    gdk_gc_unref(mGC);

  mGC = gdk_gc_ref(aGC);
  mWidth = aWidth;
  mHeight = aHeight;
  mFlags = aFlags;

  // we can draw on this offscreen because it has no parent
  mIsOffscreen = PR_TRUE;

  mPixmap = ::gdk_pixmap_new(nsnull, mWidth, mHeight, mDepth);

  if (mImage)
    gdk_image_destroy(mImage);
  mImage = nsnull;

  return NS_OK;
}

/* inline */
PRUint8 
nsDrawingSurfaceGTK::ConvertMaskToCount(unsigned long val)
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
