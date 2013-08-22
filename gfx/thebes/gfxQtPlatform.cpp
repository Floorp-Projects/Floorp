/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <QPixmap>
#include <qglobal.h>
#if (QT_VERSION < QT_VERSION_CHECK(5,0,0))
#  include <QX11Info>
#else
#  include <qpa/qplatformnativeinterface.h>
#  include <qpa/qplatformintegration.h>
#endif
#include <QApplication>
#include <QDesktopWidget>
#include <QPaintEngine>

#include "gfxQtPlatform.h"

#include "gfxFontconfigUtils.h"

#include "mozilla/gfx/2D.h"

#include "cairo.h"

#include "gfxImageSurface.h"
#include "gfxQPainterSurface.h"
#include "nsUnicodeProperties.h"

#ifdef MOZ_PANGO
#include "gfxPangoFonts.h"
#include "gfxContext.h"
#include "gfxUserFontSet.h"
#else
#include "gfxFT2Fonts.h"
#endif

#include "nsUnicharUtils.h"

#include <fontconfig/fontconfig.h>

#include "nsMathUtils.h"
#include "nsTArray.h"
#ifdef MOZ_X11
#include "gfxXlibSurface.h"
#endif

#include "qcms.h"

#ifndef MOZ_PANGO
#include <ft2build.h>
#include FT_FREETYPE_H
#endif

#include "mozilla/Preferences.h"

using namespace mozilla;
using namespace mozilla::unicode;
using namespace mozilla::gfx;

#if (QT_VERSION < QT_VERSION_CHECK(5,0,0))
#define DEFAULT_RENDER_MODE RENDER_DIRECT
#else
#define DEFAULT_RENDER_MODE RENDER_BUFFERED
#endif

static QPaintEngine::Type sDefaultQtPaintEngineType = QPaintEngine::Raster;
gfxFontconfigUtils *gfxQtPlatform::sFontconfigUtils = nullptr;
static cairo_user_data_key_t cairo_qt_pixmap_key;
static void do_qt_pixmap_unref (void *data)
{
    QPixmap *pmap = (QPixmap*)data;
    delete pmap;
}

static gfxImageFormat sOffscreenFormat = gfxASurface::ImageFormatRGB24;

#ifndef MOZ_PANGO
typedef nsDataHashtable<nsStringHashKey, nsRefPtr<FontFamily> > FontTable;
typedef nsDataHashtable<nsCStringHashKey, nsTArray<nsRefPtr<FontEntry> > > PrefFontTable;
static FontTable *gPlatformFonts = nullptr;
static FontTable *gPlatformFontAliases = nullptr;
static PrefFontTable *gPrefFonts = nullptr;
static gfxSparseBitSet *gCodepointsWithNoFonts = nullptr;
static FT_Library gPlatformFTLibrary = nullptr;
#endif

gfxQtPlatform::gfxQtPlatform()
{
    mPrefFonts.Init(50);

    if (!sFontconfigUtils)
        sFontconfigUtils = gfxFontconfigUtils::GetFontconfigUtils();

#ifdef MOZ_PANGO
    g_type_init();
#else
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

    nsresult rv;
    // 0 - default gfxQPainterSurface
    // 1 - gfxImageSurface
    int32_t ival = Preferences::GetInt("mozilla.widget-qt.render-mode", DEFAULT_RENDER_MODE);

    const char *envTypeOverride = getenv("MOZ_QT_RENDER_TYPE");
    if (envTypeOverride)
        ival = atoi(envTypeOverride);

    switch (ival) {
        case 0:
            mRenderMode = RENDER_QPAINTER;
            break;
        case 1:
            mRenderMode = RENDER_BUFFERED;
            break;
        case 2:
            mRenderMode = RENDER_DIRECT;
            break;
        default:
            mRenderMode = RENDER_QPAINTER;
    }

    // Qt doesn't provide a public API to detect the graphicssystem type. We hack
    // around this by checking what type of graphicssystem a test QPixmap uses.
    QPixmap pixmap(1, 1);
    if (pixmap.depth() == 16) {
        sOffscreenFormat = gfxASurface::ImageFormatRGB16_565;
    }
    mScreenDepth = pixmap.depth();
#if (QT_VERSION < QT_VERSION_CHECK(4,8,0))
    if (pixmap.paintEngine())
        sDefaultQtPaintEngineType = pixmap.paintEngine()->type();
#endif
}

