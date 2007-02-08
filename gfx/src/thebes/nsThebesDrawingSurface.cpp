/* -*- Mode: C++; tab-width: 50; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is thebes gfx.
 *
 * The Initial Developer of the Original Code is
 * mozilla.org.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsThebesDrawingSurface.h"
#include "nsThebesDeviceContext.h"

#include "nsMemory.h"

#include "gfxPlatform.h"

#include "gfxImageSurface.h"

#ifdef MOZ_ENABLE_GTK2
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include "gfxXlibSurface.h"
#endif

#include <stdlib.h>

NS_IMPL_ISUPPORTS1(nsThebesDrawingSurface, nsIDrawingSurface)

nsThebesDrawingSurface::nsThebesDrawingSurface()
{
}

nsThebesDrawingSurface::~nsThebesDrawingSurface()
{
    // destroy this before any other bits are destroyed,
    // in case they depend on them
    mSurface = nsnull;
}

nsresult
nsThebesDrawingSurface::Init(nsThebesDeviceContext *aDC, gfxASurface *aSurface)
{
    mDC = aDC;
    mSurface = aSurface;

    // don't know
    mWidth = 0;
    mHeight = 0;

    return NS_OK;
}

nsresult
nsThebesDrawingSurface::Init(nsThebesDeviceContext *aDC, PRUint32 aWidth, PRUint32 aHeight, PRBool aFastAccess)
{
    NS_ASSERTION(mSurface == nsnull, "Surface already initialized!");
    NS_ASSERTION(aWidth > 0 && aHeight > 0, "Invalid surface dimensions!");

    PR_LOG(gThebesGFXLog, PR_LOG_DEBUG, ("## %p nsThebesDrawingSurface::Init aDC %p w %d h %d fast %d\n", this, aDC, aWidth, aHeight, aFastAccess));

    mWidth = aWidth;
    mHeight = aHeight;
    mDC = aDC;

    // XXX Performance Problem
    // because we don't pass aDC->GetWidget() (a HWND on Windows) to this function,
    // we get DIBs instead of DDBs.
    mSurface = gfxPlatform::GetPlatform()->CreateOffscreenSurface(gfxIntSize(aWidth, aHeight), gfxImageSurface::ImageFormatARGB32);

    return NS_OK;
}

nsresult
nsThebesDrawingSurface::Init (nsThebesDeviceContext *aDC, nsIWidget *aWidget)
{
    NS_ERROR("Should never be called.");
    return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsThebesDrawingSurface::Init (nsThebesDeviceContext *aDC, nsNativeWidget aWidget)
{
    NS_ERROR("Should never be called.");
    return NS_ERROR_NOT_IMPLEMENTED;
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
#else
    NS_ERROR ("nsThebesDrawingSurface init from black/white pixmaps; either implement this or fix the caller");
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
    if (mWidth == 0 && mHeight == 0)
        NS_ERROR("nsThebesDrawingSurface::GetDimensions on a surface for which we don't know width/height!");
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
