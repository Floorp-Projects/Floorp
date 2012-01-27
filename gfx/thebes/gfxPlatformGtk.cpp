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

#include "nsUnicharUtils.h"
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

#include "mozilla/gfx/2D.h"

#include "cairo.h"
#include <gtk/gtk.h>

#include "gfxImageSurface.h"
#ifdef MOZ_X11
#include <gdk/gdkx.h>
#include "gfxXlibSurface.h"
#include "cairo-xlib.h"

/* Undefine the Status from Xlib since it will conflict with system headers on OSX */
#if defined(__APPLE__) && defined(Status)
#undef Status
#endif

#endif /* MOZ_X11 */

#include <fontconfig/fontconfig.h>

#include "nsMathUtils.h"

#define GDK_PIXMAP_SIZE_MAX 32767

#ifndef MOZ_PANGO
#include <ft2build.h>
#include FT_FREETYPE_H
#endif

using namespace mozilla;
using namespace mozilla::gfx;

gfxFontconfigUtils *gfxPlatformGtk::sFontconfigUtils = nsnull;

#ifndef MOZ_PANGO
typedef nsDataHashtable<nsStringHashKey, nsRefPtr<FontFamily> > FontTable;
typedef nsDataHashtable<nsCStringHashKey, nsTArray<nsRefPtr<gfxFontEntry> > > PrefFontTable;
static FontTable *gPlatformFonts = NULL;
static FontTable *gPlatformFontAliases = NULL;
static PrefFontTable *gPrefFonts = NULL;
static gfxSparseBitSet *gCodepointsWithNoFonts = NULL;
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
    gPrefFonts = new PrefFontTable();
    gPrefFonts->Init(100);
    gCodepointsWithNoFonts = new gfxSparseBitSet();
    UpdateFontList();
#endif
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
    delete gPrefFonts;
    gPrefFonts = NULL;
    delete gCodepointsWithNoFonts;
    gCodepointsWithNoFonts = NULL;

#ifdef NS_FREE_PERMANENT_DATA
    // do cairo cleanup *before* closing down the FTLibrary,
    // otherwise we'll crash when the gfxPlatform destructor
    // calls it (bug 605009)
    cairo_debug_reset_static_data();

    FT_Done_FreeType(gPlatformFTLibrary);
    gPlatformFTLibrary = NULL;
#endif
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
                                       gfxASurface::gfxContentType contentType)
{
    nsRefPtr<gfxASurface> newSurface;
    bool needsClear = true;
    gfxASurface::gfxImageFormat imageFormat = gfxASurface::FormatFromContent(contentType);
#ifdef MOZ_X11
    // XXX we really need a different interface here, something that passes
    // in more context, including the display and/or target surface type that
    // we should try to match
    GdkScreen *gdkScreen = gdk_screen_get_default();
    if (gdkScreen) {
        // try to optimize it for 16bpp default screen
        if (gfxASurface::CONTENT_COLOR == contentType) {
            imageFormat = GetOffscreenFormat();
        }

        if (UseClientSideRendering()) {
            // We're not going to use XRender, so we don't need to
            // search for a render format
            newSurface = new gfxImageSurface(size, imageFormat);
            // The gfxImageSurface ctor zeroes this for us, no need to
            // waste time clearing again
            needsClear = false;
        } else {
            Screen *screen = gdk_x11_screen_get_xscreen(gdkScreen);
            XRenderPictFormat* xrenderFormat =
                gfxXlibSurface::FindRenderFormat(DisplayOfScreen(screen),
                                                 imageFormat);

            if (xrenderFormat) {
                newSurface = gfxXlibSurface::Create(screen, xrenderFormat, size);
            }
        }
    }
#endif

    if (!newSurface) {
        // We couldn't create a native surface for whatever reason;
        // e.g., no display, no RENDER, bad size, etc.
        // Fall back to image surface for the data.
        newSurface = new gfxImageSurface(size, imageFormat);
    }

    if (newSurface->CairoStatus()) {
        newSurface = nsnull; // surface isn't valid for some reason
    }

    if (newSurface && needsClear) {
        gfxContext tmpCtx(newSurface);
        tmpCtx.SetOperator(gfxContext::OPERATOR_CLEAR);
        tmpCtx.Paint();
    }

    return newSurface.forget();
}

#ifdef MOZ_PANGO

