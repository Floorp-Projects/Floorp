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

#if defined (HAVE_IPC_H) && defined (HAVE_SHM_H) && defined (HAVE_XSHM_H)
#define USE_SHM
#endif

//#define USE_SHM

#include <gdk/gdkx.h>
#include <gdk/gdkprivate.h>
#ifdef USE_SHM
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>
#endif /* USE_SHM */
#include "nsDrawingSurfaceGTK.h"

static NS_DEFINE_IID(kIDrawingSurfaceIID, NS_IDRAWING_SURFACE_IID);
static NS_DEFINE_IID(kIDrawingSurfaceGTKIID, NS_IDRAWING_SURFACE_GTK_IID);

nsDrawingSurfaceGTK :: nsDrawingSurfaceGTK()
{
  NS_INIT_REFCNT();
  GdkVisual *v;

  mPixmap = nsnull;
  mGC = nsnull;
  mDepth = 0;
  mWidth = mHeight = 0;
  mFlags = 0;

  mImage = nsnull;
  mLockWidth = mLockHeight = 0;
  mLockFlags = 0;
  mLocked = PR_FALSE;

  v = ::gdk_rgb_get_visual();

  mPixFormat.mRedMask = v->red_mask;
  mPixFormat.mGreenMask = v->green_mask;
  mPixFormat.mBlueMask = v->blue_mask;
  // FIXME
  mPixFormat.mAlphaMask = 0;

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
}

NS_IMETHODIMP nsDrawingSurfaceGTK :: QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr)
    return NS_ERROR_NULL_POINTER;

  if (aIID.Equals(kIDrawingSurfaceIID))
  {
    nsIDrawingSurface* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }

  if (aIID.Equals(kIDrawingSurfaceGTKIID))
  {
    nsDrawingSurfaceGTK* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }

  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

  if (aIID.Equals(kISupportsIID))
  {
    nsIDrawingSurface* tmp = this;
    nsISupports* tmp2 = tmp;
    *aInstancePtr = (void*) tmp2;
    NS_ADDREF_THIS();
    return NS_OK;
  }

  return NS_NOINTERFACE;
}

NS_IMPL_ADDREF(nsDrawingSurfaceGTK);
NS_IMPL_RELEASE(nsDrawingSurfaceGTK);


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
#ifdef USE_SHM
  if (gdk_get_use_xshm())
  {
    mImage = gdk_image_new(GDK_IMAGE_FASTEST,
                           gdk_rgb_get_visual(),
                           mLockWidth,
                           mLockHeight);
    
    XShmGetImage(GDK_DISPLAY(),
                 GDK_WINDOW_XWINDOW(mPixmap),
                 GDK_IMAGE_XIMAGE(mImage),
                 mLockX, mLockY,
                 0xFFFFFFFF);

    gdk_flush();
  }
  else
  {
#endif /* USE_SHM */
    mImage = ::gdk_image_get(mPixmap, mLockX, mLockY, mLockWidth, mLockHeight);
#ifdef USE_SHM
  }
#endif /* USE_SHM */

  *aBits = GDK_IMAGE_XIMAGE(mImage)->data;

  *aWidthBytes = GDK_IMAGE_XIMAGE(mImage)->bytes_per_line;
  *aStride = GDK_IMAGE_XIMAGE(mImage)->bytes_per_line;


#if 0
  int bytes_per_line = GDK_IMAGE_XIMAGE(mImage)->bytes_per_line;

  //
  // All this code is a an attempt to set the stride width properly.
  // Needs to be cleaned up alot.  For now, it will only work in the
  // case where aWidthBytes and aStride are the same.  One is assigned to 
  // the other.
  // 

  *aWidthBytes = mImage->bpl;
  *aStride = mImage->bpl;

  int width_in_pixels = *aWidthBytes << 8;


  int bitmap_pad = GDK_IMAGE_XIMAGE(mImage)->bitmap_pad;
  int depth = GDK_IMAGE_XIMAGE(mImage)->depth;

#define RASWIDTH8(width, bpp) (width)
#define RASWIDTH16(width, bpp) ((((width) * (bpp) + 15) >> 4) << 1)
#define RASWIDTH32(width, bpp) ((((width) * (bpp) + 31) >> 5) << 2)

  switch(bitmap_pad)
    {
    case 8:
      *aStride = RASWIDTH8(aWidth,bitmap_pad);
      break;

    case 16:
      *aStride = bytes_per_line;
      *aStride = RASWIDTH16(aWidth,bitmap_pad);
      break;

    case 32:
      *aStride = bytes_per_line;
      *aStride = RASWIDTH32(aWidth,bitmap_pad);
      break;

    default:

      NS_ASSERTION(nsnull,"something got screwed");

    }

  *aStride = (*aWidthBytes) + ((bitmap_pad >> 3) - 1);

  GDK_IMAGE_XIMAGE(mImage)->bitmap_pad;

  *aWidthBytes = mImage->bpl;
#endif
  return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfaceGTK :: Unlock(void)
{
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
    g_print("gdk_draw_image(pixmap=%p,lockx=%d,locky=%d,lockw=%d,lockh=%d)\n",
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

  // FIXME if we are using shared mem, we shouldn't destroy the image...
  if (mImage)
    ::gdk_image_destroy(mImage);
  mImage = nsnull;

  mLocked = PR_FALSE;

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
  mGC = aGC;
  mPixmap = aDrawable;
// this is definatly going to be on the screen, as it will be the window of a
// widget or something.
  mIsOffscreen = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfaceGTK :: Init(GdkGC *aGC, PRUint32 aWidth,
                                          PRUint32 aHeight, PRUint32 aFlags)
{
//  ::g_return_val_if_fail (aGC != nsnull, NS_ERROR_FAILURE);
//  ::g_return_val_if_fail ((aWidth > 0) && (aHeight > 0), NS_ERROR_FAILURE);

  mGC = aGC;
  mWidth = aWidth;
  mHeight = aHeight;
  mFlags = aFlags;

// we can draw on this offscreen because it has no parent
  mIsOffscreen = PR_TRUE;

  mPixmap = ::gdk_pixmap_new(nsnull, mWidth, mHeight, mDepth);

  return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfaceGTK :: GetGC(GdkGC *aGC)
{
  aGC = ::gdk_gc_ref(mGC);
  return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfaceGTK :: ReleaseGC(void)
{
  ::gdk_gc_unref(mGC);
  return NS_OK;
}

/* below are utility functions used mostly for nsRenderingContext and nsImage
 * to plug into gdk_* functions for drawing.  You should not set a pointer
 * that might hang around with the return from these.  instead use the ones
 * above.  pav
 */
GdkGC *nsDrawingSurfaceGTK::GetGC(void)
{
  return mGC;
}

GdkDrawable *nsDrawingSurfaceGTK::GetDrawable(void)
{
  return mPixmap;
}

