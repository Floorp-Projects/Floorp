/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 *  Vladimir Vukicevic <vladimir@pobox.com>
 * Portions created by the Initial Developer are Copyright (C) 2004
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

#include "nsCairoDeviceContext.h"
#include "nsCairoDrawingSurface.h"

#if defined(MOZ_ENABLE_GTK2)
#include <sys/ipc.h>
#include <sys/shm.h>
#endif
#ifdef MOZ_ENABLE_XFT
#include <X11/Xft/Xft.h>
#endif


#include "nsMemory.h"

NS_IMPL_ISUPPORTS1(nsCairoDrawingSurface, nsIDrawingSurface)

nsCairoDrawingSurface::nsCairoDrawingSurface()
    : mSurface(nsnull), mImageSurface(nsnull)
{
#if defined(MOZ_ENABLE_GTK2) || defined(MOZ_ENABLE_XLIB)
    mPixmap = 0;
    mShmInfo.shmid = -1;
#ifdef MOZ_ENABLE_XFT
    mXftDraw = nsnull;
#endif
#endif
}

nsCairoDrawingSurface::~nsCairoDrawingSurface()
{
    fprintf (stderr, "++++ [%p] DESTROY\n", this);

    if (mSurface)
        cairo_surface_destroy (mSurface);
    if (mImageSurface && !mFastAccess) // otherwise, mImageSurface == mSurface
        cairo_surface_destroy (mImageSurface);

#if defined(MOZ_ENABLE_GTK2) || defined(MOZ_ENABLE_XLIB)
    if (mPixmap != 0)
        XFreePixmap(mXDisplay, mPixmap);

    if (mShmInfo.shmid != -1 && mShmInfo.shmaddr != 0) {
        XShmDetach (mXDisplay, &mShmInfo);
        shmdt (mShmInfo.shmaddr);
    }
#endif
}

nsresult
nsCairoDrawingSurface::Init(nsCairoDeviceContext *aDC, PRUint32 aWidth, PRUint32 aHeight, PRBool aFastAccess)
{
    NS_ASSERTION(mSurface == nsnull, "Surface already initialized!");
    NS_ASSERTION(aWidth > 0 && aHeight > 0, "Invalid surface dimensions!");

    mWidth = aWidth;
    mHeight = aHeight;
    mDC = aDC;

    if (aFastAccess) {
        fprintf (stderr, "++++ [%p] Creating IMAGE surface: %dx%d\n", this, aWidth, aHeight);
        mSurface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, aWidth, aHeight);
        mFastAccess = PR_TRUE;
        mDrawable = nsnull;
    } else {
        fprintf (stderr, "++++ [%p] Creating PIXMAP surface: %dx%d\n", this, aWidth, aHeight);
        // otherwise, we need to do toolkit-specific stuff
#if defined(MOZ_ENABLE_GTK2) || defined(MOZ_ENABLE_XLIB)
        mXDisplay = aDC->GetXDisplay();
#if 0
        mShmInfo.shmaddr = 0;
        mShmInfo.shmid = shmget (IPC_PRIVATE,
                                 aWidth * 4 * aHeight,
                                 IPC_CREAT | 0600);
        if (mShmInfo.shmid != -1) {
            mShmInfo.shmaddr = (char*) shmat (mShmInfo.shmid, 0, 0);
            mShmInfo.readOnly = False;
        }

        if (mShmInfo.shmid != -1 && mShmInfo.shmaddr != 0) {
            XShmAttach (mXDisplay, &mShmInfo);
            mPixmap = XShmCreatePixmap (mXDisplay, aDC->GetXPixmapParentDrawable(),
                                        mShmInfo.shmaddr, &mShmInfo,
                                        aWidth, aHeight, DefaultDepth(mXDisplay,DefaultScreen(mXDisplay)));
        } else {
#endif
            mPixmap = XCreatePixmap(mXDisplay,
                                    aDC->GetXPixmapParentDrawable(),
                                    aWidth, aHeight, DefaultDepth(mXDisplay,DefaultScreen(mXDisplay)));
#if 0
        }
#endif

        mDrawable = (Drawable)mPixmap;


        // clear the pixmap
        XGCValues gcv;
        gcv.foreground = WhitePixel(mXDisplay,DefaultScreen(mXDisplay));
        gcv.background = 0;
        GC gc = XCreateGC (mXDisplay, mPixmap, GCForeground | GCBackground, &gcv);
        XFillRectangle (mXDisplay, mPixmap, gc, 0, 0, aWidth, aHeight);
        XFreeGC (mXDisplay, gc);

        // create the surface
        mSurface = cairo_xlib_surface_create (mXDisplay,
                                              mPixmap,
                                              aDC->GetXVisual(),
                                              CAIRO_FORMAT_ARGB32,
                                              aDC->GetXColormap());

        mFastAccess = PR_FALSE;
#else
#error write me
#endif
    }

    mLockFlags = 0;

    return NS_OK;
}

