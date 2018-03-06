/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 ts=4 expandtab: */
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
#include "mozilla/gfx/PrintTarget.h"
#include "mozilla/Preferences.h"        // for Preferences
#include "mozilla/Services.h"           // for GetObserverService
#include "mozilla/mozalloc.h"           // for operator new
#include "nsCRT.h"                      // for nsCRT
#include "nsDebug.h"                    // for NS_NOTREACHED, NS_ASSERTION, etc
#include "nsFont.h"                     // for nsFont
#include "nsFontMetrics.h"              // for nsFontMetrics
#include "nsAtom.h"                    // for nsAtom, NS_Atomize
#include "nsID.h"
#include "nsIDeviceContextSpec.h"       // for nsIDeviceContextSpec
#include "nsLanguageAtomService.h"      // for nsLanguageAtomService
#include "nsIObserver.h"                // for nsIObserver, etc
#include "nsIObserverService.h"         // for nsIObserverService
#include "nsIScreen.h"                  // for nsIScreen
#include "nsISupportsImpl.h"            // for MOZ_COUNT_CTOR, etc
#include "nsISupportsUtils.h"           // for NS_ADDREF, NS_RELEASE
#include "nsIWidget.h"                  // for nsIWidget, NS_NATIVE_WINDOW
#include "nsRect.h"                     // for nsRect
#include "nsServiceManagerUtils.h"      // for do_GetService
#include "nsString.h"               // for nsDependentString
#include "nsTArray.h"                   // for nsTArray, nsTArray_Impl
#include "nsThreadUtils.h"              // for NS_IsMainThread
#include "mozilla/gfx/Logging.h"
#include "mozilla/widget/ScreenManager.h" // for ScreenManager

using namespace mozilla;
using namespace mozilla::gfx;
using mozilla::services::GetObserverService;
using mozilla::widget::ScreenManager;

class nsFontCache final : public nsIObserver
{
public:
    nsFontCache() {}

    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER

    void Init(nsDeviceContext* aContext);
    void Destroy();

    already_AddRefed<nsFontMetrics> GetMetricsFor(
        const nsFont& aFont, const nsFontMetrics::Params& aParams);

    void FontMetricsDeleted(const nsFontMetrics* aFontMetrics);
    void Compact();
    void Flush();

    void UpdateUserFonts(gfxUserFontSet* aUserFontSet);

protected:
    ~nsFontCache() {}

    nsDeviceContext*          mContext; // owner
    RefPtr<nsAtom>         mLocaleLanguage;
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

