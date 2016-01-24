/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDeviceContext.h"
#include <algorithm>                    // for max
#include "gfxASurface.h"                // for gfxASurface, etc
#include "gfxContext.h"
#include "gfxFont.h"                    // for gfxFontGroup
#include "gfxImageSurface.h"            // for gfxImageSurface
#include "gfxPoint.h"                   // for gfxSize
#include "mozilla/Attributes.h"         // for final
#include "mozilla/gfx/PathHelpers.h"
#include "mozilla/Preferences.h"        // for Preferences
#include "mozilla/Services.h"           // for GetObserverService
#include "mozilla/mozalloc.h"           // for operator new
#include "nsCRT.h"                      // for nsCRT
#include "nsDebug.h"                    // for NS_NOTREACHED, NS_ASSERTION, etc
#include "nsFont.h"                     // for nsFont
#include "nsFontMetrics.h"              // for nsFontMetrics
#include "nsIAtom.h"                    // for nsIAtom, do_GetAtom
#include "nsID.h"
#include "nsIDeviceContextSpec.h"       // for nsIDeviceContextSpec
#include "nsILanguageAtomService.h"     // for nsILanguageAtomService, etc
#include "nsIObserver.h"                // for nsIObserver, etc
#include "nsIObserverService.h"         // for nsIObserverService
#include "nsIScreen.h"                  // for nsIScreen
#include "nsIScreenManager.h"           // for nsIScreenManager
#include "nsISupportsImpl.h"            // for MOZ_COUNT_CTOR, etc
#include "nsISupportsUtils.h"           // for NS_ADDREF, NS_RELEASE
#include "nsIWidget.h"                  // for nsIWidget, NS_NATIVE_WINDOW
#include "nsRect.h"                     // for nsRect
#include "nsServiceManagerUtils.h"      // for do_GetService
#include "nsString.h"               // for nsDependentString
#include "nsTArray.h"                   // for nsTArray, nsTArray_Impl
#include "nsThreadUtils.h"              // for NS_IsMainThread
#include "mozilla/gfx/Logging.h"

#if !XP_MACOSX
#include "gfxPDFSurface.h"
#endif

#ifdef MOZ_WIDGET_GTK
#include "gfxPSSurface.h"
#elif XP_WIN
#include "gfxWindowsSurface.h"
#elif XP_MACOSX
#include "gfxQuartzSurface.h"
#endif

using namespace mozilla;
using namespace mozilla::gfx;
using mozilla::services::GetObserverService;

class nsFontCache final : public nsIObserver
{
public:
    nsFontCache()   { MOZ_COUNT_CTOR(nsFontCache); }

    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER

    void Init(nsDeviceContext* aContext);
    void Destroy();

    nsresult GetMetricsFor(const nsFont& aFont,
                           nsIAtom* aLanguage, bool aExplicitLanguage,
                           gfxFont::Orientation aOrientation,
                           gfxUserFontSet* aUserFontSet,
                           gfxTextPerfMetrics* aTextPerf,
                           nsFontMetrics*& aMetrics);

    void FontMetricsDeleted(const nsFontMetrics* aFontMetrics);
    void Compact();
    void Flush();

protected:
    ~nsFontCache()  { MOZ_COUNT_DTOR(nsFontCache); }

    nsDeviceContext*          mContext; // owner
    nsCOMPtr<nsIAtom>         mLocaleLanguage;
    nsTArray<nsFontMetrics*>  mFontMetrics;
};

NS_IMPL_ISUPPORTS(nsFontCache, nsIObserver)

// The Init and Destroy methods are necessary because it's not
// safe to call AddObserver from a constructor or RemoveObserver
// from a destructor.  That should be fixed.
void
nsFontCache::Init(nsDeviceContext* aContext)
{
    mContext = aContext;
    // register as a memory-pressure observer to free font resources
    // in low-memory situations.
    nsCOMPtr<nsIObserverService> obs = GetObserverService();
    if (obs)
        obs->AddObserver(this, "memory-pressure", false);

    nsCOMPtr<nsILanguageAtomService> langService;
    langService = do_GetService(NS_LANGUAGEATOMSERVICE_CONTRACTID);
    if (langService) {
        mLocaleLanguage = langService->GetLocaleLanguage();
    }
    if (!mLocaleLanguage) {
        mLocaleLanguage = do_GetAtom("x-western");
    }
}

