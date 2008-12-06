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
 *   Karl Tomlinson <karlt+@karlt.net>, Mozilla Corporation
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

#ifdef MOZ_PANGO
#define PANGO_ENABLE_BACKEND
#define PANGO_ENABLE_ENGINE
#endif

#include "gfxPlatformGtk.h"

#include "gfxFontconfigUtils.h"
#ifdef MOZ_PANGO
#include "gfxPangoFonts.h"
#include "gfxContext.h"
#include "gfxUserFontSet.h"
#else
#include <ft2build.h>
#include FT_FREETYPE_H
#include "gfxFT2Fonts.h"
#endif

#include "cairo.h"
#include <gtk/gtk.h>

#include "gfxImageSurface.h"
#ifdef MOZ_X11
#include <gdk/gdkx.h>
#include "gfxXlibSurface.h"
#include "cairo-xlib.h"
#endif /* MOZ_X11 */

#ifdef MOZ_DFB
#include "gfxDirectFBSurface.h"
#endif

#ifdef MOZ_DFB
#include "gfxDirectFBSurface.h"
#endif

#include <fontconfig/fontconfig.h>

#include "nsMathUtils.h"

#include "lcms.h"

#define GDK_PIXMAP_SIZE_MAX 32767

#ifndef MOZ_PANGO
#include <ft2build.h>
#include FT_FREETYPE_H
#endif

double gfxPlatformGtk::sDPI = -1.0;
gfxFontconfigUtils *gfxPlatformGtk::sFontconfigUtils = nsnull;

#ifndef MOZ_PANGO
typedef nsDataHashtable<nsStringHashKey, nsRefPtr<FontFamily> > FontTable;
static FontTable *gPlatformFonts = NULL;
static FontTable *gPlatformFontAliases = NULL;
static FT_Library gPlatformFTLibrary = NULL;
#endif

static cairo_user_data_key_t cairo_gdk_drawable_key;
static void do_gdk_drawable_unref (void *data)
{
    GdkDrawable *d = (GdkDrawable*) data;
    g_object_unref (d);
}

gfxPlatformGtk::gfxPlatformGtk()
{
    if (!sFontconfigUtils)
        sFontconfigUtils = gfxFontconfigUtils::GetFontconfigUtils();

#ifndef MOZ_PANGO
    FT_Init_FreeType(&gPlatformFTLibrary);

    gPlatformFonts = new FontTable();
    gPlatformFonts->Init(100);
    gPlatformFontAliases = new FontTable();
    gPlatformFontAliases->Init(100);
    UpdateFontList();
#endif

    InitDPI();
}