nsresult
nsCairoDrawingSurface::Init (nsCairoDeviceContext *aDC, nsIWidget *aWidget)
{
    nsNativeWidget nativeWidget = aWidget->GetNativeData(NS_NATIVE_WIDGET);
    fprintf (stderr, "++++ [%p] Creating DRAWABLE (0x%08x) surface\n", this, nativeWidget);

    mDC = aDC;

#ifdef MOZ_ENABLE_GTK2
    mDrawable = GDK_DRAWABLE_XID(GDK_DRAWABLE(nativeWidget));
    NS_ASSERTION (GDK_IS_WINDOW(nativeWidget), "unsupported native widget type!");
    mSurface = cairo_xlib_surface_create
        (GDK_WINDOW_XDISPLAY(GDK_DRAWABLE(nativeWidget)),
         GDK_WINDOW_XWINDOW(GDK_DRAWABLE(nativeWidget)),
         GDK_VISUAL_XVISUAL(gdk_drawable_get_visual(GDK_DRAWABLE(nativeWidget))),
         CAIRO_FORMAT_ARGB32, // I hope!
         GDK_COLORMAP_XCOLORMAP(gdk_drawable_get_colormap(GDK_DRAWABLE(nativeWidget))));

    Window root_ignore;
    int x_ignore, y_ignore;
    unsigned int bwidth_ignore, width, height, depth;

    XGetGeometry(GDK_WINDOW_XDISPLAY(GDK_DRAWABLE(nativeWidget)),
                 GDK_WINDOW_XWINDOW(GDK_DRAWABLE(nativeWidget)),
                 &root_ignore, &x_ignore, &y_ignore,
                 &width, &height,
                 &bwidth_ignore, &depth);

#if 0
    if (depth != 32)
        fprintf (stderr, "**** nsCairoDrawingSurface::Init with Widget: depth is %d!\n", depth);
#endif

    mWidth = width;
    mHeight = height;
    mFastAccess = PR_FALSE;

#else
#error write me
#endif

    mPixmap = 0;
    mLockFlags = 0;

    return NS_OK;
}

NS_IMETHODIMP
nsCairoDrawingSurface::Lock (PRInt32 aX, PRInt32 aY, PRUint32 aWidth, PRUint32 aHeight,
                             void **aBits, PRInt32 *aStride, PRInt32 *aWidthBytes,
                             PRUint32 aFlags)
{
    NS_ASSERTION(aX + aWidth <= mWidth, "Invalid aX/aWidth");
    NS_ASSERTION(aY + aHeight <= mHeight, "Invalid aY/aHeight");
    NS_ASSERTION(mLockFlags == 0, "nsCairoDrawingSurface::Lock while surface is already locked!");

    if (!mFastAccess) {
        mImageSurface = cairo_surface_get_image (mSurface);
    }

    char *data;
    int width, height, stride, depth;

    if (cairo_image_surface_get_data (mImageSurface,
                                      &data, &width, &height, &stride, &depth)
        != 0)
    {
        /* Something went wrong */
        if (!mFastAccess) {
            cairo_surface_destroy(mImageSurface);
            mImageSurface = nsnull;
        }
        return NS_ERROR_FAILURE;
    }

    *aBits = data + (stride * aY) + (aX * (depth / 8));
    *aStride = stride;
    *aWidthBytes = width * (depth / 8);

    mLockFlags = 0;

    return NS_OK;
}