void
nsFontCache::Destroy()
{
    nsCOMPtr<nsIObserverService> obs = GetObserverService();
    if (obs)
        obs->RemoveObserver(this, "memory-pressure");
    Flush();
}

NS_IMETHODIMP
nsFontCache::Observe(nsISupports*, const char* aTopic, const char16_t*)
{
    if (!nsCRT::strcmp(aTopic, "memory-pressure"))
        Compact();
    return NS_OK;
}

nsresult
nsFontCache::GetMetricsFor(const nsFont& aFont,
                           nsIAtom* aLanguage, bool aExplicitLanguage,
                           gfxFont::Orientation aOrientation,
                           gfxUserFontSet* aUserFontSet,
                           gfxTextPerfMetrics* aTextPerf,
                           nsFontMetrics*& aMetrics)
{
    if (!aLanguage)
        aLanguage = mLocaleLanguage;

    // First check our cache
    // start from the end, which is where we put the most-recent-used element

    nsFontMetrics* fm;
    int32_t n = mFontMetrics.Length() - 1;
    for (int32_t i = n; i >= 0; --i) {
        fm = mFontMetrics[i];
        if (fm->Font().Equals(aFont) && fm->GetUserFontSet() == aUserFontSet &&
            fm->Language() == aLanguage && fm->Orientation() == aOrientation) {
            if (i != n) {
                // promote it to the end of the cache
                mFontMetrics.RemoveElementAt(i);
                mFontMetrics.AppendElement(fm);
            }
            fm->GetThebesFontGroup()->UpdateUserFonts();
            NS_ADDREF(aMetrics = fm);
            return NS_OK;
        }
    }

    // It's not in the cache. Get font metrics and then cache them.

    fm = new nsFontMetrics();
    NS_ADDREF(fm);
    nsresult rv = fm->Init(aFont, aLanguage, aExplicitLanguage, aOrientation,
                           mContext, aUserFontSet, aTextPerf);
    if (NS_SUCCEEDED(rv)) {
        // the mFontMetrics list has the "head" at the end, because append
        // is cheaper than insert
        mFontMetrics.AppendElement(fm);
        aMetrics = fm;
        NS_ADDREF(aMetrics);
        return NS_OK;
    }
    fm->Destroy();
    NS_RELEASE(fm);

    // One reason why Init() fails is because the system is running out of
    // resources. e.g., on Win95/98 only a very limited number of GDI
    // objects are available. Compact the cache and try again.

    Compact();
    fm = new nsFontMetrics();
    NS_ADDREF(fm);
    rv = fm->Init(aFont, aLanguage, aExplicitLanguage, aOrientation, mContext,
                  aUserFontSet, aTextPerf);
    if (NS_SUCCEEDED(rv)) {
        mFontMetrics.AppendElement(fm);
        aMetrics = fm;
        return NS_OK;
    }
    fm->Destroy();
    NS_RELEASE(fm);

    // could not setup a new one, send an old one (XXX search a "best
    // match"?)

    n = mFontMetrics.Length() - 1; // could have changed in Compact()
    if (n >= 0) {
        aMetrics = mFontMetrics[n];
        NS_ADDREF(aMetrics);
        return NS_OK;
    }

    NS_POSTCONDITION(NS_SUCCEEDED(rv),
                     "font metrics should not be null - bug 136248");
    return rv;
}

void
nsFontCache::FontMetricsDeleted(const nsFontMetrics* aFontMetrics)
{
    mFontMetrics.RemoveElement(aFontMetrics);
}

void
nsFontCache::Compact()
{
    // Need to loop backward because the running element can be removed on
    // the way
    for (int32_t i = mFontMetrics.Length()-1; i >= 0; --i) {
        nsFontMetrics* fm = mFontMetrics[i];
        nsFontMetrics* oldfm = fm;
        // Destroy() isn't here because we want our device context to be
        // notified
        NS_RELEASE(fm); // this will reset fm to nullptr
        // if the font is really gone, it would have called back in
        // FontMetricsDeleted() and would have removed itself
        if (mFontMetrics.IndexOf(oldfm) != mFontMetrics.NoIndex) {
            // nope, the font is still there, so let's hold onto it too
            NS_ADDREF(oldfm);
        }
    }
}