gfxPlatformGtk::~gfxPlatformGtk()
{
    gfxFontconfigUtils::Shutdown();
    sFontconfigUtils = nsnull;

#ifdef MOZ_PANGO
    gfxPangoFontGroup::Shutdown();
#else
    delete gPlatformFonts;
    gPlatformFonts = NULL;
    delete gPlatformFontAliases;
    gPlatformFontAliases = NULL;

    FT_Done_FreeType(gPlatformFTLibrary);
    gPlatformFTLibrary = NULL;
#endif

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
    nsRefPtr<gfxASurface> newSurface = nsnull;
    PRBool sizeOk = PR_TRUE;

    if (size.width >= GDK_PIXMAP_SIZE_MAX ||
        size.height >= GDK_PIXMAP_SIZE_MAX)
        sizeOk = PR_FALSE;

#ifdef MOZ_X11
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
    if (!display)
        return nsnull;

    GdkPixmap* pixmap = nsnull;
    XRenderPictFormat* xrenderFormat =
        XRenderFindStandardFormat(display, xrenderFormatID);

    if (xrenderFormat && sizeOk) {
        pixmap = gdk_pixmap_new(nsnull, size.width, size.height,
                                xrenderFormat->depth);

        if (pixmap) {
            gdk_drawable_set_colormap(GDK_DRAWABLE(pixmap), nsnull);
            newSurface = new gfxXlibSurface(display,
                                            GDK_PIXMAP_XID(GDK_DRAWABLE(pixmap)),
                                            xrenderFormat,
                                            size);
        }

        if (newSurface && newSurface->CairoStatus() == 0) {
            // set up the surface to auto-unref the gdk pixmap when
            // the surface is released
            SetGdkDrawable(newSurface, GDK_DRAWABLE(pixmap));
        } else {
            // something went wrong with the surface creation.
            // Ignore and let's fall back to image surfaces.
            newSurface = nsnull;
        }

        // always unref; SetGdkDrawable takes its own ref
        if (pixmap)
            g_object_unref(pixmap);
    }
#endif

#ifdef MOZ_DFB
    if (sizeOk)
        newSurface = new gfxDirectFBSurface(size, imageFormat);
#endif


    if (!newSurface) {
        // We couldn't create a native surface for whatever reason;
        // e.g., no RENDER, bad size, etc.
        // Fall back to image surface for the data.
        newSurface = new gfxImageSurface(gfxIntSize(size.width, size.height), imageFormat);
    }

    if (newSurface) {
        gfxContext tmpCtx(newSurface);
        tmpCtx.SetOperator(gfxContext::OPERATOR_CLEAR);
        tmpCtx.Paint();
    }

    return newSurface.forget();
}

#ifdef MOZ_PANGO

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

nsresult
gfxPlatformGtk::GetStandardFamilyName(const nsAString& aFontName, nsAString& aFamilyName)
{
    return sFontconfigUtils->GetStandardFamilyName(aFontName, aFamilyName);
}

gfxFontGroup *
gfxPlatformGtk::CreateFontGroup(const nsAString &aFamilies,
                                const gfxFontStyle *aStyle,
                                gfxUserFontSet *aUserFontSet)
{
    return new gfxPangoFontGroup(aFamilies, aStyle, aUserFontSet);
}

gfxFontEntry* 
gfxPlatformGtk::MakePlatformFont(const gfxProxyFontEntry *aProxyEntry, 
                                 nsISupports *aLoader,
                                 const PRUint8 *aFontData, PRUint32 aLength)
{
    // Just being consistent with other platforms.
    // This will mean that only fonts in SFNT formats will be accepted.
    if (!gfxFontUtils::ValidateSFNTHeaders(aFontData, aLength))
        return nsnull;

    return gfxPangoFontGroup::NewFontEntry(*aProxyEntry, aLoader,
                                           aFontData, aLength);
}

PRBool
gfxPlatformGtk::IsFontFormatSupported(nsIURI *aFontURI, PRUint32 aFormatFlags)
{
    // reject based on format flags
    if (aFormatFlags & (gfxUserFontSet::FLAG_FORMAT_EOT | gfxUserFontSet::FLAG_FORMAT_SVG)) {
        return PR_FALSE;
    }

    // Pango doesn't apply features from AAT TrueType extensions.
    // Assume that if this is the only SFNT format specified,
    // then AAT extensions are required for complex script support.
    if ((aFormatFlags & gfxUserFontSet::FLAG_FORMAT_TRUETYPE_AAT) 
         && !(aFormatFlags & (gfxUserFontSet::FLAG_FORMAT_OPENTYPE | gfxUserFontSet::FLAG_FORMAT_TRUETYPE))) {
        return PR_FALSE;
    }

    // otherwise, return true
    return PR_TRUE;
}

