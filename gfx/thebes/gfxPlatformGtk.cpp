/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define PANGO_ENABLE_BACKEND
#define PANGO_ENABLE_ENGINE

#include "gfxPlatformGtk.h"
#include "prenv.h"

#include "nsUnicharUtils.h"
#include "nsUnicodeProperties.h"
#include "gfx2DGlue.h"
#include "gfxFcPlatformFontList.h"
#include "gfxFontconfigUtils.h"
#include "gfxFontconfigFonts.h"
#include "gfxContext.h"
#include "gfxUserFontSet.h"
#include "gfxUtils.h"
#include "gfxFT2FontBase.h"
#include "gfxPrefs.h"

#include "mozilla/gfx/2D.h"

#include "cairo.h"
#include <gtk/gtk.h>

#include "gfxImageSurface.h"
#ifdef MOZ_X11
#include <gdk/gdkx.h>
#include "gfxXlibSurface.h"
#include "cairo-xlib.h"
#include "mozilla/Preferences.h"
#include "mozilla/X11Util.h"

/* Undefine the Status from Xlib since it will conflict with system headers on OSX */
#if defined(__APPLE__) && defined(Status)
#undef Status
#endif

#endif /* MOZ_X11 */

#include <fontconfig/fontconfig.h>

#include "nsMathUtils.h"

#define GDK_PIXMAP_SIZE_MAX 32767

#define GFX_PREF_MAX_GENERIC_SUBSTITUTIONS "gfx.font_rendering.fontconfig.max_generic_substitutions"

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::unicode;

gfxFontconfigUtils *gfxPlatformGtk::sFontconfigUtils = nullptr;

#if (MOZ_WIDGET_GTK == 2)
static cairo_user_data_key_t cairo_gdk_drawable_key;
#endif

#ifdef MOZ_X11
    bool gfxPlatformGtk::sUseXRender = true;
#endif

bool gfxPlatformGtk::sUseFcFontList = false;

gfxPlatformGtk::gfxPlatformGtk()
{
    gtk_init(nullptr, nullptr);

    sUseFcFontList = mozilla::Preferences::GetBool("gfx.font_rendering.fontconfig.fontlist.enabled");
    if (!sUseFcFontList && !sFontconfigUtils) {
        sFontconfigUtils = gfxFontconfigUtils::GetFontconfigUtils();
    }

    mMaxGenericSubstitutions = UNINITIALIZED_VALUE;

#ifdef MOZ_X11
    sUseXRender = (GDK_IS_X11_DISPLAY(gdk_display_get_default())) ? 
                    mozilla::Preferences::GetBool("gfx.xrender.enabled") : false;
#endif

    uint32_t canvasMask = BackendTypeBit(BackendType::CAIRO) | BackendTypeBit(BackendType::SKIA);
    uint32_t contentMask = BackendTypeBit(BackendType::CAIRO) | BackendTypeBit(BackendType::SKIA);
    InitBackendPrefs(canvasMask, BackendType::CAIRO,
                     contentMask, BackendType::CAIRO);
}

gfxPlatformGtk::~gfxPlatformGtk()
{
    if (!sUseFcFontList) {
        gfxFontconfigUtils::Shutdown();
        sFontconfigUtils = nullptr;
        gfxPangoFontGroup::Shutdown();
    }
}

void
gfxPlatformGtk::FlushContentDrawing()
{
    if (UseXRender()) {
        XFlush(DefaultXDisplay());
    }
}

already_AddRefed<gfxASurface>
gfxPlatformGtk::CreateOffscreenSurface(const IntSize& aSize,
                                       gfxImageFormat aFormat)
{
    RefPtr<gfxASurface> newSurface;
    bool needsClear = true;
#ifdef MOZ_X11
    // XXX we really need a different interface here, something that passes
    // in more context, including the display and/or target surface type that
    // we should try to match
    GdkScreen *gdkScreen = gdk_screen_get_default();
    if (gdkScreen) {
        // When forcing PaintedLayers to use image surfaces for content,
        // force creation of gfxImageSurface surfaces.
        if (UseXRender() && !UseImageOffscreenSurfaces()) {
            Screen *screen = gdk_x11_screen_get_xscreen(gdkScreen);
            XRenderPictFormat* xrenderFormat =
                gfxXlibSurface::FindRenderFormat(DisplayOfScreen(screen),
                                                 aFormat);

            if (xrenderFormat) {
                newSurface = gfxXlibSurface::Create(screen, xrenderFormat,
                                                    aSize);
            }
        } else {
            // We're not going to use XRender, so we don't need to
            // search for a render format
            newSurface = new gfxImageSurface(aSize, aFormat);
            // The gfxImageSurface ctor zeroes this for us, no need to
            // waste time clearing again
            needsClear = false;
        }
    }
#endif

    if (!newSurface) {
        // We couldn't create a native surface for whatever reason;
        // e.g., no display, no RENDER, bad size, etc.
        // Fall back to image surface for the data.
        newSurface = new gfxImageSurface(aSize, aFormat);
    }

    if (newSurface->CairoStatus()) {
        newSurface = nullptr; // surface isn't valid for some reason
    }

    if (newSurface && needsClear) {
        gfxUtils::ClearThebesSurface(newSurface);
    }

    return newSurface.forget();
}

