/* -*- Mode: C++; tab-width: 50; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is thebes gfx
 *
 * The Initial Developer of the Original Code is 
 * mozilla.org
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Vladimir Vukicevic <vladimir@pobox.com>
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

#include "nsThebesDrawingSurface.h"
#include "nsThebesDeviceContext.h"

#include "nsMemory.h"

#include "gfxImageSurface.h"

#ifdef MOZ_ENABLE_GTK2
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include "gfxXlibSurface.h"
# ifdef MOZ_ENABLE_GLITZ
# include "gfxGlitzSurface.h"
# include "glitz-glx.h"
# endif
#elif XP_WIN
#include "gfxWindowsSurface.h"
#endif

#include <stdlib.h>

PRInt32 nsThebesDrawingSurface::mGlitzMode = -1;

NS_IMPL_ISUPPORTS1(nsThebesDrawingSurface, nsIDrawingSurface)

nsThebesDrawingSurface::nsThebesDrawingSurface()
{
}

nsThebesDrawingSurface::~nsThebesDrawingSurface()
{
#ifdef WIN_XP
    if (mWidget)
        mWidget->FreeNativeData(mNativeWidget, NS_NATIVE_GRAPHIC);
#endif
}

#ifdef MOZ_ENABLE_GTK2
static cairo_user_data_key_t cairo_gtk_pixmap_unref_key;
static void do_gtk_pixmap_unref (void *data)
{
    GdkPixmap *pmap = (GdkPixmap*)data;
    guint rc = ((GObject*)pmap)->ref_count;
    //fprintf (stderr, "do_gtk_pixmap_unref: %p refcnt %d\n", pmap, rc);
    gdk_pixmap_unref (pmap);
}
#endif

nsresult
nsThebesDrawingSurface::Init(nsThebesDeviceContext *aDC, PRUint32 aWidth, PRUint32 aHeight, PRBool aFastAccess)
{
    NS_ASSERTION(mSurface == nsnull, "Surface already initialized!");
    NS_ASSERTION(aWidth > 0 && aHeight > 0, "Invalid surface dimensions!");

    PR_LOG(gThebesGFXLog, PR_LOG_DEBUG, ("## %p nsThebesDrawingSurface::Init aDC %p w %d h %d fast %d\n", this, aDC, aWidth, aHeight, aFastAccess));

    mWidth = aWidth;
    mHeight = aHeight;
    mDC = aDC;
    mNativeWidget = nsnull;

#if defined(MOZ_ENABLE_GTK2)
    if (aFastAccess) {
        //fprintf (stderr, "## nsThebesDrawingSurface::Init gfxImageSurface %d %d\n", aWidth, aHeight);
        mSurface = new gfxImageSurface(gfxImageSurface::ImageFormatARGB32, aWidth, aHeight);
    } else {
        if (!UseGlitz()) {
            mNativeWidget = ::gdk_pixmap_new(nsnull, mWidth, mHeight, 24);
            {
                guint rc = ((GObject*)mNativeWidget)->ref_count;
                //fprintf (stderr, "do_gtk_pixmap_new: %p refcnt %d\n", mNativeWidget, rc);
            }
            gdk_drawable_set_colormap(GDK_DRAWABLE(mNativeWidget), gdk_rgb_get_colormap());

            mSurface = new gfxXlibSurface(GDK_WINDOW_XDISPLAY(GDK_DRAWABLE(mNativeWidget)),
                                          GDK_WINDOW_XWINDOW(GDK_DRAWABLE(mNativeWidget)),
                                          GDK_VISUAL_XVISUAL(gdk_drawable_get_visual(GDK_DRAWABLE(mNativeWidget))));

            // we need some thebes wrappers for surface destructor hooks
            cairo_surface_set_user_data (mSurface->CairoSurface(),
                                         &cairo_gtk_pixmap_unref_key,
                                         mNativeWidget,
                                         do_gtk_pixmap_unref);
            
            //mSurface = new gfxXlibSurface(GDK_DISPLAY(), GDK_VISUAL_XVISUAL(gdk_rgb_get_visual()), aWidth, aHeight);
        } else {
# if defined(MOZ_ENABLE_GLITZ)
            glitz_drawable_format_t *gdformat = (glitz_drawable_format_t*) aDC->GetGlitzDrawableFormat();
            glitz_drawable_t *gdraw =
                glitz_glx_create_pbuffer_drawable (GDK_DISPLAY(),
                                                   DefaultScreen(GDK_DISPLAY()),
                                                   gdformat,
                                                   aWidth,
                                                   aHeight);
            glitz_format_t *gformat =
                glitz_find_standard_format (gdraw, GLITZ_STANDARD_ARGB32);
            glitz_surface_t *gsurf =
                glitz_surface_create (gdraw,
                                      gformat,
                                      aWidth,
                                      aHeight,
                                      0,
                                      NULL);
            glitz_surface_attach (gsurf, gdraw, GLITZ_DRAWABLE_BUFFER_FRONT_COLOR);

            //fprintf (stderr, "## nsThebesDrawingSurface::Init Glitz PBUFFER %d %d\n", aWidth, aHeight);
            mSurface = new gfxGlitzSurface (gdraw, gsurf, PR_TRUE);
# endif
        }
    }
#elif XP_WIN
    if (aFastAccess) {
        mSurface = new gfxImageSurface(gfxImageSurface::ImageFormatARGB32, aWidth, aHeight);
      } else {
        // this may or may not be null.  this is currently only called from a spot
        // where this will be a hwnd
        nsNativeWidget widget = aDC->GetWidget();
        HDC dc = nsnull;
        if (widget)
            dc = GetDC((HWND)widget);

        mSurface = new gfxWindowsSurface(dc, aWidth, aHeight);

        if (widget)
            ReleaseDC((HWND)widget, dc);

    }
#else
#error Write me!
#endif

    return NS_OK;
}

nsresult
nsThebesDrawingSurface::Init (nsThebesDeviceContext *aDC, nsIWidget *aWidget)
{
    PR_LOG(gThebesGFXLog, PR_LOG_DEBUG, ("## %p nsThebesDrawingSurface::Init aDC %p aWidget %p\n", this, aDC, aWidget));

#ifdef MOZ_ENABLE_GTK2
    mNativeWidget = aWidget->GetNativeData(NS_NATIVE_WIDGET);
#elif XP_WIN
    mWidget = aWidget; // hold a pointer to this.. not a reference.. because we have to free the dc...
    mNativeWidget = aWidget->GetNativeData(NS_NATIVE_GRAPHIC);
#else
#error Write me!
#endif

    Init(aDC, mNativeWidget);

    return NS_OK;
}

nsresult
nsThebesDrawingSurface::Init (nsThebesDeviceContext *aDC, nsNativeWidget aWidget)
{
    mDC = aDC;
    mNativeWidget = aWidget;
    mWidth = 0;
    mHeight = 0;

#ifdef MOZ_ENABLE_GTK2
    NS_ASSERTION (GDK_IS_WINDOW(aWidget), "unsupported native widget type!");

    if (!UseGlitz()) {
        mSurface = new gfxXlibSurface(GDK_WINDOW_XDISPLAY(GDK_DRAWABLE(aWidget)),
                                      GDK_WINDOW_XWINDOW(GDK_DRAWABLE(aWidget)),
                                      GDK_VISUAL_XVISUAL(gdk_drawable_get_visual(GDK_DRAWABLE(aWidget))));
    } else {
# if defined(MOZ_ENABLE_GLITZ)
        glitz_surface_t *gsurf;
        glitz_drawable_t *gdraw;

        glitz_drawable_format_t *gdformat = (glitz_drawable_format_t*) aDC->GetGlitzDrawableFormat();

        Display* dpy = GDK_WINDOW_XDISPLAY(GDK_DRAWABLE(aWidget));
        Window wnd = GDK_WINDOW_XWINDOW(GDK_DRAWABLE(aWidget));

        Window root_ignore;
        int x_ignore, y_ignore;
        unsigned int bwidth_ignore, width, height, depth;

        XGetGeometry(dpy,
                     wnd,
                     &root_ignore, &x_ignore, &y_ignore,
                     &width, &height,
                     &bwidth_ignore, &depth);

        gdraw =
            glitz_glx_create_drawable_for_window (dpy,
                                                  DefaultScreen(dpy),
                                                  gdformat,
                                                  wnd,
                                                  width,
                                                  height);
        glitz_format_t *gformat =
            glitz_find_standard_format (gdraw, GLITZ_STANDARD_ARGB32);
        gsurf =
            glitz_surface_create (gdraw,
                                  gformat,
                                  width,
                                  height,
                                  0,
                                  NULL);
        glitz_surface_attach (gsurf, gdraw, GLITZ_DRAWABLE_BUFFER_FRONT_COLOR);


        //fprintf (stderr, "## nsThebesDrawingSurface::Init Glitz DRAWABLE %p (DC: %p)\n", aWidget, aDC);
        mSurface = new gfxGlitzSurface (gdraw, gsurf, PR_TRUE);

        mWidth = width;
        mHeight = height;
# endif
    }
#elif XP_WIN
    HDC nativeDC = (HDC)aWidget;
    mSurface = new gfxWindowsSurface(nativeDC);
#else
#error write me
#endif    

    return NS_OK;
}

#ifdef MOZ_ENABLE_GTK2
static nsresult ConvertPixmapsGTK(GdkPixmap* aPmBlack, GdkPixmap* aPmWhite,
                                  const nsIntSize& aSize, PRUint8* aData);
#endif

nsresult
nsThebesDrawingSurface::Init (nsThebesDeviceContext *aDC,
                              void* aNativePixmapBlack,
                              void* aNativePixmapWhite,
                              const nsIntSize& aSrcSize)
{
    PR_LOG(gThebesGFXLog, PR_LOG_DEBUG, ("## %p nsThebesDrawingSurface::Init aDC %p nativeBlack %p nativeWhite %p size [%d,%d]\n", this, aDC, aNativePixmapBlack, aNativePixmapWhite, aSrcSize.width, aSrcSize.height));

    mWidth = aSrcSize.width;
    mHeight = aSrcSize.height;

#ifdef MOZ_ENABLE_GTK2
    nsresult rv;
    nsRefPtr<gfxImageSurface> imgSurf = new gfxImageSurface(gfxImageSurface::ImageFormatARGB32, mWidth, mHeight);

    GdkPixmap* pmBlack = NS_STATIC_CAST(GdkPixmap*, aNativePixmapBlack);
    GdkPixmap* pmWhite = NS_STATIC_CAST(GdkPixmap*, aNativePixmapWhite);

    rv = ConvertPixmapsGTK(pmBlack, pmWhite, aSrcSize, imgSurf->Data());
    if (NS_FAILED(rv))
        return rv;

    mSurface = imgSurf;
#elif XP_WIN
    // XXX writeme
#else
#error Write me!
#endif

    return NS_OK;
}

nsresult
nsThebesDrawingSurface::PushFilter(const nsIntRect& aRect, PRBool aAreaIsOpaque, float aOpacity)
{
    return NS_OK;
}

void
nsThebesDrawingSurface::PopFilter()
{
    
}

NS_IMETHODIMP
nsThebesDrawingSurface::Lock (PRInt32 aX, PRInt32 aY, PRUint32 aWidth, PRUint32 aHeight,
                             void **aBits, PRInt32 *aStride, PRInt32 *aWidthBytes,
                             PRUint32 aFlags)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsThebesDrawingSurface::Unlock (void)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsThebesDrawingSurface::GetDimensions (PRUint32 *aWidth, PRUint32 *aHeight)
{
    *aWidth = mWidth;
    *aHeight = mHeight;
    return NS_OK;
}

NS_IMETHODIMP
nsThebesDrawingSurface::IsOffscreen(PRBool *aOffScreen)
{
    /* remove this method */
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsThebesDrawingSurface::IsPixelAddressable(PRBool *aAddressable)
{
    /* remove this method */
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsThebesDrawingSurface::GetPixelFormat(nsPixelFormat *aFormat)
{
    /* remove this method, and make canvas and DumpToPPM stop using it */
    return NS_ERROR_NOT_IMPLEMENTED;
}

/***** Platform specific helpers ****/

/**
 * Builds an ARGB word from channel values. r,g,b must already
 * be premultipled/rendered onto black.
 */
static PRInt32 BuildARGB(PRUint8 aR, PRUint8 aG, PRUint8 aB, PRUint8 aA)
{
    return (aA << 24) | (aR << 16) | (aG << 8) | aB;
}

#ifdef MOZ_ENABLE_GTK2
static nsresult ConvertPixmapsGTK(GdkPixmap* aPmBlack, GdkPixmap* aPmWhite,
                                  const nsIntSize& aSize, PRUint8* aData)
{
    GdkImage *imgBlack = gdk_image_get(aPmBlack, 0, 0,
                                       aSize.width,
                                       aSize.height);
    GdkImage *imgWhite = gdk_image_get(aPmWhite, 0, 0,
                                       aSize.width,
                                       aSize.height);

    if (!imgBlack || !imgWhite)
        return NS_ERROR_OUT_OF_MEMORY;

    PRUint8* blackData = (PRUint8*)GDK_IMAGE_XIMAGE(imgBlack)->data;
    PRUint8* whiteData = (PRUint8*)GDK_IMAGE_XIMAGE(imgWhite)->data;
    PRInt32 bpp = GDK_IMAGE_XIMAGE(imgBlack)->bits_per_pixel;
    PRInt32 stride = GDK_IMAGE_XIMAGE(imgBlack)->bytes_per_line;
    PRInt32* argb = (PRInt32*)aData;

    if (bpp == 24 || bpp == 32) {
        PRInt32 pixSize = bpp/8;
        for (PRInt32 y = 0; y < aSize.height; ++y) {
            PRUint8* blackRow = blackData + y*stride;
            PRUint8* whiteRow = whiteData + y*stride;
            for (PRInt32 x = 0; x < aSize.width; ++x) {
                PRUint8 alpha = 0;
                if (blackRow[0] == whiteRow[0] &&
                    blackRow[1] == whiteRow[1] &&
                    blackRow[2] == whiteRow[2]) {
                    alpha = 0xFF;
                }
                *argb++ = BuildARGB(blackRow[2], blackRow[1],
                                    blackRow[0], alpha);
                blackRow += pixSize;
                whiteRow += pixSize;
            }
        }
    } else {
        NS_ERROR("Unhandled bpp!");
    }

    gdk_image_unref(imgBlack);
    gdk_image_unref(imgWhite);

    return NS_OK;
}
#endif

PRBool
nsThebesDrawingSurface::UseGlitz()
{
#ifdef MOZ_ENABLE_GLITZ
    if (mGlitzMode == -1) {
        if (getenv("MOZ_GLITZ")) {
            glitz_glx_init (NULL);
            mGlitzMode = 1;
        } else {
            mGlitzMode = 0;
        }
    }

    if (mGlitzMode)
        return PR_TRUE;

#endif
    return PR_FALSE;
}
