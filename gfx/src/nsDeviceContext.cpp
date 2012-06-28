/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDeviceContext.h"
#include "nsCRT.h"
#include "nsFontMetrics.h"
#include "nsRenderingContext.h"
#include "nsIWidget.h"

#include "mozilla/Attributes.h"
#include "mozilla/Services.h"
#include "mozilla/Preferences.h"
#include "nsIServiceManager.h"
#include "nsILanguageAtomService.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"

#include "gfxImageSurface.h"

#if !XP_MACOSX
#include "gfxPDFSurface.h"
#endif

#ifdef MOZ_WIDGET_GTK
#include "gfxPSSurface.h"
#elif XP_WIN
#include "gfxWindowsSurface.h"
#elif defined(XP_OS2)
#include "gfxOS2Surface.h"
#elif XP_MACOSX
#include "gfxQuartzSurface.h"
#endif

using namespace mozilla;
using mozilla::services::GetObserverService;

class nsFontCache MOZ_FINAL : public nsIObserver
{
public:
    nsFontCache()   { MOZ_COUNT_CTOR(nsFontCache); }
    ~nsFontCache()  { MOZ_COUNT_DTOR(nsFontCache); }

    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER

    void Init(nsDeviceContext* aContext);
    void Destroy();

    nsresult GetMetricsFor(const nsFont& aFont, nsIAtom* aLanguage,
                           gfxUserFontSet* aUserFontSet,
                           nsFontMetrics*& aMetrics);

    void FontMetricsDeleted(const nsFontMetrics* aFontMetrics);
    void Compact();
    void Flush();

protected:
    nsDeviceContext*          mContext; // owner
    nsCOMPtr<nsIAtom>         mLocaleLanguage;
    nsTArray<nsFontMetrics*>  mFontMetrics;
};

NS_IMPL_ISUPPORTS1(nsFontCache, nsIObserver)

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
nsFontCache::Observe(nsISupports*, const char* aTopic, const PRUnichar*)
{
    if (!nsCRT::strcmp(aTopic, "memory-pressure"))
        Compact();
    return NS_OK;
}