nsresult
gfxPlatformGtk::GetFontList(nsIAtom *aLangGroup,
                            const nsACString& aGenericFamily,
                            nsTArray<nsString>& aListOfFonts)
{
    if (sUseFcFontList) {
        gfxPlatformFontList::PlatformFontList()->GetFontList(aLangGroup,
                                                             aGenericFamily,
                                                             aListOfFonts);
        return NS_OK;
    }

    return sFontconfigUtils->GetFontList(aLangGroup,
                                         aGenericFamily,
                                         aListOfFonts);
}

nsresult
gfxPlatformGtk::UpdateFontList()
{
    if (sUseFcFontList) {
        gfxPlatformFontList::PlatformFontList()->UpdateFontList();
        return NS_OK;
    }

    return sFontconfigUtils->UpdateFontList();
}

// xxx - this is ubuntu centric, need to go through other distros and flesh
// out a more general list
static const char kFontDejaVuSans[] = "DejaVu Sans";
static const char kFontDejaVuSerif[] = "DejaVu Serif";
static const char kFontFreeSans[] = "FreeSans";
static const char kFontFreeSerif[] = "FreeSerif";
static const char kFontTakaoPGothic[] = "TakaoPGothic";
static const char kFontDroidSansFallback[] = "Droid Sans Fallback";
static const char kFontWenQuanYiMicroHei[] = "WenQuanYi Micro Hei";
static const char kFontNanumGothic[] = "NanumGothic";

void
gfxPlatformGtk::GetCommonFallbackFonts(uint32_t aCh, uint32_t aNextCh,
                                       Script aRunScript,
                                       nsTArray<const char*>& aFontList)
{
    aFontList.AppendElement(kFontDejaVuSerif);
    aFontList.AppendElement(kFontFreeSerif);
    aFontList.AppendElement(kFontDejaVuSans);
    aFontList.AppendElement(kFontFreeSans);

    // add fonts for CJK ranges
    // xxx - this isn't really correct, should use the same CJK font ordering
    // as the pref font code
    if (aCh >= 0x3000 &&
        ((aCh < 0xe000) ||
         (aCh >= 0xf900 && aCh < 0xfff0) ||
         ((aCh >> 16) == 2))) {
        aFontList.AppendElement(kFontTakaoPGothic);
        aFontList.AppendElement(kFontDroidSansFallback);
        aFontList.AppendElement(kFontWenQuanYiMicroHei);
        aFontList.AppendElement(kFontNanumGothic);
    }
}

gfxPlatformFontList*
gfxPlatformGtk::CreatePlatformFontList()
{
    gfxPlatformFontList* list = new gfxFcPlatformFontList();
    if (NS_SUCCEEDED(list->InitFontList())) {
        return list;
    }
    gfxPlatformFontList::Shutdown();
    return nullptr;
}

nsresult
gfxPlatformGtk::GetStandardFamilyName(const nsAString& aFontName, nsAString& aFamilyName)
{
    if (sUseFcFontList) {
        gfxPlatformFontList::PlatformFontList()->
            GetStandardFamilyName(aFontName, aFamilyName);
        return NS_OK;
    }

    return sFontconfigUtils->GetStandardFamilyName(aFontName, aFamilyName);
}

gfxFontGroup *
gfxPlatformGtk::CreateFontGroup(const FontFamilyList& aFontFamilyList,
                                const gfxFontStyle* aStyle,
                                gfxTextPerfMetrics* aTextPerf,
                                gfxUserFontSet* aUserFontSet,
                                gfxFloat aDevToCssSize)
{
    if (sUseFcFontList) {
        return new gfxFontGroup(aFontFamilyList, aStyle, aTextPerf,
                                aUserFontSet, aDevToCssSize);
    }

    return new gfxPangoFontGroup(aFontFamilyList, aStyle,
                                 aUserFontSet, aDevToCssSize);
}