gfxQtPlatform::~gfxQtPlatform()
{
    gfxFontconfigUtils::Shutdown();
    sFontconfigUtils = nullptr;

#ifdef MOZ_PANGO
    gfxPangoFontGroup::Shutdown();
#else
    delete gPlatformFonts;
    gPlatformFonts = nullptr;
    delete gPlatformFontAliases;
    gPlatformFontAliases = nullptr;
    delete gPrefFonts;
    gPrefFonts = nullptr;
    delete gCodepointsWithNoFonts;
    gCodepointsWithNoFonts = nullptr;

    cairo_debug_reset_static_data();

    FT_Done_FreeType(gPlatformFTLibrary);
    gPlatformFTLibrary = nullptr;
#endif

#if 0
    // It would be nice to do this (although it might need to be after
    // the cairo shutdown that happens in ~gfxPlatform).  It even looks
    // idempotent.  But it has fatal assertions that fire if stuff is
    // leaked, and we hit them.
    FcFini();
#endif
}

#ifdef MOZ_X11
Display*
gfxQtPlatform::GetXDisplay(QWidget* aWindow)
{
#if (QT_VERSION < QT_VERSION_CHECK(5,0,0))
#ifdef Q_WS_X11
  return aWindow ? aWindow->x11Info().display() : QX11Info::display();
#else
  return nullptr;
#endif
#else
  return (Display*)(qApp->platformNativeInterface()->
    nativeResourceForWindow("display", aWindow ? aWindow->windowHandle() : nullptr));
#endif
}

Screen*
gfxQtPlatform::GetXScreen(QWidget* aWindow)
{
#if (QT_VERSION < QT_VERSION_CHECK(5,0,0))
#ifdef Q_WS_X11
  return ScreenOfDisplay(GetXDisplay(aWindow), aWindow ? aWindow->x11Info().screen() : QX11Info().screen());
#else
  return nullptr;
#endif
#else
  return ScreenOfDisplay(GetXDisplay(aWindow),
                         (int)(intptr_t)qApp->platformNativeInterface()->
                           nativeResourceForWindow("screen",
                             aWindow ? aWindow->windowHandle() : nullptr));
#endif
}
#endif

already_AddRefed<gfxASurface>
gfxQtPlatform::CreateOffscreenSurface(const gfxIntSize& size,
                                      gfxASurface::gfxContentType contentType)
{
    nsRefPtr<gfxASurface> newSurface = nullptr;

    gfxASurface::gfxImageFormat imageFormat = OptimalFormatForContent(contentType);

#ifdef CAIRO_HAS_QT_SURFACE
    if (mRenderMode == RENDER_QPAINTER) {
      newSurface = new gfxQPainterSurface(size, imageFormat);
      return newSurface.forget();
    }
#endif

    if ((mRenderMode == RENDER_BUFFERED || mRenderMode == RENDER_DIRECT) &&
        sDefaultQtPaintEngineType != QPaintEngine::X11) {
      newSurface = new gfxImageSurface(size, imageFormat);
      return newSurface.forget();
    }

#ifdef MOZ_X11
    XRenderPictFormat* xrenderFormat =
        gfxXlibSurface::FindRenderFormat(GetXDisplay(), imageFormat);

    Screen* screen = GetXScreen();
    newSurface = gfxXlibSurface::Create(screen, xrenderFormat, size);
#endif

    if (newSurface) {
        gfxContext ctx(newSurface);
        ctx.SetOperator(gfxContext::OPERATOR_CLEAR);
        ctx.Paint();
    }

    return newSurface.forget();
}

nsresult
gfxQtPlatform::GetFontList(nsIAtom *aLangGroup,
                           const nsACString& aGenericFamily,
                           nsTArray<nsString>& aListOfFonts)
{
    return sFontconfigUtils->GetFontList(aLangGroup, aGenericFamily,
                                         aListOfFonts);
}