void
nsFontCache::Flush()
{
    for (int32_t i = mFontMetrics.Length()-1; i >= 0; --i) {
        nsFontMetrics* fm = mFontMetrics[i];
        // Destroy() will unhook our device context from the fm so that we
        // won't waste time in triggering the notification of
        // FontMetricsDeleted() in the subsequent release
        fm->Destroy();
        NS_RELEASE(fm);
    }
    mFontMetrics.Clear();
}

nsDeviceContext::nsDeviceContext()
    : mWidth(0), mHeight(0), mDepth(0),
      mAppUnitsPerDevPixel(-1), mAppUnitsPerDevPixelAtUnitFullZoom(-1),
      mAppUnitsPerPhysicalInch(-1),
      mFullZoom(1.0f), mPrintingScale(1.0f),
      mFontCache(nullptr)
{
    MOZ_ASSERT(NS_IsMainThread(), "nsDeviceContext created off main thread");
}

// Note: we use a bare pointer for mFontCache so that nsFontCache
// can be an incomplete type in nsDeviceContext.h.
// Therefore we have to do all the refcounting by hand.
nsDeviceContext::~nsDeviceContext()
{
    if (mFontCache) {
        mFontCache->Destroy();
        NS_RELEASE(mFontCache);
    }
}

nsresult
nsDeviceContext::GetMetricsFor(const nsFont& aFont,
                               nsIAtom* aLanguage,
                               bool aExplicitLanguage,
                               gfxFont::Orientation aOrientation,
                               gfxUserFontSet* aUserFontSet,
                               gfxTextPerfMetrics* aTextPerf,
                               nsFontMetrics*& aMetrics)
{
    if (!mFontCache) {
        mFontCache = new nsFontCache();
        NS_ADDREF(mFontCache);
        mFontCache->Init(this);
    }

    return mFontCache->GetMetricsFor(aFont, aLanguage, aExplicitLanguage,
                                     aOrientation, aUserFontSet, aTextPerf,
                                     aMetrics);
}

nsresult
nsDeviceContext::FlushFontCache(void)
{
    if (mFontCache)
        mFontCache->Flush();
    return NS_OK;
}

nsresult
nsDeviceContext::FontMetricsDeleted(const nsFontMetrics* aFontMetrics)
{
    if (mFontCache) {
        mFontCache->FontMetricsDeleted(aFontMetrics);
    }
    return NS_OK;
}

bool
nsDeviceContext::IsPrinterSurface()
{
    return mPrintingSurface != nullptr;
}

void
nsDeviceContext::SetDPI()
{
    float dpi = -1.0f;

    // Use the printing DC to determine DPI values, if we have one.
    if (mDeviceContextSpec) {
        dpi = mDeviceContextSpec->GetDPI();
        mPrintingScale = mDeviceContextSpec->GetPrintingScale();
        mAppUnitsPerDevPixelAtUnitFullZoom =
            NS_lround((AppUnitsPerCSSPixel() * 96) / dpi);
    } else {
        // A value of -1 means use the maximum of 96 and the system DPI.
        // A value of 0 means use the system DPI. A positive value is used as the DPI.
        // This sets the physical size of a device pixel and thus controls the
        // interpretation of physical units.
        int32_t prefDPI = Preferences::GetInt("layout.css.dpi", -1);

        if (prefDPI > 0) {
            dpi = prefDPI;
        } else if (mWidget) {
            dpi = mWidget->GetDPI();

            if (prefDPI < 0) {
                dpi = std::max(96.0f, dpi);
            }
        } else {
            dpi = 96.0f;
        }

        CSSToLayoutDeviceScale scale = mWidget ? mWidget->GetDefaultScale()
                                               : CSSToLayoutDeviceScale(1.0);
        double devPixelsPerCSSPixel = scale.scale;

        mAppUnitsPerDevPixelAtUnitFullZoom =
            std::max(1, NS_lround(AppUnitsPerCSSPixel() / devPixelsPerCSSPixel));
    }

    NS_ASSERTION(dpi != -1.0, "no dpi set");

    mAppUnitsPerPhysicalInch = NS_lround(dpi * mAppUnitsPerDevPixelAtUnitFullZoom);
    UpdateAppUnitsForFullZoom();
}