gfxFontEntry*
gfxPlatformGtk::LookupLocalFont(const nsAString& aFontName,
                                uint16_t aWeight,
                                int16_t aStretch,
                                uint8_t aStyle)
{
    if (sUseFcFontList) {
        gfxPlatformFontList* pfl = gfxPlatformFontList::PlatformFontList();
        return pfl->LookupLocalFont(aFontName, aWeight, aStretch,
                                    aStyle);
    }

    return gfxPangoFontGroup::NewFontEntry(aFontName, aWeight,
                                           aStretch, aStyle);
}

gfxFontEntry* 
gfxPlatformGtk::MakePlatformFont(const nsAString& aFontName,
                                 uint16_t aWeight,
                                 int16_t aStretch,
                                 uint8_t aStyle,
                                 const uint8_t* aFontData,
                                 uint32_t aLength)
{
    if (sUseFcFontList) {
        gfxPlatformFontList* pfl = gfxPlatformFontList::PlatformFontList();
        return pfl->MakePlatformFont(aFontName, aWeight, aStretch,
                                     aStyle, aFontData, aLength);
    }

    // passing ownership of the font data to the new font entry
    return gfxPangoFontGroup::NewFontEntry(aFontName, aWeight,
                                           aStretch, aStyle,
                                           aFontData, aLength);
}

bool
gfxPlatformGtk::IsFontFormatSupported(nsIURI *aFontURI, uint32_t aFormatFlags)
{
    // check for strange format flags
    NS_ASSERTION(!(aFormatFlags & gfxUserFontSet::FLAG_FORMAT_NOT_USED),
                 "strange font format hint set");

    // accept supported formats
    // Pango doesn't apply features from AAT TrueType extensions.
    // Assume that if this is the only SFNT format specified,
    // then AAT extensions are required for complex script support.
    if (aFormatFlags & gfxUserFontSet::FLAG_FORMATS_COMMON) {
        return true;
    }

    // reject all other formats, known and unknown
    if (aFormatFlags != 0) {
        return false;
    }

    // no format hint set, need to look at data
    return true;
}

static int32_t sDPI = 0;

int32_t
gfxPlatformGtk::GetDPI()
{
    if (!sDPI) {
        // Make sure init is run so we have a resolution
        GdkScreen *screen = gdk_screen_get_default();
        gtk_settings_get_for_screen(screen);
        sDPI = int32_t(round(gdk_screen_get_resolution(screen)));
        if (sDPI <= 0) {
            // Fall back to something sane
            sDPI = 96;
        }
    }
    return sDPI;
}

double
gfxPlatformGtk::GetDPIScale()
{
    // Integer scale factors work well with GTK window scaling, image scaling,
    // and pixel alignment, but there is a range where 1 is too small and 2 is
    // too big.  An additional step of 1.5 is added because this is common
    // scale on WINNT and at this ratio the advantages of larger rendering
    // outweigh the disadvantages from scaling and pixel mis-alignment.
    int32_t dpi = GetDPI();
    if (dpi < 144) {
        return 1.0;
    } else if (dpi < 168) {
        return 1.5;
    } else {
        return round(dpi/96.0);
    }
}

bool
gfxPlatformGtk::UseImageOffscreenSurfaces()
{
    return GetDefaultContentBackend() != mozilla::gfx::BackendType::CAIRO ||
           gfxPrefs::UseImageOffscreenSurfaces();
}

gfxImageFormat
gfxPlatformGtk::GetOffscreenFormat()
{
    // Make sure there is a screen
    GdkScreen *screen = gdk_screen_get_default();
    if (screen && gdk_visual_get_depth(gdk_visual_get_system()) == 16) {
        return SurfaceFormat::R5G6B5_UINT16;
    }

    return SurfaceFormat::X8R8G8B8_UINT32;
}

void gfxPlatformGtk::FontsPrefsChanged(const char *aPref)
{
    // only checking for generic substitions, pass other changes up
    if (strcmp(GFX_PREF_MAX_GENERIC_SUBSTITUTIONS, aPref)) {
        gfxPlatform::FontsPrefsChanged(aPref);
        return;
    }

    mMaxGenericSubstitutions = UNINITIALIZED_VALUE;
    if (sUseFcFontList) {
        gfxFcPlatformFontList* pfl = gfxFcPlatformFontList::PlatformFontList();
        pfl->ClearGenericMappings();
        FlushFontAndWordCaches();
    }
}