nsresult
gfxQtPlatform::UpdateFontList()
{
#ifndef MOZ_PANGO
    FcPattern *pat = nullptr;
    FcObjectSet *os = nullptr;
    FcFontSet *fs = nullptr;

    pat = FcPatternCreate();
    os = FcObjectSetBuild(FC_FAMILY, FC_FILE, FC_INDEX, FC_WEIGHT, FC_SLANT, FC_WIDTH, nullptr);

    fs = FcFontList(nullptr, pat, os);


    for (int i = 0; i < fs->nfont; i++) {
        char *str;

        if (FcPatternGetString(fs->fonts[i], FC_FAMILY, 0, (FcChar8 **) &str) != FcResultMatch)
            continue;

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
        }

        int x;
        if (FcPatternGetInteger(fs->fonts[i], FC_INDEX, 0, &x) == FcResultMatch) {
            fe->mFTFontIndex = x;
        } else {
            fe->mFTFontIndex = 0;
        }

        if (FcPatternGetInteger(fs->fonts[i], FC_WEIGHT, 0, &x) == FcResultMatch) {
            switch(x) {
            case 0:
                fe->mWeight = 100;
                break;
            case 40:
                fe->mWeight = 200;
                break;
            case 50:
                fe->mWeight = 300;
                break;
            case 75:
            case 80:
                fe->mWeight = 400;
                break;
            case 100:
                fe->mWeight = 500;
                break;
            case 180:
                fe->mWeight = 600;
                break;
            case 200:
                fe->mWeight = 700;
                break;
            case 205:
                fe->mWeight = 800;
                break;
            case 210:
                fe->mWeight = 900;
                break;
            default:
                // rough estimate
                fe->mWeight = (((x * 4) + 100) / 100) * 100;
                break;
            }
        }

        fe->mItalic = false;
        if (FcPatternGetInteger(fs->fonts[i], FC_SLANT, 0, &x) == FcResultMatch) {
            switch (x) {
            case FC_SLANT_ITALIC:
            case FC_SLANT_OBLIQUE:
                fe->mItalic = true;
            }
        }

        // XXX deal with font-stretch (FC_WIDTH) stuff later
    }

    if (pat)
        FcPatternDestroy(pat);
    if (os)
        FcObjectSetDestroy(os);
    if (fs)
        FcFontSetDestroy(fs);
#endif

    return sFontconfigUtils->UpdateFontList();
}