nsresult
nsDeviceContext::Init(nsIWidget *aWidget)
{
    nsresult rv = NS_OK;
    if (mScreenManager && mWidget == aWidget)
        return rv;

    mWidget = aWidget;
    SetDPI();

    if (mScreenManager)
        return rv;

    mScreenManager = do_GetService("@mozilla.org/gfx/screenmanager;1", &rv);

    return rv;
}

already_AddRefed<gfxContext>
nsDeviceContext::CreateRenderingContext()
{
    MOZ_ASSERT(mWidth > 0 && mHeight > 0);

    RefPtr<gfxASurface> printingSurface = mPrintingSurface;
#ifdef XP_MACOSX
    // CreateRenderingContext() can be called (on reflow) after EndPage()
    // but before BeginPage().  On OS X (and only there) mPrintingSurface
    // will in this case be null, because OS X printing surfaces are
    // per-page, and therefore only truly valid between calls to BeginPage()
    // and EndPage().  But we can get away with fudging things here, if need
    // be, by using a cached copy.
    if (!printingSurface) {
      printingSurface = mCachedPrintingSurface;
    }
#endif

    RefPtr<gfx::DrawTarget> dt =
      gfxPlatform::GetPlatform()->CreateDrawTargetForSurface(printingSurface,
                                                             gfx::IntSize(mWidth, mHeight));

    // This can legitimately happen - CreateDrawTargetForSurface will fail
    // to create a draw target if the size is too large, for instance.
    if (!dt) {
        gfxCriticalNote << "Failed to create draw target in device context sized " << mWidth << "x" << mHeight << " and pointers " << hexa(mPrintingSurface) << " and " << hexa(printingSurface);
        return nullptr;
    }

    RefPtr<DrawEventRecorder> recorder;
    nsresult rv = mDeviceContextSpec->GetDrawEventRecorder(getter_AddRefs(recorder));
    if (NS_SUCCEEDED(rv) && recorder) {
      dt = gfx::Factory::CreateRecordingDrawTarget(recorder, dt);
    }

#ifdef XP_MACOSX
    dt->AddUserData(&gfxContext::sDontUseAsSourceKey, dt, nullptr);
#endif
    dt->AddUserData(&sDisablePixelSnapping, (void*)0x1, nullptr);

    RefPtr<gfxContext> pContext = new gfxContext(dt);

    gfxMatrix transform;
    if (printingSurface->GetRotateForLandscape()) {
      // Rotate page 90 degrees to draw landscape page on portrait paper
      IntSize size = printingSurface->GetSize();
      transform.Translate(gfxPoint(0, size.width));
      gfxMatrix rotate(0, -1,
                       1,  0,
                       0,  0);
      transform = rotate * transform;
    }
    transform.Scale(mPrintingScale, mPrintingScale);

    pContext->SetMatrix(transform);
    return pContext.forget();
}

nsresult
nsDeviceContext::GetDepth(uint32_t& aDepth)
{
    if (mDepth == 0 && mScreenManager) {
        nsCOMPtr<nsIScreen> primaryScreen;
        mScreenManager->GetPrimaryScreen(getter_AddRefs(primaryScreen));
        primaryScreen->GetColorDepth(reinterpret_cast<int32_t *>(&mDepth));
    }

    aDepth = mDepth;
    return NS_OK;
}

nsresult
nsDeviceContext::GetDeviceSurfaceDimensions(nscoord &aWidth, nscoord &aHeight)
{
    if (mPrintingSurface) {
        // we have a printer device
        aWidth = mWidth;
        aHeight = mHeight;
    } else {
        nsRect area;
        ComputeFullAreaUsingScreen(&area);
        aWidth = area.width;
        aHeight = area.height;
    }

    return NS_OK;
}

