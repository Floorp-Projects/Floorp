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
 * The Original Code is Mozilla Foundation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2005
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

#define PANGO_ENABLE_BACKEND
#define PANGO_ENABLE_ENGINE

#include "gfxPlatformGtk.h"

#include "gfxFontconfigUtils.h"
#include "gfxPangoFonts.h"

#include "cairo.h"
#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include "gfxImageSurface.h"
#include "gfxXlibSurface.h"

#include "gfxPangoFonts.h"

#include <pango/pangocairo.h>

#ifdef MOZ_ENABLE_GLITZ
#include "gfxGlitzSurface.h"
#include "glitz-glx.h"
#endif

#include <fontconfig/fontconfig.h>

#include "nsMathUtils.h"

#include "lcms.h"

PRInt32 gfxPlatformGtk::sDPI = -1;
gfxFontconfigUtils *gfxPlatformGtk::sFontconfigUtils = nsnull;

gfxPlatformGtk::gfxPlatformGtk()
{
#ifdef MOZ_ENABLE_GLITZ
    if (UseGlitz())
        glitz_glx_init(NULL);
#endif
    if (!sFontconfigUtils)
        sFontconfigUtils = gfxFontconfigUtils::GetFontconfigUtils();

    InitDPI();
}

gfxPlatformGtk::~gfxPlatformGtk()
{
    gfxFontconfigUtils::Shutdown();
    sFontconfigUtils = nsnull;

    gfxPangoFont::Shutdown();

#if 0
    // It would be nice to do this (although it might need to be after
    // the cairo shutdown that happens in ~gfxPlatform).  It even looks
    // idempotent.  But it has fatal assertions that fire if stuff is
    // leaked, and we hit them.
    FcFini();
#endif
}