NS_IMETHODIMP
nsCairoDrawingSurface::Unlock (void)
{
    NS_ASSERTION(mLockFlags != 0, "nsCairoDrawingSurface::Unlock on non-locked surface!");

    if (mFastAccess) {
        mLockFlags = 0;
        return NS_OK;
    }

    if (mLockFlags & NS_LOCK_SURFACE_WRITE_ONLY) {
        /* need to copy back */
        cairo_surface_set_image (mSurface, mImageSurface);
    }

    cairo_surface_destroy (mImageSurface);
    mImageSurface = nsnull;
    mLockFlags = 0;

    return NS_OK;
}

NS_IMETHODIMP
nsCairoDrawingSurface::GetDimensions (PRUint32 *aWidth, PRUint32 *aHeight)
{
    *aWidth = mWidth;
    *aHeight = mHeight;
    return NS_OK;
}

NS_IMETHODIMP
nsCairoDrawingSurface::IsOffscreen(PRBool *aOffScreen)
{
    *aOffScreen = PR_FALSE;

    if (mFastAccess)
        *aOffScreen = PR_TRUE;
#if defined(MOZ_ENABLE_GTK2) || defined(MOZ_ENABLE_XLIB)
    else if (mPixmap)
        *aOffScreen = PR_TRUE;
#endif

    return NS_OK;
}

NS_IMETHODIMP
nsCairoDrawingSurface::IsPixelAddressable(PRBool *aAddressable)
{
    *aAddressable = mFastAccess;
    return NS_OK;
}

NS_IMETHODIMP
nsCairoDrawingSurface::GetPixelFormat(nsPixelFormat *aFormat)
{
    aFormat->mRedMask = 0x000000ff;
    aFormat->mGreenMask = 0x0000ff00;
    aFormat->mBlueMask = 0x00ff0000;
    aFormat->mAlphaMask = 0xff000000;

    aFormat->mRedCount = 8;
    aFormat->mGreenCount = 8;
    aFormat->mBlueCount = 8;
    aFormat->mAlphaCount = 8;

    aFormat->mRedShift = 0;
    aFormat->mGreenShift = 8;
    aFormat->mBlueShift = 16;
    aFormat->mAlphaCount = 24;

    return NS_OK;
}

#ifdef MOZ_ENABLE_XFT
XftDraw *
nsCairoDrawingSurface::GetXftDraw(void)
{
    if (mDrawable == nsnull) {
        NS_ERROR("GetXftDraw with null drawable!\n");
    }

    if (!mXftDraw) {
        fprintf (stderr, "++++ [%p] Creating XFTDRAW for (0x%08x)\n", this, mDrawable);

        mXftDraw = XftDrawCreate(mDC->GetXDisplay(), mDrawable,
                                 mDC->GetXVisual(),
                                 mDC->GetXColormap());
    }

    return mXftDraw;
}

void
nsCairoDrawingSurface::GetLastXftClip(nsIRegion **aLastRegion)
{
    *aLastRegion = mLastXftClip.get();
    NS_IF_ADDREF(*aLastRegion);
}

void
nsCairoDrawingSurface::SetLastXftClip(nsIRegion *aLastRegion)
{
    mLastXftClip = aLastRegion;
}

#endif /* MOZ_ENABLE_XFT */