#else

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
    FcPattern *pat = NULL;
    FcObjectSet *os = NULL;
    FcFontSet *fs = NULL;
    PRInt32 result = -1;

    pat = FcPatternCreate();
    os = FcObjectSetBuild(FC_FAMILY, FC_FILE, FC_INDEX, FC_WEIGHT, FC_SLANT, FC_WIDTH, NULL);

    fs = FcFontList(NULL, pat, os);


    for (int i = 0; i < fs->nfont; i++) {
        char *str;

        if (FcPatternGetString(fs->fonts[i], FC_FAMILY, 0, (FcChar8 **) &str) != FcResultMatch)
            continue;

        //printf("Family: %s\n", str);

        nsAutoString name(NS_ConvertUTF8toUTF16(nsDependentCString(str)).get());
        nsAutoString key(name);
        /* FIXME DFB */
        //ToLowerCase(key);
        nsRefPtr<FontFamily> ff;
        if (!gPlatformFonts->Get(key, &ff)) {
            ff = new FontFamily(name);
            gPlatformFonts->Put(key, ff);
        }

        nsRefPtr<FontEntry> fe = new FontEntry(ff->mName);
        ff->mFaces.AppendElement(fe);

        if (FcPatternGetString(fs->fonts[i], FC_FILE, 0, (FcChar8 **) &str) == FcResultMatch) {
            fe->mFilename = nsDependentCString(str);
            //printf(" - file: %s\n", str);
        }

        int x;
        if (FcPatternGetInteger(fs->fonts[i], FC_INDEX, 0, &x) == FcResultMatch) {
            //printf(" - index: %d\n", x);
            fe->mFTFontIndex = x;
        } else {
            fe->mFTFontIndex = 0;
        }

        fe->mWeight = gfxFontconfigUtils::GetThebesWeight(fs->fonts[i]);
        //printf(" - weight: %d\n", fe->mWeight);

        fe->mItalic = PR_FALSE;
        if (FcPatternGetInteger(fs->fonts[i], FC_SLANT, 0, &x) == FcResultMatch) {
            switch (x) {
            case FC_SLANT_ITALIC:
            case FC_SLANT_OBLIQUE:
                fe->mItalic = PR_TRUE;
            }
            //printf(" - slant: %d\n", x);
        }

        //if (FcPatternGetInteger(fs->fonts[i], FC_WIDTH, 0, &x) == FcResultMatch)
            //printf(" - width: %d\n", x);
        // XXX deal with font-stretch stuff later
    }

    if (pat)
        FcPatternDestroy(pat);
    if (os)
        FcObjectSetDestroy(os);
    if (fs)
        FcFontSetDestroy(fs);

    return sFontconfigUtils->UpdateFontList();
}