nsresult
nsDeviceContext::GetRect(nsRect &aRect)
{
    if (mPrintingSurface) {
        // we have a printer device
        aRect.x = 0;
        aRect.y = 0;
        aRect.width = mWidth;
        aRect.height = mHeight;
    } else
        ComputeFullAreaUsingScreen ( &aRect );

    return NS_OK;
}

nsresult
nsDeviceContext::GetClientRect(nsRect &aRect)
{
    if (mPrintingSurface) {
        // we have a printer device
        aRect.x = 0;
        aRect.y = 0;
        aRect.width = mWidth;
        aRect.height = mHeight;
    }
    else
        ComputeClientRectUsingScreen(&aRect);

    return NS_OK;
}

nsresult
nsDeviceContext::InitForPrinting(nsIDeviceContextSpec *aDevice)
{
    NS_ENSURE_ARG_POINTER(aDevice);

    mDeviceContextSpec = aDevice;

    nsresult rv = aDevice->GetSurfaceForPrinter(getter_AddRefs(mPrintingSurface));
    if (NS_FAILED(rv))
        return NS_ERROR_FAILURE;

    Init(nullptr);

    if (!CalcPrintingSize()) {
        return NS_ERROR_FAILURE;
    }

    return NS_OK;
}

nsresult
nsDeviceContext::BeginDocument(const nsAString& aTitle,
                               const nsAString& aPrintToFileName,
                               int32_t          aStartPage,
                               int32_t          aEndPage)
{
    nsresult rv = mPrintingSurface->BeginPrinting(aTitle, aPrintToFileName);

    if (NS_SUCCEEDED(rv) && mDeviceContextSpec) {
      rv = mDeviceContextSpec->BeginDocument(aTitle, aPrintToFileName,
                                             aStartPage, aEndPage);
    }

    return rv;
}


nsresult
nsDeviceContext::EndDocument(void)
{
    nsresult rv = NS_OK;

    if (mPrintingSurface) {
        rv = mPrintingSurface->EndPrinting();
        if (NS_SUCCEEDED(rv))
            mPrintingSurface->Finish();
    }

    if (mDeviceContextSpec)
        mDeviceContextSpec->EndDocument();

    return rv;
}


nsresult
nsDeviceContext::AbortDocument(void)
{
    nsresult rv = mPrintingSurface->AbortPrinting();

    if (mDeviceContextSpec)
        mDeviceContextSpec->EndDocument();

    return rv;
}


nsresult
nsDeviceContext::BeginPage(void)
{
    nsresult rv = NS_OK;

    if (mDeviceContextSpec)
        rv = mDeviceContextSpec->BeginPage();

    if (NS_FAILED(rv)) return rv;

#ifdef XP_MACOSX
    // We need to get a new surface for each page on the Mac, as the
    // CGContextRefs are only good for one page.
    mDeviceContextSpec->GetSurfaceForPrinter(getter_AddRefs(mPrintingSurface));
#endif

    rv = mPrintingSurface->BeginPage();

    return rv;
}

nsresult
nsDeviceContext::EndPage(void)
{
    nsresult rv = mPrintingSurface->EndPage();

#ifdef XP_MACOSX
    // We need to release the CGContextRef in the surface here, plus it's
    // not something you would want anyway, as these CGContextRefs are only
    // good for one page.  But we need to keep a cached reference to it, since
    // CreateRenderingContext() may try to access it when mPrintingSurface
    // would normally be null.  See bug 665218.  If we just stop nulling out
    // mPrintingSurface here (and thereby make that our cached copy), we'll
    // break all our null checks on mPrintingSurface.  See bug 684622.
    mCachedPrintingSurface = mPrintingSurface;
    mPrintingSurface = nullptr;
#endif

    if (mDeviceContextSpec)
        mDeviceContextSpec->EndPage();

    return rv;
}

void
nsDeviceContext::ComputeClientRectUsingScreen(nsRect* outRect)
{
    // we always need to recompute the clientRect
    // because the window may have moved onto a different screen. In the single
    // monitor case, we only need to do the computation if we haven't done it
    // once already, and remember that we have because we're assured it won't change.
    nsCOMPtr<nsIScreen> screen;
    FindScreen (getter_AddRefs(screen));
    if (screen) {
        int32_t x, y, width, height;
        screen->GetAvailRect(&x, &y, &width, &height);

        // convert to device units
        outRect->y = NSIntPixelsToAppUnits(y, AppUnitsPerDevPixel());
        outRect->x = NSIntPixelsToAppUnits(x, AppUnitsPerDevPixel());
        outRect->width = NSIntPixelsToAppUnits(width, AppUnitsPerDevPixel());
        outRect->height = NSIntPixelsToAppUnits(height, AppUnitsPerDevPixel());
    }
}

