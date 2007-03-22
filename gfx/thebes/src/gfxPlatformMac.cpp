/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is thebes gfx code.
 *
 * The Initial Developer of the Original Code is Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com>
 *   Masayuki Nakano <masayuki@d-toybox.com>
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

#include "gfxPlatformMac.h"

#include "gfxImageSurface.h"
#include "gfxQuartzSurface.h"

#include "gfxQuartzFontCache.h"

#ifdef MOZ_ENABLE_GLITZ
#include "gfxGlitzSurface.h"
#include "glitz-agl.h"
#endif

gfxPlatformMac::gfxPlatformMac()
{
#ifdef MOZ_ENABLE_GLITZ
    if (UseGlitz())
        glitz_agl_init();
#endif
}

already_AddRefed<gfxASurface>
gfxPlatformMac::CreateOffscreenSurface(const gfxIntSize& size,
                                       gfxASurface::gfxImageFormat imageFormat)
{
    gfxASurface *newSurface = nsnull;

    if (!UseGlitz()) {
        newSurface = new gfxQuartzSurface(size, imageFormat);
    } else {
#ifdef MOZ_ENABLE_GLITZ
        int bpp, glitzf;
        switch (imageFormat) {
            case gfxASurface::ImageFormatARGB32:
                bpp = 32;
                glitzf = 0; // GLITZ_STANDARD_ARGB32;
                break;
            case gfxASurface::ImageFormatRGB24:
                bpp = 24;
                glitzf = 1; // GLITZ_STANDARD_RGB24;
                break;
            case gfxASurface::ImageFormatA8:
                bpp = 8;
                glitzf = 2; // GLITZ_STANDARD_A8;
            case gfxASurface::ImageFormatA1:
                bpp = 1;
                glitzf = 3; // GLITZ_STANDARD_A1;
                break;
            default:
                return nsnull;
        }

        // XXX look for the right kind of format based on bpp
        glitz_drawable_format_t templ;
        memset(&templ, 0, sizeof(templ));
        templ.color.red_size = 8;
        templ.color.green_size = 8;
        templ.color.blue_size = 8;
        if (bpp == 32)
            templ.color.alpha_size = 8;
        else
            templ.color.alpha_size = 0;
        templ.doublebuffer = FALSE;
        templ.samples = 1;

        unsigned long mask =
            GLITZ_FORMAT_RED_SIZE_MASK |
            GLITZ_FORMAT_GREEN_SIZE_MASK |
            GLITZ_FORMAT_BLUE_SIZE_MASK |
            GLITZ_FORMAT_ALPHA_SIZE_MASK |
            GLITZ_FORMAT_SAMPLES_MASK |
            GLITZ_FORMAT_DOUBLEBUFFER_MASK;

        glitz_drawable_format_t *gdformat =
            glitz_agl_find_pbuffer_format(mask, &templ, 0);

        glitz_drawable_t *gdraw =
            glitz_agl_create_pbuffer_drawable(gdformat, width, height);

        glitz_format_t *gformat =
            glitz_find_standard_format(gdraw, (glitz_format_name_t) glitzf);

        glitz_surface_t *gsurf =
            glitz_surface_create(gdraw,
                                 gformat,
                                 width,
                                 height,
                                 0,
                                 NULL);

        glitz_surface_attach(gsurf, gdraw, GLITZ_DRAWABLE_BUFFER_FRONT_COLOR);

        newSurface = new gfxGlitzSurface(gdraw, gsurf, PR_TRUE);
#endif
    }

    NS_IF_ADDREF(newSurface);
    return newSurface;
}

nsresult
gfxPlatformMac::ResolveFontName(const nsAString& aFontName,
                                FontResolverCallback aCallback,
                                void *aClosure, PRBool& aAborted)
{
    nsAutoString resolvedName;
    if (!gfxQuartzFontCache::SharedFontCache()->
             ResolveFontName(aFontName, resolvedName)) {
        aAborted = PR_FALSE;
        return NS_OK;
    }
    aAborted = !(*aCallback)(resolvedName, aClosure);
    return NS_OK;
}

nsresult
gfxPlatformMac::GetFontList(const nsACString& aLangGroup,
                            const nsACString& aGenericFamily,
                            nsStringArray& aListOfFonts)
{
    gfxQuartzFontCache::SharedFontCache()->
        GetFontList(aLangGroup, aGenericFamily, aListOfFonts);
    return NS_OK;
}

nsresult
gfxPlatformMac::UpdateFontList()
{
    gfxQuartzFontCache::SharedFontCache()->UpdateFontList();
    return NS_OK;
}