nsresult
gfxPlatformGtk::ResolveFontName(const nsAString& aFontName,
                                FontResolverCallback aCallback,
                                void *aClosure,
                                PRBool& aAborted)
{

    nsAutoString name(aFontName);
    /* FIXME: DFB */
    //ToLowerCase(name);

    nsRefPtr<FontFamily> ff;
    if (gPlatformFonts->Get(name, &ff) ||
        gPlatformFontAliases->Get(name, &ff)) {
        aAborted = !(*aCallback)(ff->mName, aClosure);
        return NS_OK;
    }

    nsCAutoString utf8Name = NS_ConvertUTF16toUTF8(aFontName);

    FcPattern *npat = FcPatternCreate();
    FcPatternAddString(npat, FC_FAMILY, (FcChar8*)utf8Name.get());
    FcObjectSet *nos = FcObjectSetBuild(FC_FAMILY, NULL);
    FcFontSet *nfs = FcFontList(NULL, npat, nos);

    for (int k = 0; k < nfs->nfont; k++) {
        FcChar8 *str;
        if (FcPatternGetString(nfs->fonts[k], FC_FAMILY, 0, (FcChar8 **) &str) != FcResultMatch)
            continue;
        nsAutoString altName = NS_ConvertUTF8toUTF16(nsDependentCString(reinterpret_cast<char*>(str)));
        /* FIXME: DFB */
        //ToLowerCase(altName);
        if (gPlatformFonts->Get(altName, &ff)) {
            //printf("Adding alias: %s -> %s\n", utf8Name.get(), str);
            gPlatformFontAliases->Put(name, ff);
            aAborted = !(*aCallback)(NS_ConvertUTF8toUTF16(nsDependentCString(reinterpret_cast<char*>(str))), aClosure);
            goto DONE;
        }
    }

    FcPatternDestroy(npat);
    FcObjectSetDestroy(nos);
    FcFontSetDestroy(nfs);

    {
    npat = FcPatternCreate();
    FcPatternAddString(npat, FC_FAMILY, (FcChar8*)utf8Name.get());
    FcPatternDel(npat, FC_LANG);
    FcConfigSubstitute(NULL, npat, FcMatchPattern);
    FcDefaultSubstitute(npat);

    nos = FcObjectSetBuild(FC_FAMILY, NULL);
    nfs = FcFontList(NULL, npat, nos);

    FcResult fresult;

    FcPattern *match = FcFontMatch(NULL, npat, &fresult);
    if (match)
        FcFontSetAdd(nfs, match);

    for (int k = 0; k < nfs->nfont; k++) {
        FcChar8 *str;
        if (FcPatternGetString(nfs->fonts[k], FC_FAMILY, 0, (FcChar8 **) &str) != FcResultMatch)
            continue;
        nsAutoString altName = NS_ConvertUTF8toUTF16(nsDependentCString(reinterpret_cast<char*>(str)));
        /* FIXME: DFB */
        //ToLowerCase(altName);
        if (gPlatformFonts->Get(altName, &ff)) {
            //printf("Adding alias: %s -> %s\n", utf8Name.get(), str);
            gPlatformFontAliases->Put(name, ff);
            aAborted = !(*aCallback)(NS_ConvertUTF8toUTF16(nsDependentCString(reinterpret_cast<char*>(str))), aClosure);
            goto DONE;
        }
    }
    }
 DONE:
    FcPatternDestroy(npat);
    FcObjectSetDestroy(nos);
    FcFontSetDestroy(nfs);

    return NS_OK;
}

nsresult
gfxPlatformGtk::GetStandardFamilyName(const nsAString& aFontName, nsAString& aFamilyName)
{
    return sFontconfigUtils->GetStandardFamilyName(aFontName, aFamilyName);
}

gfxFontGroup *
gfxPlatformGtk::CreateFontGroup(const nsAString &aFamilies,
                               const gfxFontStyle *aStyle)
{
    return new gfxFT2FontGroup(aFamilies, aStyle);
}

#endif


/* static */
void
gfxPlatformGtk::InitDPI()
{
    sDPI = gdk_screen_get_resolution(gdk_screen_get_default());

    if (sDPI <= 0.0) {
        // Fall back to something sane
        sDPI = 96.0;
    }
}

cmsHPROFILE
gfxPlatformGtk::GetPlatformCMSOutputProfile()
{
#ifdef MOZ_X11
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
#endif

    return nsnull;
}


#ifndef MOZ_PANGO
FT_Library
gfxPlatformGtk::GetFTLibrary()
{
    return gPlatformFTLibrary;
}

FontFamily *
gfxPlatformGtk::FindFontFamily(const nsAString& aName)
{
    nsAutoString name(aName);
    /* FIXME: DFB */
    //ToLowerCase(name);

    nsRefPtr<FontFamily> ff;
    if (!gPlatformFonts->Get(name, &ff)) {
        return nsnull;
    }
    return ff.get();
}

FontEntry *
gfxPlatformGtk::FindFontEntry(const nsAString& aName, const gfxFontStyle& aFontStyle)
{
    nsRefPtr<FontFamily> ff = FindFontFamily(aName);
    if (!ff)
        return nsnull;

    return ff->FindFontEntry(aFontStyle);
}
#endif


void
gfxPlatformGtk::SetGdkDrawable(gfxASurface *target,
                               GdkDrawable *drawable)
{
    if (target->CairoStatus())
        return;

    gdk_drawable_ref(drawable);

    cairo_surface_set_user_data (target->CairoSurface(),
                                 &cairo_gdk_drawable_key,
                                 drawable,
                                 do_gdk_drawable_unref);
}