void
nsDeviceContext::ComputeFullAreaUsingScreen(nsRect* outRect)
{
    // if we have more than one screen, we always need to recompute the clientRect
    // because the window may have moved onto a different screen. In the single
    // monitor case, we only need to do the computation if we haven't done it
    // once already, and remember that we have because we're assured it won't change.
    nsCOMPtr<nsIScreen> screen;
    FindScreen ( getter_AddRefs(screen) );
    if ( screen ) {
        int32_t x, y, width, height;
        screen->GetRect ( &x, &y, &width, &height );

        // convert to device units
        outRect->y = NSIntPixelsToAppUnits(y, AppUnitsPerDevPixel());
        outRect->x = NSIntPixelsToAppUnits(x, AppUnitsPerDevPixel());
        outRect->width = NSIntPixelsToAppUnits(width, AppUnitsPerDevPixel());
        outRect->height = NSIntPixelsToAppUnits(height, AppUnitsPerDevPixel());

        mWidth = outRect->width;
        mHeight = outRect->height;
    }
}

//
// FindScreen
//
// Determines which screen intersects the largest area of the given surface.
//
void
nsDeviceContext::FindScreen(nsIScreen** outScreen)
{
    if (!mWidget || !mScreenManager) {
        return;
    }

    CheckDPIChange();

    if (mWidget->GetOwningTabChild()) {
        mScreenManager->ScreenForNativeWidget((void *)mWidget->GetOwningTabChild(),
                                              outScreen);
    }
    else if (mWidget->GetNativeData(NS_NATIVE_WINDOW)) {
        mScreenManager->ScreenForNativeWidget(mWidget->GetNativeData(NS_NATIVE_WINDOW),
                                              outScreen);
    }
    else {
        mScreenManager->GetPrimaryScreen(outScreen);
    }
}

bool
nsDeviceContext::CalcPrintingSize()
{
    if (!mPrintingSurface) {
        return (mWidth > 0 && mHeight > 0);
    }

    gfxSize size = mPrintingSurface->GetSize();
    // For printing, CSS inches and physical inches are identical
    // so it doesn't matter which we use here
    mWidth = NSToCoordRound(size.width * AppUnitsPerPhysicalInch()
                            / POINTS_PER_INCH_FLOAT);
    mHeight = NSToCoordRound(size.height * AppUnitsPerPhysicalInch()
                             / POINTS_PER_INCH_FLOAT);

    return (mWidth > 0 && mHeight > 0);
}

bool nsDeviceContext::CheckDPIChange() {
    int32_t oldDevPixels = mAppUnitsPerDevPixelAtUnitFullZoom;
    int32_t oldInches = mAppUnitsPerPhysicalInch;

    SetDPI();

    return oldDevPixels != mAppUnitsPerDevPixelAtUnitFullZoom ||
        oldInches != mAppUnitsPerPhysicalInch;
}

bool
nsDeviceContext::SetFullZoom(float aScale)
{
    if (aScale <= 0) {
        NS_NOTREACHED("Invalid full zoom value");
        return false;
    }
    int32_t oldAppUnitsPerDevPixel = mAppUnitsPerDevPixel;
    mFullZoom = aScale;
    UpdateAppUnitsForFullZoom();
    return oldAppUnitsPerDevPixel != mAppUnitsPerDevPixel;
}

void
nsDeviceContext::UpdateAppUnitsForFullZoom()
{
    mAppUnitsPerDevPixel =
        std::max(1, NSToIntRound(float(mAppUnitsPerDevPixelAtUnitFullZoom) / mFullZoom));
    // adjust mFullZoom to reflect appunit rounding
    mFullZoom = float(mAppUnitsPerDevPixelAtUnitFullZoom) / mAppUnitsPerDevPixel;
}