nsresult
nsFontCache::GetMetricsFor(const nsFont& aFont, nsIAtom* aLanguage,
                           gfxUserFontSet* aUserFontSet,
                           nsFontMetrics*& aMetrics)
{
    if (!aLanguage)
        aLanguage = mLocaleLanguage;

    // First check our cache
    // start from the end, which is where we put the most-recent-used element

    nsFontMetrics* fm;
    PRInt32 n = mFontMetrics.Length() - 1;
    for (PRInt32 i = n; i >= 0; --i) {
        fm = mFontMetrics[i];
        if (fm->Font().Equals(aFont) && fm->GetUserFontSet() == aUserFontSet &&
            fm->Language() == aLanguage) {
            if (i != n) {
                // promote it to the end of the cache
                mFontMetrics.RemoveElementAt(i);
                mFontMetrics.AppendElement(fm);
            }
            fm->GetThebesFontGroup()->UpdateFontList();
            NS_ADDREF(aMetrics = fm);
            return NS_OK;
        }
    }

    // It's not in the cache. Get font metrics and then cache them.

    fm = new nsFontMetrics();
    NS_ADDREF(fm);
    nsresult rv = fm->Init(aFont, aLanguage, mContext, aUserFontSet);
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
    rv = fm->Init(aFont, aLanguage, mContext, aUserFontSet);
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
    for (PRInt32 i = mFontMetrics.Length()-1; i >= 0; --i) {
        nsFontMetrics* fm = mFontMetrics[i];
        nsFontMetrics* oldfm = fm;
        // Destroy() isn't here because we want our device context to be
        // notified
        NS_RELEASE(fm); // this will reset fm to nsnull
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
    for (PRInt32 i = mFontMetrics.Length()-1; i >= 0; --i) {
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
      mAppUnitsPerDevPixel(-1), mAppUnitsPerDevNotScaledPixel(-1),
      mAppUnitsPerPhysicalInch(-1),
      mPixelScale(1.0f), mPrintingScale(1.0f),
      mFontCache(nsnull)
{
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
                               gfxUserFontSet* aUserFontSet,
                               nsFontMetrics*& aMetrics)
{
    if (!mFontCache) {
        mFontCache = new nsFontCache();
        NS_ADDREF(mFontCache);
        mFontCache->Init(this);
    }

    return mFontCache->GetMetricsFor(aFont, aLanguage, aUserFontSet, aMetrics);
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
    return(mPrintingSurface != NULL);
}

void
nsDeviceContext::SetDPI()
{
    float dpi = -1.0f;

    // PostScript, PDF and Mac (when printing) all use 72 dpi
    // Use a printing DC to determine the other dpi values
    if (mPrintingSurface) {
        switch (mPrintingSurface->GetType()) {
        case gfxASurface::SurfaceTypePDF:
        case gfxASurface::SurfaceTypePS:
        case gfxASurface::SurfaceTypeQuartz:
            dpi = 72.0f;
            break;
#ifdef XP_WIN
        case gfxASurface::SurfaceTypeWin32:
        case gfxASurface::SurfaceTypeWin32Printing: {
            HDC dc = reinterpret_cast<gfxWindowsSurface*>(mPrintingSurface.get())->GetDC();
            PRInt32 OSVal = GetDeviceCaps(dc, LOGPIXELSY);
            dpi = 144.0f;
            mPrintingScale = float(OSVal) / dpi;
            break;
        }
#endif
#ifdef XP_OS2
        case gfxASurface::SurfaceTypeOS2: {
            LONG lDPI;
            HDC dc = GpiQueryDevice(reinterpret_cast<gfxOS2Surface*>(mPrintingSurface.get())->GetPS());
            if (DevQueryCaps(dc, CAPS_VERTICAL_FONT_RES, 1, &lDPI))
                dpi = lDPI;
            break;
        }
#endif
        default:
            NS_NOTREACHED("Unexpected printing surface type");
            break;
        }

        mAppUnitsPerDevNotScaledPixel =
            NS_lround((AppUnitsPerCSSPixel() * 96) / dpi);
    } else {
        // A value of -1 means use the maximum of 96 and the system DPI.
        // A value of 0 means use the system DPI. A positive value is used as the DPI.
        // This sets the physical size of a device pixel and thus controls the
        // interpretation of physical units.
        PRInt32 prefDPI = Preferences::GetInt("layout.css.dpi", -1);

        if (prefDPI > 0) {
            dpi = prefDPI;
        } else if (mWidget) {
            dpi = mWidget->GetDPI();

            if (prefDPI < 0) {
                dpi = NS_MAX(96.0f, dpi);
            }
        } else {
            dpi = 96.0f;
        }

        // The number of device pixels per CSS pixel. A value <= 0 means choose
        // automatically based on the DPI. A positive value is used as-is. This effectively
        // controls the size of a CSS "px".
        float devPixelsPerCSSPixel = -1.0;

        nsAdoptingCString prefString = Preferences::GetCString("layout.css.devPixelsPerPx");
        if (!prefString.IsEmpty()) {
            devPixelsPerCSSPixel = static_cast<float>(atof(prefString));
        }

        if (devPixelsPerCSSPixel <= 0) {
            if (mWidget) {
                devPixelsPerCSSPixel = mWidget->GetDefaultScale();
            } else {
                devPixelsPerCSSPixel = 1.0;
            }
        }

        mAppUnitsPerDevNotScaledPixel =
            NS_MAX(1, NS_lround(AppUnitsPerCSSPixel() / devPixelsPerCSSPixel));
    }

    NS_ASSERTION(dpi != -1.0, "no dpi set");

    mAppUnitsPerPhysicalInch = NS_lround(dpi * mAppUnitsPerDevNotScaledPixel);
    UpdateScaledAppUnits();
}

nsresult
nsDeviceContext::Init(nsIWidget *aWidget)
{
    if (mScreenManager && mWidget == aWidget)
        return NS_OK;

    mWidget = aWidget;
    SetDPI();

    if (mScreenManager)
        return NS_OK;

    mScreenManager = do_GetService("@mozilla.org/gfx/screenmanager;1");

    return NS_OK;
}

nsresult
nsDeviceContext::CreateRenderingContext(nsRenderingContext *&aContext)
{
    NS_ABORT_IF_FALSE(mPrintingSurface, "only call for printing dcs");

    nsRefPtr<nsRenderingContext> pContext = new nsRenderingContext();

    pContext->Init(this, mPrintingSurface);
    pContext->Scale(mPrintingScale, mPrintingScale);
    aContext = pContext;
    NS_ADDREF(aContext);

    return NS_OK;
}

nsresult
nsDeviceContext::GetDepth(PRUint32& aDepth)
{
    if (mDepth == 0) {
        nsCOMPtr<nsIScreen> primaryScreen;
        mScreenManager->GetPrimaryScreen(getter_AddRefs(primaryScreen));
        primaryScreen->GetColorDepth(reinterpret_cast<PRInt32 *>(&mDepth));
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

    Init(nsnull);

    CalcPrintingSize();

    return NS_OK;
}

nsresult
nsDeviceContext::BeginDocument(PRUnichar*  aTitle,
                               PRUnichar*  aPrintToFileName,
                               PRInt32     aStartPage,
                               PRInt32     aEndPage)
{
    static const PRUnichar kEmpty[] = { '\0' };
    nsresult rv;

    rv = mPrintingSurface->BeginPrinting(nsDependentString(aTitle ? aTitle : kEmpty),
                                         nsDependentString(aPrintToFileName ? aPrintToFileName : kEmpty));

    if (NS_SUCCEEDED(rv) && mDeviceContextSpec)
        rv = mDeviceContextSpec->BeginDocument(aTitle, aPrintToFileName, aStartPage, aEndPage);

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
    // And we don't null it out in EndPage because mPrintingSurface needs
    // to be available also in-between EndPage/BeginPage (bug 665218).
    mDeviceContextSpec->GetSurfaceForPrinter(getter_AddRefs(mPrintingSurface));
#endif

    rv = mPrintingSurface->BeginPage();

    return rv;
}

nsresult
nsDeviceContext::EndPage(void)
{
    nsresult rv = mPrintingSurface->EndPage();

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
        PRInt32 x, y, width, height;
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
        PRInt32 x, y, width, height;
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
    if (mWidget && mWidget->GetNativeData(NS_NATIVE_WINDOW))
        mScreenManager->ScreenForNativeWidget(mWidget->GetNativeData(NS_NATIVE_WINDOW),
                                              outScreen);
    else
        mScreenManager->GetPrimaryScreen(outScreen);
}

void
nsDeviceContext::CalcPrintingSize()
{
    if (!mPrintingSurface)
        return;

    bool inPoints = true;

    gfxSize size(0, 0);
    switch (mPrintingSurface->GetType()) {
    case gfxASurface::SurfaceTypeImage:
        inPoints = false;
        size = reinterpret_cast<gfxImageSurface*>(mPrintingSurface.get())->GetSize();
        break;

#if defined(MOZ_PDF_PRINTING)
    case gfxASurface::SurfaceTypePDF:
        inPoints = true;
        size = reinterpret_cast<gfxPDFSurface*>(mPrintingSurface.get())->GetSize();
        break;
#endif

#ifdef MOZ_WIDGET_GTK
    case gfxASurface::SurfaceTypePS:
        inPoints = true;
        size = reinterpret_cast<gfxPSSurface*>(mPrintingSurface.get())->GetSize();
        break;
#endif

#ifdef XP_MACOSX
    case gfxASurface::SurfaceTypeQuartz:
        inPoints = true; // this is really only true when we're printing
        size = reinterpret_cast<gfxQuartzSurface*>(mPrintingSurface.get())->GetSize();
        break;
#endif

#ifdef XP_WIN
    case gfxASurface::SurfaceTypeWin32:
    case gfxASurface::SurfaceTypeWin32Printing:
        {
            inPoints = false;
            HDC dc = reinterpret_cast<gfxWindowsSurface*>(mPrintingSurface.get())->GetDC();
            if (!dc)
                dc = GetDC((HWND)mWidget->GetNativeData(NS_NATIVE_WIDGET));
            size.width = NSFloatPixelsToAppUnits(::GetDeviceCaps(dc, HORZRES)/mPrintingScale, AppUnitsPerDevPixel());
            size.height = NSFloatPixelsToAppUnits(::GetDeviceCaps(dc, VERTRES)/mPrintingScale, AppUnitsPerDevPixel());
            mDepth = (PRUint32)::GetDeviceCaps(dc, BITSPIXEL);
            if (dc != reinterpret_cast<gfxWindowsSurface*>(mPrintingSurface.get())->GetDC())
                ReleaseDC((HWND)mWidget->GetNativeData(NS_NATIVE_WIDGET), dc);
            break;
        }
#endif

#ifdef XP_OS2
    case gfxASurface::SurfaceTypeOS2:
        {
            inPoints = false;
            // we already set the size in the surface constructor we set for
            // printing, so just get those values here
            size = reinterpret_cast<gfxOS2Surface*>(mPrintingSurface.get())->GetSize();
            // as they are in pixels we need to scale them to app units
            size.width = NSFloatPixelsToAppUnits(size.width, AppUnitsPerDevPixel());
            size.height = NSFloatPixelsToAppUnits(size.height, AppUnitsPerDevPixel());
            // still need to get the depth from the device context
            HDC dc = GpiQueryDevice(reinterpret_cast<gfxOS2Surface*>(mPrintingSurface.get())->GetPS());
            LONG value;
            if (DevQueryCaps(dc, CAPS_COLOR_BITCOUNT, 1, &value))
                mDepth = value;
            else
                mDepth = 8; // default to 8bpp, should be enough for printers
            break;
        }
#endif
    default:
        NS_ERROR("trying to print to unknown surface type");
    }

    if (inPoints) {
        // For printing, CSS inches and physical inches are identical
        // so it doesn't matter which we use here
        mWidth = NSToCoordRound(float(size.width) * AppUnitsPerPhysicalInch() / 72);
        mHeight = NSToCoordRound(float(size.height) * AppUnitsPerPhysicalInch() / 72);
    } else {
        mWidth = NSToIntRound(size.width);
        mHeight = NSToIntRound(size.height);
    }
}

bool nsDeviceContext::CheckDPIChange() {
    PRInt32 oldDevPixels = mAppUnitsPerDevNotScaledPixel;
    PRInt32 oldInches = mAppUnitsPerPhysicalInch;

    SetDPI();

    return oldDevPixels != mAppUnitsPerDevNotScaledPixel ||
        oldInches != mAppUnitsPerPhysicalInch;
}

bool
nsDeviceContext::SetPixelScale(float aScale)
{
    if (aScale <= 0) {
        NS_NOTREACHED("Invalid pixel scale value");
        return false;
    }
    PRUint32 oldAppUnitsPerDevPixel = mAppUnitsPerDevPixel;
    mPixelScale = aScale;
    UpdateScaledAppUnits();
    return oldAppUnitsPerDevPixel != mAppUnitsPerDevPixel;
}

void
nsDeviceContext::UpdateScaledAppUnits()
{
    mAppUnitsPerDevPixel =
        NS_MAX(1, NSToIntRound(float(mAppUnitsPerDevNotScaledPixel) / mPixelScale));
}