#ifdef MOZ_X11
// Look for an existing Colormap that is known to be associated with visual.
static GdkColormap *
LookupGdkColormapForVisual(const Screen* screen, const Visual* visual)
{
    Display* dpy = DisplayOfScreen(screen);
    GdkDisplay* gdkDpy = gdk_x11_lookup_xdisplay(dpy);
    if (!gdkDpy)
        return NULL;

    // I wish there were a gdk_x11_display_lookup_screen.
    gint screen_num = 0;
    for (int s = 0; s < ScreenCount(dpy); ++s) {
        if (ScreenOfDisplay(dpy, s) == screen) {
            screen_num = s;
            break;
        }
    }
    GdkScreen* gdkScreen = gdk_display_get_screen(gdkDpy, screen_num);

    // Common case: the display's default colormap
    if (visual ==
        GDK_VISUAL_XVISUAL(gdk_screen_get_system_visual(gdkScreen)))
        return gdk_screen_get_system_colormap(gdkScreen);    

    // widget/src/gtk2/mozcontainer.c uses gdk_rgb_get_colormap()
    // which is inherited by child widgets, so this is the visual
    // expected when drawing directly to widget surfaces or surfaces
    // created using cairo_surface_create_similar with
    // CAIRO_CONTENT_COLOR.
    // gdk_screen_get_rgb_colormap is the generalization of
    // gdk_rgb_get_colormap for any screen.
    if (visual ==
        GDK_VISUAL_XVISUAL(gdk_screen_get_rgb_visual(gdkScreen)))
        return gdk_screen_get_rgb_colormap(gdkScreen);

    // This is the visual expected on displays with the Composite
    // extension enabled when the surface has been created using
    // cairo_surface_create_similar with CAIRO_CONTENT_COLOR_ALPHA,
    // as happens with non-unit opacity.
    if (visual ==
        GDK_VISUAL_XVISUAL(gdk_screen_get_rgba_visual(gdkScreen)))
        return gdk_screen_get_rgba_colormap(gdkScreen);

    return NULL;
}
#endif

GdkDrawable *
gfxPlatformGtk::GetGdkDrawable(gfxASurface *target)
{
    if (target->CairoStatus())
        return nsnull;

    GdkDrawable *result;

    result = (GdkDrawable*) cairo_surface_get_user_data (target->CairoSurface(),
                                                         &cairo_gdk_drawable_key);
    if (result)
        return result;

#ifdef MOZ_X11
    if (target->GetType() == gfxASurface::SurfaceTypeXlib) {
        gfxXlibSurface *xs = (gfxXlibSurface*) target;

        // try looking it up in gdk's table
        result = (GdkDrawable*) gdk_xid_table_lookup(xs->XDrawable());
        if (result) {
            SetGdkDrawable(target, result);
            return result;
        }

        // If all else fails, try doing a foreign_new
        // but don't bother if we can't get a colormap.
        // Without a colormap GDK won't know how to draw.
        Screen *screen = cairo_xlib_surface_get_screen(xs->CairoSurface());
        Visual *visual = cairo_xlib_surface_get_visual(xs->CairoSurface());
        GdkColormap *cmap = LookupGdkColormapForVisual(screen, visual);
        if (cmap == None)
            return nsnull;

        result = (GdkDrawable*) gdk_pixmap_foreign_new_for_display
            (gdk_display_get_default(), xs->XDrawable());
        if (result) {
            gdk_drawable_set_colormap(result, cmap);

            SetGdkDrawable(target, result);
            // Release our ref.  The object is held by target.  Caller will
            // only need to ref if it wants to keep the drawable longer than
            // target.
            g_object_unref(result);
            return result;
        }
    }
#endif

    return nsnull;
}