nsresult
gfxQtPlatform::ResolveFontName(const nsAString& aFontName,
                                FontResolverCallback aCallback,
                                void *aClosure,
                                bool& aAborted)
{
#ifdef MOZ_PANGO
    return sFontconfigUtils->ResolveFontName(aFontName, aCallback,
                                             aClosure, aAborted);
#else
    nsAutoString name(aFontName);
    ToLowerCase(name);

    nsRefPtr<FontFamily> ff;
    if (gPlatformFonts->Get(name, &ff) ||
        gPlatformFontAliases->Get(name, &ff)) {
        aAborted = !(*aCallback)(ff->Name(), aClosure);
        return NS_OK;
    }

    nsAutoCString utf8Name = NS_ConvertUTF16toUTF8(aFontName);

    FcPattern *npat = FcPatternCreate();
    FcPatternAddString(npat, FC_FAMILY, (FcChar8*)utf8Name.get());
    FcObjectSet *nos = FcObjectSetBuild(FC_FAMILY, nullptr);
    FcFontSet *nfs = FcFontList(nullptr, npat, nos);

    for (int k = 0; k < nfs->nfont; k++) {
        FcChar8 *str;
        if (FcPatternGetString(nfs->fonts[k], FC_FAMILY, 0, (FcChar8 **) &str) != FcResultMatch)
            continue;
        nsAutoString altName = NS_ConvertUTF8toUTF16(nsDependentCString(reinterpret_cast<char*>(str)));
        ToLowerCase(altName);
        if (gPlatformFonts->Get(altName, &ff)) {
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
    FcConfigSubstitute(nullptr, npat, FcMatchPattern);
    FcDefaultSubstitute(npat);

    nos = FcObjectSetBuild(FC_FAMILY, nullptr);
    nfs = FcFontList(nullptr, npat, nos);

    FcResult fresult;

    FcPattern *match = FcFontMatch(nullptr, npat, &fresult);
    if (match)
        FcFontSetAdd(nfs, match);

    for (int k = 0; k < nfs->nfont; k++) {
        FcChar8 *str;
        if (FcPatternGetString(nfs->fonts[k], FC_FAMILY, 0, (FcChar8 **) &str) != FcResultMatch)
            continue;
        nsAutoString altName = NS_ConvertUTF8toUTF16(nsDependentCString(reinterpret_cast<char*>(str)));
        ToLowerCase(altName);
        if (gPlatformFonts->Get(altName, &ff)) {
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
#endif
}

nsresult
gfxQtPlatform::GetStandardFamilyName(const nsAString& aFontName, nsAString& aFamilyName)
{
    return sFontconfigUtils->GetStandardFamilyName(aFontName, aFamilyName);
}

gfxFontGroup *
gfxQtPlatform::CreateFontGroup(const nsAString &aFamilies,
                               const gfxFontStyle *aStyle,
                               gfxUserFontSet* aUserFontSet)
{
#ifdef MOZ_PANGO
    return new gfxPangoFontGroup(aFamilies, aStyle, aUserFontSet);
#else
    return new gfxFT2FontGroup(aFamilies, aStyle, aUserFontSet);
#endif
}

#ifdef MOZ_PANGO
gfxFontEntry*
gfxQtPlatform::LookupLocalFont(const gfxProxyFontEntry *aProxyEntry,
                                const nsAString& aFontName)
{
    return gfxPangoFontGroup::NewFontEntry(*aProxyEntry, aFontName);
}

gfxFontEntry*
gfxQtPlatform::MakePlatformFont(const gfxProxyFontEntry *aProxyEntry,
                                 const uint8_t *aFontData, uint32_t aLength)
{
    // passing ownership of the font data to the new font entry
    return gfxPangoFontGroup::NewFontEntry(*aProxyEntry,
                                           aFontData, aLength);
}

bool
gfxQtPlatform::IsFontFormatSupported(nsIURI *aFontURI, uint32_t aFormatFlags)
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
#endif

qcms_profile*
gfxQtPlatform::GetPlatformCMSOutputProfile()
{
    return nullptr;
}

#ifndef MOZ_PANGO
FT_Library
gfxQtPlatform::GetFTLibrary()
{
    return gPlatformFTLibrary;
}

FontFamily *
gfxQtPlatform::FindFontFamily(const nsAString& aName)
{
    nsAutoString name(aName);
    ToLowerCase(name);

    nsRefPtr<FontFamily> ff;
    if (!gPlatformFonts->Get(name, &ff)) {
        return nullptr;
    }
    return ff.get();
}

FontEntry *
gfxQtPlatform::FindFontEntry(const nsAString& aName, const gfxFontStyle& aFontStyle)
{
    nsRefPtr<FontFamily> ff = FindFontFamily(aName);
    if (!ff)
        return nullptr;

    return ff->FindFontEntry(aFontStyle);
}

static PLDHashOperator
FindFontForCharProc(nsStringHashKey::KeyType aKey,
                    nsRefPtr<FontFamily>& aFontFamily,
                    void* aUserArg)
{
    GlobalFontMatch *data = (GlobalFontMatch*)aUserArg;
    aFontFamily->FindFontForChar(data);
    return PL_DHASH_NEXT;
}

already_AddRefed<gfxFont>
gfxQtPlatform::FindFontForChar(uint32_t aCh, gfxFont *aFont)
{
    if (!gPlatformFonts || !gCodepointsWithNoFonts)
        return nullptr;

    // is codepoint with no matching font? return null immediately
    if (gCodepointsWithNoFonts->test(aCh)) {
        return nullptr;
    }

    GlobalFontMatch data(aCh, GetScriptCode(aCh),
                         (aFont ? aFont->GetStyle() : nullptr));

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

    return nullptr;
}

bool
gfxQtPlatform::GetPrefFontEntries(const nsCString& aKey, nsTArray<nsRefPtr<gfxFontEntry> > *array)
{
    return mPrefFonts.Get(aKey, array);
}

void
gfxQtPlatform::SetPrefFontEntries(const nsCString& aKey, nsTArray<nsRefPtr<gfxFontEntry> >& array)
{
    mPrefFonts.Put(aKey, array);
}

#endif

int32_t
gfxQtPlatform::GetDPI()
{
    QDesktopWidget* rootWindow = qApp->desktop();
    int32_t dpi = rootWindow->logicalDpiY(); // y-axis DPI for fonts
    return dpi <= 0 ? 96 : dpi;
}

gfxImageFormat
gfxQtPlatform::GetOffscreenFormat()
{
    return sOffscreenFormat;
}

int
gfxQtPlatform::GetScreenDepth() const
{
    return mScreenDepth;
}