already_AddRefed<gfxASurface>
gfxPlatformGtk::CreateOffscreenSurface(const gfxIntSize& size,
                                       gfxASurface::gfxImageFormat imageFormat)
{
    gfxASurface *newSurface = nsnull;

    int glitzf;
    int xrenderFormatID;
    switch (imageFormat) {
        case gfxASurface::ImageFormatARGB32:
            glitzf = 0; // GLITZ_STANDARD_ARGB32;
            xrenderFormatID = PictStandardARGB32;
            break;
        case gfxASurface::ImageFormatRGB24:
            glitzf = 1; // GLITZ_STANDARD_RGB24;
            xrenderFormatID = PictStandardRGB24;
            break;
        case gfxASurface::ImageFormatA8:
            glitzf = 2; // GLITZ_STANDARD_A8;
            xrenderFormatID = PictStandardA8;
            break;
        case gfxASurface::ImageFormatA1:
            glitzf = 3; // GLITZ_STANDARD_A1;
            xrenderFormatID = PictStandardA1;
            break;
        default:
            return nsnull;
    }

    // XXX we really need a different interface here, something that passes
    // in more context, including the display and/or target surface type that
    // we should try to match
    Display* display = GDK_DISPLAY();
    if (!UseGlitz()) {
        GdkPixmap* pixmap = nsnull;
        XRenderPictFormat* xrenderFormat =
            XRenderFindStandardFormat(display, xrenderFormatID);
        if (!xrenderFormat) {
            // We don't have Render; see if we can just create a pixmap
            // of the requested depth.  Otherwise, create an Image surface.
            GdkVisual* vis;

            if (imageFormat == gfxASurface::ImageFormatRGB24) {
                vis = gdk_rgb_get_visual();
                if (vis->type == GDK_VISUAL_TRUE_COLOR)
                    pixmap = gdk_pixmap_new(nsnull, size.width, size.height, vis->depth);
            }

            if (pixmap) {
                gdk_drawable_set_colormap(GDK_DRAWABLE(pixmap), nsnull);
                newSurface = new gfxXlibSurface(display,
                                                GDK_PIXMAP_XID(GDK_DRAWABLE(pixmap)),
                                                GDK_VISUAL_XVISUAL(vis),
                                                size);
            } else {
                // we couldn't create a Gdk Pixmap; fall back to image surface for the data
                newSurface = new gfxImageSurface(gfxIntSize(size.width, size.height), imageFormat);
            }
        } else {
            pixmap = gdk_pixmap_new(nsnull, size.width, size.height,
                                    xrenderFormat->depth);
            gdk_drawable_set_colormap(GDK_DRAWABLE(pixmap), nsnull);

            newSurface = new gfxXlibSurface(display,
                                            GDK_PIXMAP_XID(GDK_DRAWABLE(pixmap)),
                                            xrenderFormat,
                                            size);
        }

    } else {
#ifdef MOZ_ENABLE_GLITZ
        glitz_drawable_format_t *gdformat = glitz_glx_find_pbuffer_format
            (display,
             gdk_x11_get_default_screen(),
             0, NULL, 0);

        glitz_drawable_t *gdraw =
            glitz_glx_create_pbuffer_drawable(display,
                                              DefaultScreen(display),
                                              gdformat,
                                              size.width,
                                              size.height);
        glitz_format_t *gformat =
            glitz_find_standard_format(gdraw, (glitz_format_name_t)glitzf);

        glitz_surface_t *gsurf =
            glitz_surface_create(gdraw,
                                 gformat,
                                 size.width,
                                 size.height,
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
gfxPlatformGtk::GetFontList(const nsACString& aLangGroup,
                            const nsACString& aGenericFamily,
                            nsStringArray& aListOfFonts)
{
    return sFontconfigUtils->GetFontList(aLangGroup, aGenericFamily,
                                         aListOfFonts);
}

nsresult
gfxPlatformGtk::UpdateFontList()
{
    return sFontconfigUtils->UpdateFontList();
}

nsresult
gfxPlatformGtk::ResolveFontName(const nsAString& aFontName,
                                FontResolverCallback aCallback,
                                void *aClosure,
                                PRBool& aAborted)
{
    return sFontconfigUtils->ResolveFontName(aFontName, aCallback,
                                             aClosure, aAborted);
}

gfxFontGroup *
gfxPlatformGtk::CreateFontGroup(const nsAString &aFamilies,
                                const gfxFontStyle *aStyle)
{
    return new gfxPangoFontGroup(aFamilies, aStyle);
}

/* static */
void
gfxPlatformGtk::InitDPI()
{
    PangoContext *context = gdk_pango_context_get ();
    sDPI = pango_cairo_context_get_resolution (context);
    g_object_unref (context);

    if (sDPI <= 0) {
	// Fall back to something sane
	sDPI = 96;
    }
}

cmsHPROFILE
gfxPlatformGtk::GetPlatformCMSOutputProfile()
{
    const char EDID1_ATOM_NAME[] = "XFree86_DDC_EDID1_RAWDATA";
    const char ICC_PROFILE_ATOM_NAME[] = "_ICC_PROFILE";

    Atom edidAtom, iccAtom;
    Display *dpy = GDK_DISPLAY();
    Window root = gdk_x11_get_default_root_xwindow();

    Atom retAtom;
    int retFormat;
    unsigned long retLength, retAfter;
    unsigned char *retProperty ;

    iccAtom = XInternAtom(dpy, ICC_PROFILE_ATOM_NAME, TRUE);
    if (iccAtom) {
        // read once to get size, once for the data
        if (Success == XGetWindowProperty(dpy, root, iccAtom,
                                          0, 0 /* length */,
                                          False, AnyPropertyType,
                                          &retAtom, &retFormat, &retLength,
                                          &retAfter, &retProperty)) {
            XGetWindowProperty(dpy, root, iccAtom,
                               0, retLength,
                               False, AnyPropertyType,
                               &retAtom, &retFormat, &retLength,
                               &retAfter, &retProperty);

            cmsHPROFILE profile =
                cmsOpenProfileFromMem(retProperty, retLength);

            XFree(retProperty);

            if (profile) {
#ifdef DEBUG_tor
                fprintf(stderr,
                        "ICM profile read from %s successfully\n",
                        ICC_PROFILE_ATOM_NAME);
#endif
                return profile;
            }
        }
    }

    edidAtom = XInternAtom(dpy, EDID1_ATOM_NAME, TRUE);
    if (edidAtom) {
        if (Success == XGetWindowProperty(dpy, root, edidAtom, 0, 32,
                                          False, AnyPropertyType,
                                          &retAtom, &retFormat, &retLength,
                                          &retAfter, &retProperty)) {
            double gamma;
            cmsCIExyY whitePoint;
            cmsCIExyYTRIPLE primaries;

            if (retLength != 128) {
#ifdef DEBUG_tor
                fprintf(stderr, "Short EDID data\n");
#endif
                return nsnull;
            }

            // Format documented in "VESA E-EDID Implementation Guide"

            gamma = (100 + retProperty[0x17]) / 100.0;
            whitePoint.x = ((retProperty[0x21] << 2) |
                            (retProperty[0x1a] >> 2 & 3)) / 1024.0;
            whitePoint.y = ((retProperty[0x22] << 2) |
                            (retProperty[0x1a] >> 0 & 3)) / 1024.0;
            whitePoint.Y = 1.0;

            primaries.Red.x = ((retProperty[0x1b] << 2) |
                               (retProperty[0x19] >> 6 & 3)) / 1024.0;
            primaries.Red.y = ((retProperty[0x1c] << 2) |
                               (retProperty[0x19] >> 4 & 3)) / 1024.0;
            primaries.Red.Y = 1.0;

            primaries.Green.x = ((retProperty[0x1d] << 2) |
                                 (retProperty[0x19] >> 2 & 3)) / 1024.0;
            primaries.Green.y = ((retProperty[0x1e] << 2) |
                                 (retProperty[0x19] >> 0 & 3)) / 1024.0;
            primaries.Green.Y = 1.0;

            primaries.Blue.x = ((retProperty[0x1f] << 2) |
                               (retProperty[0x1a] >> 6 & 3)) / 1024.0;
            primaries.Blue.y = ((retProperty[0x20] << 2) |
                               (retProperty[0x1a] >> 4 & 3)) / 1024.0;
            primaries.Blue.Y = 1.0;

            XFree(retProperty);

#ifdef DEBUG_tor
            fprintf(stderr, "EDID gamma: %f\n", gamma);
            fprintf(stderr, "EDID whitepoint: %f %f %f\n",
                    whitePoint.x, whitePoint.y, whitePoint.Y);
            fprintf(stderr, "EDID primaries: [%f %f %f] [%f %f %f] [%f %f %f]\n",
                    primaries.Red.x, primaries.Red.y, primaries.Red.Y,
                    primaries.Green.x, primaries.Green.y, primaries.Green.Y,
                    primaries.Blue.x, primaries.Blue.y, primaries.Blue.Y);
#endif

            LPGAMMATABLE gammaTable[3];
            gammaTable[0] = gammaTable[1] = gammaTable[2] =
                cmsBuildGamma(256, gamma);

            if (!gammaTable[0])
                return nsnull;

            cmsHPROFILE profile =
                cmsCreateRGBProfile(&whitePoint, &primaries, gammaTable);

            cmsFreeGamma(gammaTable[0]);

#ifdef DEBUG_tor
            if (profile) {
                fprintf(stderr,
                        "ICM profile read from %s successfully\n",
                        EDID1_ATOM_NAME);
            }
#endif

            return profile;
        }
    }
    return nsnull;
}