nsresult
gfxPlatformGtk::GetFontList(nsIAtom *aLangGroup,
                            const nsACString& aGenericFamily,
                            nsTArray<nsString>& aListOfFonts)
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
                                bool& aAborted)
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
gfxPlatformGtk::LookupLocalFont(const gfxProxyFontEntry *aProxyEntry,
                                const nsAString& aFontName)
{
    return gfxPangoFontGroup::NewFontEntry(*aProxyEntry, aFontName);
}

gfxFontEntry* 
gfxPlatformGtk::MakePlatformFont(const gfxProxyFontEntry *aProxyEntry, 
                                 const PRUint8 *aFontData, PRUint32 aLength)
{
    // passing ownership of the font data to the new font entry
    return gfxPangoFontGroup::NewFontEntry(*aProxyEntry,
                                           aFontData, aLength);
}

bool
gfxPlatformGtk::IsFontFormatSupported(nsIURI *aFontURI, PRUint32 aFormatFlags)
{
    // check for strange format flags
    NS_ASSERTION(!(aFormatFlags & gfxUserFontSet::FLAG_FORMAT_NOT_USED),
                 "strange font format hint set");

    // accept supported formats
    // Pango doesn't apply features from AAT TrueType extensions.
    // Assume that if this is the only SFNT format specified,
    // then AAT extensions are required for complex script support.
    if (aFormatFlags & (gfxUserFontSet::FLAG_FORMAT_WOFF     |
                        gfxUserFontSet::FLAG_FORMAT_OPENTYPE | 
                        gfxUserFontSet::FLAG_FORMAT_TRUETYPE)) {
        return true;
    }

    // reject all other formats, known and unknown
    if (aFormatFlags != 0) {
        return false;
    }

    // no format hint set, need to look at data
    return true;
}

#else

