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

#include "gfxPangoFonts.h"
#include "gfxContext.h"
#include "gfxUserFontSet.h"

#include "nsUnicharUtils.h"

#include <fontconfig/fontconfig.h>

#include "nsMathUtils.h"
#include "nsTArray.h"
#ifdef MOZ_X11
#include "gfxXlibSurface.h"
#endif

#include "qcms.h"

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

static gfxImageFormat sOffscreenFormat = gfxImageFormatRGB24;

gfxQtPlatform::gfxQtPlatform()
{
    mPrefFonts.Init(50);

    if (!sFontconfigUtils)
        sFontconfigUtils = gfxFontconfigUtils::GetFontconfigUtils();

    g_type_init();

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
        sOffscreenFormat = gfxImageFormatRGB16_565;
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

    gfxPangoFontGroup::Shutdown();

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
                                      gfxContentType contentType)
{
    nsRefPtr<gfxASurface> newSurface = nullptr;

    gfxImageFormat imageFormat = OptimalFormatForContent(contentType);

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
    return sFontconfigUtils->UpdateFontList();
}

nsresult
gfxQtPlatform::ResolveFontName(const nsAString& aFontName,
                                FontResolverCallback aCallback,
                                void *aClosure,
                                bool& aAborted)
{
    return sFontconfigUtils->ResolveFontName(aFontName, aCallback,
                                             aClosure, aAborted);
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
    return new gfxPangoFontGroup(aFamilies, aStyle, aUserFontSet);
}

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

qcms_profile*
gfxQtPlatform::GetPlatformCMSOutputProfile()
{
    return nullptr;
}

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