uint32_t gfxPlatformGtk::MaxGenericSubstitions()
{
    if (mMaxGenericSubstitutions == UNINITIALIZED_VALUE) {
        mMaxGenericSubstitutions =
            Preferences::GetInt(GFX_PREF_MAX_GENERIC_SUBSTITUTIONS, 3);
        if (mMaxGenericSubstitutions < 0) {
            mMaxGenericSubstitutions = 3;
        }
    }

    return uint32_t(mMaxGenericSubstitutions);
}

void
gfxPlatformGtk::GetPlatformCMSOutputProfile(void *&mem, size_t &size)
{
    mem = nullptr;
    size = 0;

#ifdef MOZ_X11
    GdkDisplay *display = gdk_display_get_default();
    if (!GDK_IS_X11_DISPLAY(display))
        return;

    const char EDID1_ATOM_NAME[] = "XFree86_DDC_EDID1_RAWDATA";
    const char ICC_PROFILE_ATOM_NAME[] = "_ICC_PROFILE";

    Atom edidAtom, iccAtom;
    Display *dpy = GDK_DISPLAY_XDISPLAY(display);
    // In xpcshell tests, we never initialize X and hence don't have a Display.
    // In this case, there's no output colour management to be done, so we just
    // return with nullptr.
    if (!dpy)
        return;
 
    Window root = gdk_x11_get_default_root_xwindow();

    Atom retAtom;
    int retFormat;
    unsigned long retLength, retAfter;
    unsigned char *retProperty ;

    iccAtom = XInternAtom(dpy, ICC_PROFILE_ATOM_NAME, TRUE);
    if (iccAtom) {
        // read once to get size, once for the data
        if (Success == XGetWindowProperty(dpy, root, iccAtom,
                                          0, INT_MAX /* length */,
                                          False, AnyPropertyType,
                                          &retAtom, &retFormat, &retLength,
                                          &retAfter, &retProperty)) {

            if (retLength > 0) {
                void *buffer = malloc(retLength);
                if (buffer) {
                    memcpy(buffer, retProperty, retLength);
                    mem = buffer;
                    size = retLength;
                }
            }

            XFree(retProperty);
            if (size > 0) {
#ifdef DEBUG_tor
                fprintf(stderr,
                        "ICM profile read from %s successfully\n",
                        ICC_PROFILE_ATOM_NAME);
#endif
                return;
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
                return;
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

            qcms_data_create_rgb_with_gamma(whitePoint, primaries, gamma, &mem, &size);

#ifdef DEBUG_tor
            if (size > 0) {
                fprintf(stderr,
                        "ICM profile read from %s successfully\n",
                        EDID1_ATOM_NAME);
            }
#endif
        }
    }
#endif
}


#if (MOZ_WIDGET_GTK == 2)
void
gfxPlatformGtk::SetGdkDrawable(cairo_surface_t *target,
                               GdkDrawable *drawable)
{
    if (cairo_surface_status(target))
        return;

    g_object_ref(drawable);

    cairo_surface_set_user_data (target,
                                 &cairo_gdk_drawable_key,
                                 drawable,
                                 g_object_unref);
}

GdkDrawable *
gfxPlatformGtk::GetGdkDrawable(cairo_surface_t *target)
{
    if (cairo_surface_status(target))
        return nullptr;

    GdkDrawable *result;

    result = (GdkDrawable*) cairo_surface_get_user_data (target,
                                                         &cairo_gdk_drawable_key);
    if (result)
        return result;

#ifdef MOZ_X11
    if (cairo_surface_get_type(target) != CAIRO_SURFACE_TYPE_XLIB)
        return nullptr;

    // try looking it up in gdk's table
    result = (GdkDrawable*) gdk_xid_table_lookup(cairo_xlib_surface_get_drawable(target));
    if (result) {
        SetGdkDrawable(target, result);
        return result;
    }
#endif

    return nullptr;
}
#endif

already_AddRefed<ScaledFont>
gfxPlatformGtk::GetScaledFontForFont(DrawTarget* aTarget, gfxFont *aFont)
{
    return GetScaledFontForFontWithCairoSkia(aTarget, aFont);
}
