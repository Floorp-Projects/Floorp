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

#include <gtk/gtk.h>

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
#ifdef NS_GTK_REF
  if (mPixmap)
    ::gdk_pixmap_unref(mPixmap);
#endif
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

NS_IMETHODIMP nsDrawingSurfaceGTK :: Lock(PRInt32 aX, PRInt32 aY,
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

/*
 i see. it might be that this does something that just isn't
 possible directly with x11. you probably have to copy from the
 gdkpixmap to a local gdkimage in lock(), do whatever is done to
 the bits, and copy back in unlock(). if the display is local,
 and shared pixmaps are supported, you don't need to do that.
*/

  mImage = ::gdk_image_get(mPixmap, mLockX, mLockY, mLockWidth, mLockHeight);
 
 // aBits = &mImage->mem;
 // FIXME

  return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfaceGTK :: Unlock(void)
{
  if (!mLocked)
  {
    NS_ASSERTION(0, "attempting to unlock an DS that isn't locked");
    return NS_ERROR_FAILURE;
  }
/*
 //if it is writeable, we are going to want to redraw the pixmap from the image.
// my bit looking/mangling skills are asleep.. someone do this the right way
  if (!(mFlags & NS_LOCK_SURFACE_READ_ONLY))
  {
    gdk_draw_image(mPixmap,
		   mGC,
		   mImage,
		   0, 0,
		   mLockX, mLockY,
		   mLockWidth, mLockHeight);

  }
*/
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
  g_print("pixmap is not going to be freed\n");

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

GdkGC *nsDrawingSurfaceGTK::GetGC(void)
{
  return mGC;
}

/* below are utility functions used mostly for nsRenderingContext and nsImage
 * to plug into gdk_* functions for drawing.  You should not set a pointer
 * that might hang around with the return from these.  instead use the ones
 * above.  pav
 */
GdkDrawable *nsDrawingSurfaceGTK::GetDrawable(void)
{
  return mPixmap;
}