    mLocaleLanguage = nsLanguageAtomService::GetService()->GetLocaleLanguage();
    if (!mLocaleLanguage) {
        mLocaleLanguage = NS_Atomize("x-western");
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

already_AddRefed<nsFontMetrics>
nsFontCache::GetMetricsFor(const nsFont& aFont,
                           const nsFontMetrics::Params& aParams)
{
    nsAtom* language = aParams.language ? aParams.language
                                         : mLocaleLanguage.get();

    // First check our cache
    // start from the end, which is where we put the most-recent-used element

    int32_t n = mFontMetrics.Length() - 1;
    for (int32_t i = n; i >= 0; --i) {
        nsFontMetrics* fm = mFontMetrics[i];
        if (fm->Font().Equals(aFont) &&
            fm->GetUserFontSet() == aParams.userFontSet &&
            fm->Language() == language &&
            fm->Orientation() == aParams.orientation) {
            if (i != n) {
                // promote it to the end of the cache
                mFontMetrics.RemoveElementAt(i);
                mFontMetrics.AppendElement(fm);
            }
            fm->GetThebesFontGroup()->UpdateUserFonts();
            return do_AddRef(fm);
        }
    }

    // It's not in the cache. Get font metrics and then cache them.

    nsFontMetrics::Params params = aParams;
    params.language = language;
    RefPtr<nsFontMetrics> fm = new nsFontMetrics(aFont, params, mContext);
    // the mFontMetrics list has the "head" at the end, because append
    // is cheaper than insert
    mFontMetrics.AppendElement(do_AddRef(fm).take());
    return fm.forget();
}

void
nsFontCache::UpdateUserFonts(gfxUserFontSet* aUserFontSet)
{
    for (nsFontMetrics* fm : mFontMetrics) {
        gfxFontGroup* fg = fm->GetThebesFontGroup();
        if (fg->GetUserFontSet() == aUserFontSet) {
            fg->UpdateUserFonts();
        }
    }
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
    : mWidth(0), mHeight(0),
      mAppUnitsPerDevPixel(-1), mAppUnitsPerDevPixelAtUnitFullZoom(-1),
      mAppUnitsPerPhysicalInch(-1),
      mFullZoom(1.0f), mPrintingScale(1.0f),
      mIsCurrentlyPrintingDoc(false)
#ifdef DEBUG
    , mIsInitialized(false)
#endif
{
    MOZ_ASSERT(NS_IsMainThread(), "nsDeviceContext created off main thread");
}

nsDeviceContext::~nsDeviceContext()
{
    if (mFontCache) {
        mFontCache->Destroy();
    }
}

void
nsDeviceContext::InitFontCache()
{
    if (!mFontCache) {
        mFontCache = new nsFontCache();
        mFontCache->Init(this);
    }
}

void
nsDeviceContext::UpdateFontCacheUserFonts(gfxUserFontSet* aUserFontSet)
{
    if (mFontCache) {
        mFontCache->UpdateUserFonts(aUserFontSet);
    }
}

already_AddRefed<nsFontMetrics>
nsDeviceContext::GetMetricsFor(const nsFont& aFont,
                               const nsFontMetrics::Params& aParams)
{
    InitFontCache();
    return mFontCache->GetMetricsFor(aFont, aParams);
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
nsDeviceContext::IsPrinterContext()
{
  return mPrintTarget != nullptr;
}

void
nsDeviceContext::SetDPI(double* aScale)
{
    float dpi = -1.0f;

    // Use the printing DC to determine DPI values, if we have one.
    if (mDeviceContextSpec) {
        dpi = mDeviceContextSpec->GetDPI();
        mPrintingScale = mDeviceContextSpec->GetPrintingScale();
        mAppUnitsPerDevPixelAtUnitFullZoom =
            NS_lround((AppUnitsPerCSSPixel() * 96) / dpi);
    } else {
        nsCOMPtr<nsIScreen> primaryScreen;
        ScreenManager& screenManager = ScreenManager::GetSingleton();
        screenManager.GetPrimaryScreen(getter_AddRefs(primaryScreen));
        MOZ_ASSERT(primaryScreen);

        // A value of -1 means use the maximum of 96 and the system DPI.
        // A value of 0 means use the system DPI. A positive value is used as the DPI.
        // This sets the physical size of a device pixel and thus controls the
        // interpretation of physical units.
        int32_t prefDPI = Preferences::GetInt("layout.css.dpi", -1);

        if (prefDPI > 0) {
            dpi = prefDPI;
        } else if (mWidget) {
            // PuppetWidget could return -1 if the value's not available yet.
            dpi = mWidget->GetDPI();
            // In case that the widget returns -1, use the primary screen's
            // value as default.
            if (dpi < 0) {
                primaryScreen->GetDpi(&dpi);
            }
            if (prefDPI < 0) {
                dpi = std::max(96.0f, dpi);
            }
        } else {
            dpi = 96.0f;
        }

        double devPixelsPerCSSPixel;
        if (aScale && *aScale > 0.0) {
            // if caller provided a scale, we just use it
            devPixelsPerCSSPixel = *aScale;
        } else {
            // otherwise get from the widget, and return it in aScale for
            // the caller to pass to child contexts if needed
            CSSToLayoutDeviceScale scale =
                mWidget ? mWidget->GetDefaultScale()
                        : CSSToLayoutDeviceScale(1.0);
            devPixelsPerCSSPixel = scale.scale;
            // In case that the widget returns -1, use the primary screen's
            // value as default.
            if (devPixelsPerCSSPixel < 0) {
                primaryScreen->GetDefaultCSSScaleFactor(&devPixelsPerCSSPixel);
            }
            if (aScale) {
                *aScale = devPixelsPerCSSPixel;
            }
        }

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
#ifdef DEBUG
    // We can't assert |!mIsInitialized| here since EndSwapDocShellsForDocument
    // re-initializes nsDeviceContext objects.  We can only assert in
    // InitForPrinting (below).
    mIsInitialized = true;
#endif

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

// XXX This is only for printing. We should make that obvious in the name.
already_AddRefed<gfxContext>
nsDeviceContext::CreateRenderingContext()
{
  return CreateRenderingContextCommon(/* not a reference context */ false);
}

already_AddRefed<gfxContext>
nsDeviceContext::CreateReferenceRenderingContext()
{
  return CreateRenderingContextCommon(/* a reference context */ true);
}

already_AddRefed<gfxContext>
nsDeviceContext::CreateRenderingContextCommon(bool aWantReferenceContext)
{
    MOZ_ASSERT(IsPrinterContext());
    MOZ_ASSERT(mWidth > 0 && mHeight > 0);

    RefPtr<gfx::DrawTarget> dt;
    if (aWantReferenceContext) {
      dt = mPrintTarget->GetReferenceDrawTarget();
    } else {
      // This will be null if e10s is disabled or print.print_via_parent=false.
      RefPtr<DrawEventRecorder> recorder;
      mDeviceContextSpec->GetDrawEventRecorder(getter_AddRefs(recorder));
      dt = mPrintTarget->MakeDrawTarget(gfx::IntSize(mWidth, mHeight), recorder);
    }

    if (!dt || !dt->IsValid()) {
      gfxCriticalNote
        << "Failed to create draw target in device context sized "
        << mWidth << "x" << mHeight << " and pointer "
        << hexa(mPrintTarget);
      return nullptr;
    }

#ifdef XP_MACOSX
    // The CGContextRef provided by PMSessionGetCGGraphicsContext is
    // write-only, so we need to prevent gfxContext::PushGroupAndCopyBackground
    // trying to read from it or else we'll crash.
    // XXXjwatt Consider adding a MakeDrawTarget override to PrintTargetCG and
    // moving this AddUserData call there.
    dt->AddUserData(&gfxContext::sDontUseAsSourceKey, dt, nullptr);
#endif
    dt->AddUserData(&sDisablePixelSnapping, (void*)0x1, nullptr);

    RefPtr<gfxContext> pContext = gfxContext::CreateOrNull(dt);
    MOZ_ASSERT(pContext); // already checked draw target above

    gfxMatrix transform;
    if (mPrintTarget->RotateNeededForLandscape()) {
      // Rotate page 90 degrees to draw landscape page on portrait paper
      IntSize size = mPrintTarget->GetSize();
      transform.PreTranslate(gfxPoint(0, size.width));
      gfxMatrix rotate(0, -1,
                       1,  0,
                       0,  0);
      transform = rotate * transform;
    }
    transform.PreScale(mPrintingScale, mPrintingScale);

    pContext->SetMatrixDouble(transform);
    return pContext.forget();
}

nsresult
nsDeviceContext::GetDepth(uint32_t& aDepth)
{
    nsCOMPtr<nsIScreen> screen;
    FindScreen(getter_AddRefs(screen));
    if (!screen) {
        ScreenManager& screenManager = ScreenManager::GetSingleton();
        screenManager.GetPrimaryScreen(getter_AddRefs(screen));
        MOZ_ASSERT(screen);
    }
    screen->GetColorDepth(reinterpret_cast<int32_t *>(&aDepth));

    return NS_OK;
}

nsresult
nsDeviceContext::GetDeviceSurfaceDimensions(nscoord &aWidth, nscoord &aHeight)
{
    if (IsPrinterContext()) {
        aWidth = mWidth;
        aHeight = mHeight;
    } else {
        nsRect area;
        ComputeFullAreaUsingScreen(&area);
        aWidth = area.Width();
        aHeight = area.Height();
    }

    return NS_OK;
}

nsresult
nsDeviceContext::GetRect(nsRect &aRect)
{
    if (IsPrinterContext()) {
        aRect.SetRect(0, 0, mWidth, mHeight);
    } else
        ComputeFullAreaUsingScreen ( &aRect );

    return NS_OK;
}

nsresult
nsDeviceContext::GetClientRect(nsRect &aRect)
{
    if (IsPrinterContext()) {
        aRect.SetRect(0, 0, mWidth, mHeight);
    }
    else
        ComputeClientRectUsingScreen(&aRect);

    return NS_OK;
}

nsresult
nsDeviceContext::InitForPrinting(nsIDeviceContextSpec *aDevice)
{
    NS_ENSURE_ARG_POINTER(aDevice);

    MOZ_ASSERT(!mIsInitialized,
               "Only initialize once, immediately after construction");

    // We don't set mIsInitialized here. The Init() call below does that.

    mPrintTarget = aDevice->MakePrintTarget();
    if (!mPrintTarget) {
        return NS_ERROR_FAILURE;
    }

    mDeviceContextSpec = aDevice;

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
    MOZ_ASSERT(!mIsCurrentlyPrintingDoc,
               "Mismatched BeginDocument/EndDocument calls");

    nsresult rv = mPrintTarget->BeginPrinting(aTitle, aPrintToFileName,
                                              aStartPage, aEndPage);

    if (NS_SUCCEEDED(rv)) {
        if (mDeviceContextSpec) {
           rv = mDeviceContextSpec->BeginDocument(aTitle, aPrintToFileName,
                                                  aStartPage, aEndPage);
        }
        mIsCurrentlyPrintingDoc = true;
    }

    // Warn about any failure (except user cancelling):
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv) || rv == NS_ERROR_ABORT,
                         "nsDeviceContext::BeginDocument failed");

    return rv;
}


nsresult
nsDeviceContext::EndDocument(void)
{
    MOZ_ASSERT(mIsCurrentlyPrintingDoc,
               "Mismatched BeginDocument/EndDocument calls");

    nsresult rv = NS_OK;

    mIsCurrentlyPrintingDoc = false;

    rv = mPrintTarget->EndPrinting();
    if (NS_SUCCEEDED(rv)) {
        mPrintTarget->Finish();
    }

    if (mDeviceContextSpec)
        mDeviceContextSpec->EndDocument();

    mPrintTarget = nullptr;

    return rv;
}


nsresult
nsDeviceContext::AbortDocument(void)
{
    MOZ_ASSERT(mIsCurrentlyPrintingDoc,
               "Mismatched BeginDocument/EndDocument calls");

    nsresult rv = mPrintTarget->AbortPrinting();

    mIsCurrentlyPrintingDoc = false;

    if (mDeviceContextSpec)
        mDeviceContextSpec->EndDocument();

    mPrintTarget = nullptr;

    return rv;
}


nsresult
nsDeviceContext::BeginPage(void)
{
    nsresult rv = NS_OK;

    if (mDeviceContextSpec)
        rv = mDeviceContextSpec->BeginPage();

    if (NS_FAILED(rv)) return rv;

    return mPrintTarget->BeginPage();
}

nsresult
nsDeviceContext::EndPage(void)
{
    nsresult rv = mPrintTarget->EndPage();

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
        outRect->SetRect(NSIntPixelsToAppUnits(x, AppUnitsPerDevPixel()),
                         NSIntPixelsToAppUnits(y, AppUnitsPerDevPixel()),
                         NSIntPixelsToAppUnits(width, AppUnitsPerDevPixel()),
                         NSIntPixelsToAppUnits(height, AppUnitsPerDevPixel()));
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
        outRect->SetRect(NSIntPixelsToAppUnits(x, AppUnitsPerDevPixel()),
                         NSIntPixelsToAppUnits(y, AppUnitsPerDevPixel()),
                         NSIntPixelsToAppUnits(width, AppUnitsPerDevPixel()),
                         NSIntPixelsToAppUnits(height, AppUnitsPerDevPixel()));
        mWidth = outRect->Width();
        mHeight = outRect->Height();
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

    nsCOMPtr<nsIScreen> screen = mWidget->GetWidgetScreen();
    screen.forget(outScreen);

    if (!(*outScreen)) {
        mScreenManager->GetPrimaryScreen(outScreen);
    }
}

bool
nsDeviceContext::CalcPrintingSize()
{
    gfxSize size(mPrintTarget->GetSize());
    // For printing, CSS inches and physical inches are identical
    // so it doesn't matter which we use here
    mWidth = NSToCoordRound(size.width * AppUnitsPerPhysicalInch()
                            / POINTS_PER_INCH_FLOAT);
    mHeight = NSToCoordRound(size.height * AppUnitsPerPhysicalInch()
                             / POINTS_PER_INCH_FLOAT);

    return (mWidth > 0 && mHeight > 0);
}

bool nsDeviceContext::CheckDPIChange(double* aScale)
{
    int32_t oldDevPixels = mAppUnitsPerDevPixelAtUnitFullZoom;
    int32_t oldInches = mAppUnitsPerPhysicalInch;

    SetDPI(aScale);

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

DesktopToLayoutDeviceScale
nsDeviceContext::GetDesktopToDeviceScale()
{
    nsCOMPtr<nsIScreen> screen;
    FindScreen(getter_AddRefs(screen));

    if (screen) {
        double scale;
        screen->GetContentsScaleFactor(&scale);
        return DesktopToLayoutDeviceScale(scale);
    }

    return DesktopToLayoutDeviceScale(1.0);
}

bool
nsDeviceContext::IsSyncPagePrinting() const
{
  MOZ_ASSERT(mPrintTarget);
  return mPrintTarget->IsSyncPagePrinting();
}

void
nsDeviceContext::RegisterPageDoneCallback(PrintTarget::PageDoneCallback&& aCallback)
{
  MOZ_ASSERT(mPrintTarget && aCallback && !IsSyncPagePrinting());
  mPrintTarget->RegisterPageDoneCallback(Move(aCallback));
}
void
nsDeviceContext::UnregisterPageDoneCallback()
{
  if (mPrintTarget) {
    mPrintTarget->UnregisterPageDoneCallback();
  }
}