nsresult
gfxPlatformGtk::GetFontList(nsIAtom *aLangGroup,
                            const nsACString& aGenericFamily,
                            nsTArray<nsString>& aListOfFonts)
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
        ToLowerCase(key);
        nsRefPtr<FontFamily> ff;
        if (!gPlatformFonts->Get(key, &ff)) {
            ff = new FontFamily(name);
            gPlatformFonts->Put(key, ff);
        }

        FontEntry *fe = new FontEntry(ff->Name());
        ff->AddFontEntry(fe);

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

        fe->mItalic = false;
        if (FcPatternGetInteger(fs->fonts[i], FC_SLANT, 0, &x) == FcResultMatch) {
            switch (x) {
            case FC_SLANT_ITALIC:
            case FC_SLANT_OBLIQUE:
                fe->mItalic = true;
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
                                bool& aAborted)
{

    nsAutoString name(aFontName);
    ToLowerCase(name);

    nsRefPtr<FontFamily> ff;
    if (gPlatformFonts->Get(name, &ff) ||
        gPlatformFontAliases->Get(name, &ff)) {
        aAborted = !(*aCallback)(ff->Name(), aClosure);
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
        ToLowerCase(altName);
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
        ToLowerCase(altName);
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
                                const gfxFontStyle *aStyle,
                                gfxUserFontSet *aUserFontSet)
{
    return new gfxFT2FontGroup(aFamilies, aStyle, aUserFontSet);
}

#endif

static PRInt32 sDPI = 0;

PRInt32
gfxPlatformGtk::GetDPI()
{
    if (!sDPI) {
        // Make sure init is run so we have a resolution
        GdkScreen *screen = gdk_screen_get_default();
        gtk_settings_get_for_screen(screen);
        sDPI = PRInt32(round(gdk_screen_get_resolution(screen)));
        if (sDPI <= 0) {
            // Fall back to something sane
            sDPI = 96;
        }
    }
    return sDPI;
}

gfxImageFormat
gfxPlatformGtk::GetOffscreenFormat()
{
    if (gdk_visual_get_system()->depth == 16) {
        return gfxASurface::ImageFormatRGB16_565;
    }

    return gfxASurface::ImageFormatRGB24;
}

qcms_profile *
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

            qcms_profile* profile = NULL;

            if (retLength > 0)
                profile = qcms_profile_from_memory(retProperty, retLength);

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
            qcms_CIE_xyY whitePoint;
            qcms_CIE_xyYTRIPLE primaries;

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

            primaries.red.x = ((retProperty[0x1b] << 2) |
                               (retProperty[0x19] >> 6 & 3)) / 1024.0;
            primaries.red.y = ((retProperty[0x1c] << 2) |
                               (retProperty[0x19] >> 4 & 3)) / 1024.0;
            primaries.red.Y = 1.0;

            primaries.green.x = ((retProperty[0x1d] << 2) |
                                 (retProperty[0x19] >> 2 & 3)) / 1024.0;
            primaries.green.y = ((retProperty[0x1e] << 2) |
                                 (retProperty[0x19] >> 0 & 3)) / 1024.0;
            primaries.green.Y = 1.0;

            primaries.blue.x = ((retProperty[0x1f] << 2) |
                               (retProperty[0x1a] >> 6 & 3)) / 1024.0;
            primaries.blue.y = ((retProperty[0x20] << 2) |
                               (retProperty[0x1a] >> 4 & 3)) / 1024.0;
            primaries.blue.Y = 1.0;

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

            qcms_profile* profile =
                qcms_profile_create_rgb_with_gamma(whitePoint, primaries, gamma);

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
    ToLowerCase(name);

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

static PLDHashOperator
FindFontForCharProc(nsStringHashKey::KeyType aKey,
                    nsRefPtr<FontFamily>& aFontFamily,
                    void* aUserArg)
{
    FontSearch *data = (FontSearch*)aUserArg;
    aFontFamily->FindFontForChar(data);
    return PL_DHASH_NEXT;
}

already_AddRefed<gfxFont>
gfxPlatformGtk::FindFontForChar(PRUint32 aCh, gfxFont *aFont)
{
    if (!gPlatformFonts || !gCodepointsWithNoFonts)
        return nsnull;

    // is codepoint with no matching font? return null immediately
    if (gCodepointsWithNoFonts->test(aCh)) {
        return nsnull;
    }

    FontSearch data(aCh, aFont);

    // find fonts that support the character
    gPlatformFonts->Enumerate(FindFontForCharProc, &data);

    if (data.mBestMatch) {
        nsRefPtr<gfxFT2Font> font =
            gfxFT2Font::GetOrMakeFont(static_cast<FontEntry*>(data.mBestMatch.get()),
                                      aFont->GetStyle()); 
        gfxFont* ret = font.forget().get();
        return already_AddRefed<gfxFont>(ret);
    }

    // no match? add to set of non-matching codepoints
    gCodepointsWithNoFonts->set(aCh);

    return nsnull;
}

bool
gfxPlatformGtk::GetPrefFontEntries(const nsCString& aKey, nsTArray<nsRefPtr<gfxFontEntry> > *aFontEntryList)
{
    return gPrefFonts->Get(aKey, aFontEntryList);
}

void
gfxPlatformGtk::SetPrefFontEntries(const nsCString& aKey, nsTArray<nsRefPtr<gfxFontEntry> >& aFontEntryList)
{
    gPrefFonts->Put(aKey, aFontEntryList);
}
#endif


void
gfxPlatformGtk::SetGdkDrawable(gfxASurface *target,
                               GdkDrawable *drawable)
{
    if (target->CairoStatus())
        return;

    g_object_ref(drawable);

    cairo_surface_set_user_data (target->CairoSurface(),
                                 &cairo_gdk_drawable_key,
                                 drawable,
                                 do_gdk_drawable_unref);
}

GdkDrawable *
gfxPlatformGtk::GetGdkDrawable(gfxASurface *target)
{
    if (target->CairoStatus())
        return NULL;

    GdkDrawable *result;

    result = (GdkDrawable*) cairo_surface_get_user_data (target->CairoSurface(),
                                                         &cairo_gdk_drawable_key);
    if (result)
        return result;

#ifdef MOZ_X11
    if (target->GetType() != gfxASurface::SurfaceTypeXlib)
        return NULL;

    gfxXlibSurface *xs = static_cast<gfxXlibSurface*>(target);

    // try looking it up in gdk's table
    result = (GdkDrawable*) gdk_xid_table_lookup(xs->XDrawable());
    if (result) {
        SetGdkDrawable(target, result);
        return result;
    }
#endif

    return NULL;
}

RefPtr<ScaledFont>
gfxPlatformGtk::GetScaledFontForFont(gfxFont *aFont)
{
  NativeFont nativeFont;
  nativeFont.mType = NATIVE_FONT_SKIA_FONT_FACE;
  nativeFont.mFont = aFont;
  RefPtr<ScaledFont> scaledFont =
    Factory::CreateScaledFontForNativeFont(nativeFont, aFont->GetAdjustedSize());

  return scaledFont;
}

bool
gfxPlatformGtk::SupportsAzure(BackendType& aBackend)
{
  aBackend = BACKEND_SKIA;
  return true;
}

