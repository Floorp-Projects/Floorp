/* vim: set sw=4 sts=4 et cin: */
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
 * The Original Code is OS/2 code in Thebes.
 *
 * The Initial Developer of the Original Code is
 * Peter Weilbacher <mozilla@Weilbacher.org>.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "gfxOS2Platform.h"
#include "gfxOS2Surface.h"
#include "gfxImageSurface.h"

/**********************************************************************
 * class gfxOS2Platform
 **********************************************************************/
gfxOS2Platform::gfxOS2Platform()
    : mDC(NULL), mPS(NULL), mBitmap(NULL)
{
#ifdef DEBUG_thebes
    printf("gfxOS2Platform::gfxOS2Platform()\n");
#endif
    // this seems to be reasonably early in the process and only once,
    // so it's a good place to initialize OS/2 cairo stuff
    cairo_os2_init();
#ifdef DEBUG_thebes
    printf("  cairo_os2_init() was called\n");
#endif
}

gfxOS2Platform::~gfxOS2Platform()
{
#ifdef DEBUG_thebes
    printf("gfxOS2Platform::~gfxOS2Platform()\n");
#endif

    if (mBitmap) {
        GpiSetBitmap(mPS, NULL);
        GpiDeleteBitmap(mBitmap);
    }
    if (mPS) {
        GpiDestroyPS(mPS);
    }
    if (mDC) {
        DevCloseDC(mDC);
    }
    // clean up OS/2 cairo stuff
    cairo_os2_fini();
#ifdef DEBUG_thebes
    printf("  cairo_os2_fini() was called\n");
#endif
}

already_AddRefed<gfxASurface>
gfxOS2Platform::CreateOffscreenSurface(const gfxIntSize& aSize,
                                       gfxASurface::gfxImageFormat aImageFormat)
{
#ifdef DEBUG_thebes
    printf("gfxOS2Platform::CreateOffscreenSurface(%d/%d, %d)\n",
           aSize.width, aSize.height, aImageFormat);
#endif
    gfxASurface *newSurface = nsnull;

    // XXX we only ever seem to get aImageFormat=0 or ImageFormatARGB32 but
    // I don't really know if we need to differ between ARGB32 and RGB24 here
    if (aImageFormat == gfxASurface::ImageFormatARGB32 ||
        aImageFormat == gfxASurface::ImageFormatRGB24)
    {
        // create a PS, partly taken from nsOffscreenSurface::Init(), i.e. nsDrawingSurfaceOS2.cpp
        DEVOPENSTRUC dop = { 0, 0, 0, 0, 0 };
        SIZEL sizel = { 0, 0 }; /* use same page size as device */
        mDC = DevOpenDC(0, OD_MEMORY, "*", 5, (PDEVOPENDATA)&dop, NULLHANDLE);
        if (mDC != DEV_ERROR) {
            mPS = GpiCreatePS(0, mDC, &sizel, PU_PELS | GPIT_MICRO | GPIA_ASSOC);
            if (mPS != GPI_ERROR) {
                // XXX: nsPaletteOS2::SelectGlobalPalette(mPS);
                // perhaps implement the palette stuff at some point?!

                // now create a bitmap of the right size
                BITMAPINFOHEADER2 hdr = { 0 };
                hdr.cbFix = sizeof(BITMAPINFOHEADER2);
                hdr.cx = aSize.width;
                hdr.cy = aSize.height;
                hdr.cPlanes = 1;

                // find bit depth, XXX this may not work here, use the aImageFormat instead?!
                LONG lBitCount = 0;
                DevQueryCaps(mDC, CAPS_COLOR_BITCOUNT, 1, &lBitCount);
                hdr.cBitCount = (USHORT)lBitCount;

                mBitmap = GpiCreateBitmap(mPS, &hdr, 0, 0, 0);
                if (mBitmap != GPI_ERROR) {
                    // set final stats & select bitmap into ps
                    GpiSetBitmap(mPS, mBitmap);
                }
            } /* if mPS */
        } /* if mDC */
        newSurface = new gfxOS2Surface(mPS, aSize);
    } else if (aImageFormat == gfxASurface::ImageFormatA8 ||
               aImageFormat == gfxASurface::ImageFormatA1) {
        newSurface = new gfxImageSurface(aSize, aImageFormat);
    } else {
        return nsnull;
    }

    NS_IF_ADDREF(newSurface);
    return newSurface;
}

nsresult
gfxOS2Platform::GetFontList(const nsACString& aLangGroup,
                            const nsACString& aGenericFamily,
                            nsStringArray& aListOfFonts)
{
#ifdef DEBUG_thebes
    char *langgroup = ToNewCString(aLangGroup),
         *family = ToNewCString(aGenericFamily);
    printf("gfxOS2Platform::GetFontList(%s, %s, ..)\n",
           langgroup, family);
#endif
    return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
gfxOS2Platform::ResolveFontName(const nsAString& aFontName,
                                FontResolverCallback aCallback,
                                void *aClosure, PRBool& aAborted)
{
#ifdef DEBUG_thebes
    char *fontname = ToNewCString(aFontName);
    printf("gfxOS2Platform::ResolveFontName(%s, ...)\n", fontname);
#endif
    aAborted = !(*aCallback)(aFontName, aClosure);
    return NS_OK;
}
