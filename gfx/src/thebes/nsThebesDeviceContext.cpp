/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is thebes gfx
 *
 * The Initial Developer of the Original Code is
 * mozilla.org.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com>
 *   Stuart Parmenter <pavlov@pavlov.net>
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

#include "nsFont.h"
#include "nsGfxCIID.h"
#include "nsIFontMetrics.h"
#include "nsHashtable.h"
#include "nsILanguageAtomService.h"
#include "nsUnicharUtils.h"

#include "nsIServiceManager.h"
#include "nsIPrefService.h"
#include "nsCRT.h"
#include "mozilla/Services.h"

#include "nsThebesDeviceContext.h"
#include "nsThebesRenderingContext.h"
#include "gfxUserFontSet.h"
#include "gfxPlatform.h"

#include "nsIWidget.h"
#include "nsIView.h"
#include "nsILookAndFeel.h"

#include "gfxImageSurface.h"

#ifdef MOZ_ENABLE_GTK2
#include "nsSystemFontsGTK2.h"
#include "gfxPDFSurface.h"
#include "gfxPSSurface.h"
static nsSystemFontsGTK2 *gSystemFonts = nsnull;
#elif XP_WIN
#include "nsSystemFontsWin.h"
#include "gfxWindowsSurface.h"
#include "gfxPDFSurface.h"
static nsSystemFontsWin *gSystemFonts = nsnull;
#ifndef WINCE
#include <usp10.h>
#endif
#elif defined(XP_OS2)
#include "nsSystemFontsOS2.h"
#include "gfxPDFSurface.h"
static nsSystemFontsOS2 *gSystemFonts = nsnull;
#elif defined(XP_BEOS)
#include "nsSystemFontsBeOS.h"
static nsSystemFontsBeOS *gSystemFonts = nsnull;
#elif XP_MACOSX
#include "nsSystemFontsMac.h"
#include "gfxQuartzSurface.h"
#include "gfxImageSurface.h"
static nsSystemFontsMac *gSystemFonts = nsnull;
#elif defined(MOZ_WIDGET_QT)
#include "nsSystemFontsQt.h"
static nsSystemFontsQt *gSystemFonts = nsnull;
#elif defined(ANDROID)
#include "nsSystemFontsAndroid.h"
static nsSystemFontsAndroid *gSystemFonts = nsnull;
#else
#error Need to declare gSystemFonts!
#endif

#ifdef PR_LOGGING
PRLogModuleInfo* gThebesGFXLog = nsnull;
#endif

class nsFontCache
{
public:
    nsFontCache();
    ~nsFontCache();

    nsresult Init(nsIDeviceContext* aContext);
    nsresult GetMetricsFor(const nsFont& aFont, nsIAtom* aLanguage,
                           gfxUserFontSet* aUserFontSet,
                           nsIFontMetrics*& aMetrics);

    nsresult FontMetricsDeleted(const nsIFontMetrics* aFontMetrics);
    nsresult Compact();
    nsresult Flush();
    nsresult CreateFontMetricsInstance(nsIFontMetrics** fm);

protected:
    nsTArray<nsIFontMetrics*> mFontMetrics;
    nsIDeviceContext         *mContext; // we do not addref this since
                                        // ownership is implied. MMP.
};

nsFontCache::nsFontCache()
{
    MOZ_COUNT_CTOR(nsFontCache);
    mContext = nsnull;
}

nsFontCache::~nsFontCache()
{
    MOZ_COUNT_DTOR(nsFontCache);
    Flush();
}

nsresult
nsFontCache::Init(nsIDeviceContext* aContext)
{
    NS_PRECONDITION(nsnull != aContext, "null ptr");
    // Note: we don't hold a reference to the device context, because it
    // holds a reference to us and we don't want circular references
    mContext = aContext;
    return NS_OK;
}

nsresult
nsFontCache::GetMetricsFor(const nsFont& aFont, nsIAtom* aLanguage,
  gfxUserFontSet* aUserFontSet, nsIFontMetrics*& aMetrics)
{
    // First check our cache
    // start from the end, which is where we put the most-recent-used element

    nsIFontMetrics* fm;
    PRInt32 n = mFontMetrics.Length() - 1;
    for (PRInt32 i = n; i >= 0; --i) {
        fm = mFontMetrics[i];
        nsIThebesFontMetrics* tfm = static_cast<nsIThebesFontMetrics*>(fm);
        if (fm->Font().Equals(aFont) && tfm->GetUserFontSet() == aUserFontSet) {
            nsCOMPtr<nsIAtom> language;
            fm->GetLanguage(getter_AddRefs(language));
            if (aLanguage == language.get()) {
                if (i != n) {
                    // promote it to the end of the cache
                    mFontMetrics.RemoveElementAt(i);
                    mFontMetrics.AppendElement(fm);
                }
                tfm->GetThebesFontGroup()->UpdateFontList();
                NS_ADDREF(aMetrics = fm);
                return NS_OK;
            }
        }
    }

    // It's not in the cache. Get font metrics and then cache them.

    aMetrics = nsnull;
    nsresult rv = CreateFontMetricsInstance(&fm);
    if (NS_FAILED(rv)) return rv;
    rv = fm->Init(aFont, aLanguage, mContext, aUserFontSet);
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
    rv = CreateFontMetricsInstance(&fm);
    if (NS_FAILED(rv)) return rv;
    rv = fm->Init(aFont, aLanguage, mContext, aUserFontSet);
    if (NS_SUCCEEDED(rv)) {
        mFontMetrics.AppendElement(fm);
        aMetrics = fm;
        NS_ADDREF(aMetrics);
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

    NS_POSTCONDITION(NS_SUCCEEDED(rv), "font metrics should not be null - bug 136248");
    return rv;
}

nsresult
nsFontCache::CreateFontMetricsInstance(nsIFontMetrics** fm)
{
    static NS_DEFINE_CID(kFontMetricsCID, NS_FONT_METRICS_CID);
    return CallCreateInstance(kFontMetricsCID, fm);
}

nsresult nsFontCache::FontMetricsDeleted(const nsIFontMetrics* aFontMetrics)
{
    mFontMetrics.RemoveElement(aFontMetrics);
    return NS_OK;
}

nsresult nsFontCache::Compact()
{
    // Need to loop backward because the running element can be removed on
    // the way
    for (PRInt32 i = mFontMetrics.Length()-1; i >= 0; --i) {
        nsIFontMetrics* fm = mFontMetrics[i];
        nsIFontMetrics* oldfm = fm;
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
    return NS_OK;
}

nsresult nsFontCache::Flush()
{
    for (PRInt32 i = mFontMetrics.Length()-1; i >= 0; --i) {
        nsIFontMetrics* fm = mFontMetrics[i];
        // Destroy() will unhook our device context from the fm so that we
        // won't waste time in triggering the notification of
        // FontMetricsDeleted() in the subsequent release
        fm->Destroy();
        NS_RELEASE(fm);
    }

    mFontMetrics.Clear();

    return NS_OK;
}

NS_IMPL_ISUPPORTS3(nsThebesDeviceContext, nsIDeviceContext, nsIObserver, nsISupportsWeakReference)

nsThebesDeviceContext::nsThebesDeviceContext()
{
#ifdef PR_LOGGING
    if (!gThebesGFXLog)
        gThebesGFXLog = PR_NewLogModule("thebesGfx");
#endif

    PR_LOG(gThebesGFXLog, PR_LOG_DEBUG, ("#### Creating DeviceContext %p\n", this));

    mAppUnitsPerDevPixel = nscoord(-1);
    mAppUnitsPerPhysicalInch = nscoord(-1);
    mAppUnitsPerDevNotScaledPixel = nscoord(-1);
    mPixelScale = 1.0f;

    mFontCache = nsnull;
    mWidget = nsnull;
    mFontAliasTable = nsnull;

    mDepth = 0;
    mWidth = 0;
    mHeight = 0;
    mPrintingScale = 1.0f;

#if defined(XP_WIN) && !defined(WINCE)
    SCRIPT_DIGITSUBSTITUTE sds;
    ScriptRecordDigitSubstitution(LOCALE_USER_DEFAULT, &sds);
#endif
}

static PRBool DeleteValue(nsHashKey* aKey, void* aValue, void* closure)
{
    delete ((nsString*)aValue);
    return PR_TRUE;
}

nsThebesDeviceContext::~nsThebesDeviceContext()
{
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (obs)
        obs->RemoveObserver(this, "memory-pressure");

    if (nsnull != mFontCache) {
        delete mFontCache;
        mFontCache = nsnull;
    }

    if (nsnull != mFontAliasTable) {
        mFontAliasTable->Enumerate(DeleteValue);
        delete mFontAliasTable;
    }
}

NS_IMETHODIMP
nsThebesDeviceContext::Observe(nsISupports* aSubject, const char* aTopic, const PRUnichar* aSomeData)
{
    if (mFontCache && !nsCRT::strcmp(aTopic, "memory-pressure")) {
        mFontCache->Compact();
    }
    return NS_OK;
}

NS_IMETHODIMP nsThebesDeviceContext::CreateFontCache()
{
    mFontCache = new nsFontCache();
    if (!mFontCache) {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    return mFontCache->Init(this);
}

NS_IMETHODIMP nsThebesDeviceContext::FontMetricsDeleted(const nsIFontMetrics* aFontMetrics)
{
    if (mFontCache) {
        mFontCache->FontMetricsDeleted(aFontMetrics);
    }
    return NS_OK;
}

void
nsThebesDeviceContext::GetLocaleLanguage(void)
{
    if (!mLocaleLanguage) {
        nsCOMPtr<nsILanguageAtomService> langService;
        langService = do_GetService(NS_LANGUAGEATOMSERVICE_CONTRACTID);
        if (langService) {
            mLocaleLanguage = langService->GetLocaleLanguage();
        }
        if (!mLocaleLanguage) {
            mLocaleLanguage = do_GetAtom("x-western");
        }
    }
}

NS_IMETHODIMP nsThebesDeviceContext::GetMetricsFor(const nsFont& aFont,
  nsIAtom* aLanguage, gfxUserFontSet* aUserFontSet, nsIFontMetrics*& aMetrics)
{
    if (nsnull == mFontCache) {
        nsresult rv = CreateFontCache();
        if (NS_FAILED(rv)) {
            aMetrics = nsnull;
            return rv;
        }
        // XXX temporary fix for performance problem -- erik
        GetLocaleLanguage();
    }

    // XXX figure out why aLanguage is NULL sometimes
    //      -> see nsPageFrame.cpp:511
    if (!aLanguage) {
        aLanguage = mLocaleLanguage;
    }

    return mFontCache->GetMetricsFor(aFont, aLanguage, aUserFontSet, aMetrics);
}

NS_IMETHODIMP nsThebesDeviceContext::GetMetricsFor(const nsFont& aFont,
                                                   gfxUserFontSet* aUserFontSet,
                                                   nsIFontMetrics*& aMetrics)
{
    if (nsnull == mFontCache) {
        nsresult rv = CreateFontCache();
        if (NS_FAILED(rv)) {
            aMetrics = nsnull;
            return rv;
        }
        // XXX temporary fix for performance problem -- erik
        GetLocaleLanguage();
    }
    return mFontCache->GetMetricsFor(aFont, mLocaleLanguage, aUserFontSet,
                                     aMetrics);
}

struct FontEnumData {
    FontEnumData(nsIDeviceContext* aDC, nsString& aFaceName)
        : mDC(aDC), mFaceName(aFaceName)
    {}
    nsIDeviceContext* mDC;
    nsString&         mFaceName;
};

static PRBool FontEnumCallback(const nsString& aFamily, PRBool aGeneric, void *aData)
{
    FontEnumData* data = (FontEnumData*)aData;
    // XXX for now, all generic fonts are presumed to exist
    //     we may want to actually check if there's an installed conversion
    if (aGeneric) {
        data->mFaceName = aFamily;
        return PR_FALSE; // found one, stop.
    }
    else {
        nsAutoString local;
        PRBool       aliased;
        data->mDC->GetLocalFontName(aFamily, local, aliased);
        if (aliased || (NS_SUCCEEDED(data->mDC->CheckFontExistence(local)))) {
            data->mFaceName = local;
            return PR_FALSE; // found one, stop.
        }
    }
    return PR_TRUE; // didn't exist, continue looking
}

NS_IMETHODIMP nsThebesDeviceContext::FirstExistingFont(const nsFont& aFont, nsString& aFaceName)
{
    FontEnumData data(this, aFaceName);
    if (aFont.EnumerateFamilies(FontEnumCallback, &data)) {
        return NS_ERROR_FAILURE; // ran out
    }
    return NS_OK;
}

class FontAliasKey: public nsHashKey
{
public:
    FontAliasKey(const nsString& aString)
    { mString.Assign(aString); }

    virtual PRUint32 HashCode(void) const;
    virtual PRBool Equals(const nsHashKey *aKey) const;
    virtual nsHashKey *Clone(void) const;

    nsString mString;
};

PRUint32 FontAliasKey::HashCode(void) const
{
    PRUint32 hash = 0;
    const PRUnichar* string = mString.get();
    PRUnichar ch;
    while ((ch = *string++) != 0) {
        // FYI: hash = hash*37 + ch
        ch = ToUpperCase(ch);
        hash = ((hash << 5) + (hash << 2) + hash) + ch;
    }
    return hash;
}

PRBool FontAliasKey::Equals(const nsHashKey *aKey) const
{
    return mString.Equals(((FontAliasKey*)aKey)->mString, nsCaseInsensitiveStringComparator());
}

nsHashKey* FontAliasKey::Clone(void) const
{
    return new FontAliasKey(mString);
}

nsresult nsThebesDeviceContext::CreateFontAliasTable()
{
    nsresult result = NS_OK;

    if (nsnull == mFontAliasTable) {
        mFontAliasTable = new nsHashtable();
        if (nsnull != mFontAliasTable) {

            nsAutoString times;         times.AssignLiteral("Times");
            nsAutoString timesNewRoman; timesNewRoman.AssignLiteral("Times New Roman");
            nsAutoString timesRoman;    timesRoman.AssignLiteral("Times Roman");
            nsAutoString arial;         arial.AssignLiteral("Arial");
            nsAutoString helvetica;     helvetica.AssignLiteral("Helvetica");
            nsAutoString courier;       courier.AssignLiteral("Courier");
            nsAutoString courierNew;    courierNew.AssignLiteral("Courier New");
            nsAutoString nullStr;

            AliasFont(times, timesNewRoman, timesRoman, PR_FALSE);
            AliasFont(timesRoman, timesNewRoman, times, PR_FALSE);
            AliasFont(timesNewRoman, timesRoman, times, PR_FALSE);
            AliasFont(arial, helvetica, nullStr, PR_FALSE);
            AliasFont(helvetica, arial, nullStr, PR_FALSE);
            AliasFont(courier, courierNew, nullStr, PR_TRUE);
            AliasFont(courierNew, courier, nullStr, PR_FALSE);
        }
        else {
            result = NS_ERROR_OUT_OF_MEMORY;
        }
    }
    return result;
}

nsresult nsThebesDeviceContext::AliasFont(const nsString& aFont,
                                          const nsString& aAlias,
                                          const nsString& aAltAlias,
                                          PRBool aForceAlias)
{
    nsresult result = NS_OK;

    if (nsnull != mFontAliasTable) {
        if (aForceAlias || NS_FAILED(CheckFontExistence(aFont))) {
            if (NS_SUCCEEDED(CheckFontExistence(aAlias))) {
                nsString* entry = new nsString(aAlias);
                if (nsnull != entry) {
                    FontAliasKey key(aFont);
                    mFontAliasTable->Put(&key, entry);
                }
                else {
                    result = NS_ERROR_OUT_OF_MEMORY;
                }
            }
            else if (!aAltAlias.IsEmpty() && NS_SUCCEEDED(CheckFontExistence(aAltAlias))) {
                nsString* entry = new nsString(aAltAlias);
                if (nsnull != entry) {
                    FontAliasKey key(aFont);
                    mFontAliasTable->Put(&key, entry);
                }
                else {
                    result = NS_ERROR_OUT_OF_MEMORY;
                }
            }
        }
    }
    else {
        result = NS_ERROR_FAILURE;
    }
    return result;
}

NS_IMETHODIMP nsThebesDeviceContext::GetLocalFontName(const nsString& aFaceName,
                                                      nsString& aLocalName,
                                                      PRBool& aAliased)
{
    nsresult result = NS_OK;

    if (nsnull == mFontAliasTable) {
        result = CreateFontAliasTable();
    }

    if (nsnull != mFontAliasTable) {
        FontAliasKey key(aFaceName);
        const nsString* alias = (const nsString*)mFontAliasTable->Get(&key);
        if (nsnull != alias) {
            aLocalName = *alias;
            aAliased = PR_TRUE;
        }
        else {
            aLocalName = aFaceName;
            aAliased = PR_FALSE;
        }
    }
    return result;
}

NS_IMETHODIMP nsThebesDeviceContext::FlushFontCache(void)
{
    if (nsnull != mFontCache)
        mFontCache->Flush();
    return NS_OK;
}

/* static */ void
nsThebesDeviceContext::Shutdown()
{
    delete gSystemFonts;
    gSystemFonts = nsnull;
}

PRBool
nsThebesDeviceContext::IsPrinterSurface()
{
  return(mPrintingSurface != NULL);
}

nsresult
nsThebesDeviceContext::SetDPI()
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
                PRInt32 OSVal = GetDeviceCaps(GetPrintHDC(), LOGPIXELSY);
                dpi = 144.0f;
                mPrintingScale = float(OSVal) / dpi;
                break;
            }
#endif
#ifdef XP_OS2
            case gfxASurface::SurfaceTypeOS2:
                LONG lDPI;
                if (DevQueryCaps(GetPrintHDC(), CAPS_VERTICAL_FONT_RES, 1, &lDPI))
                    dpi = lDPI;
                break;
#endif
            default:
                NS_NOTREACHED("Unexpected printing surface type");
                break;
        }

        mAppUnitsPerDevNotScaledPixel =
          NS_lround((AppUnitsPerCSSPixel() * 96) / dpi);
    } else {
        nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);

        // A value of -1 means use the maximum of 96 and the system DPI.
        // A value of 0 means use the system DPI. A positive value is used as the DPI.
        // This sets the physical size of a device pixel and thus controls the
        // interpretation of physical units.
        PRInt32 prefDPI = -1;
        if (prefs) {
            nsresult rv = prefs->GetIntPref("layout.css.dpi", &prefDPI);
            if (NS_FAILED(rv)) {
                prefDPI = -1;
            }
        }

        if (prefDPI > 0) {
            dpi = prefDPI;
        } else if (mWidget) {
            dpi = mWidget->GetDPI();

            if (prefDPI < 0) {
                dpi = PR_MAX(96.0f, dpi);
            }
        } else {
            dpi = 96.0f;
        }

        // The number of device pixels per CSS pixel. A value <= 0 means choose
        // automatically based on the DPI. A positive value is used as-is. This effectively
        // controls the size of a CSS "px".
        float devPixelsPerCSSPixel = -1.0;

        if (prefs) {
            nsXPIDLCString prefString;
            nsresult rv = prefs->GetCharPref("layout.css.devPixelsPerPx", getter_Copies(prefString));
            if (NS_SUCCEEDED(rv) && !prefString.IsEmpty()) {
                devPixelsPerCSSPixel = static_cast<float>(atof(prefString));
            }
        }

        if (devPixelsPerCSSPixel <= 0) {
            if (mWidget) {
                devPixelsPerCSSPixel = mWidget->GetDefaultScale();
            } else {
                devPixelsPerCSSPixel = 1.0;
            }
        }

        mAppUnitsPerDevNotScaledPixel =
            PR_MAX(1, NS_lround(AppUnitsPerCSSPixel() / devPixelsPerCSSPixel));
    }

    NS_ASSERTION(dpi != -1.0, "no dpi set");

    mAppUnitsPerPhysicalInch = NS_lround(dpi * mAppUnitsPerDevNotScaledPixel);
    UpdateScaledAppUnits();

    return NS_OK;
}

NS_IMETHODIMP
nsThebesDeviceContext::Init(nsIWidget *aWidget)
{
    if (mScreenManager && mWidget == aWidget)
        return NS_OK;

    mWidget = aWidget;
    SetDPI();

    if (mScreenManager)
        return NS_OK;

    // register as a memory-pressure observer to free font resources
    // in low-memory situations.
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (obs)
        obs->AddObserver(this, "memory-pressure", PR_TRUE);

    mScreenManager = do_GetService("@mozilla.org/gfx/screenmanager;1");

    return NS_OK;
}

NS_IMETHODIMP
nsThebesDeviceContext::CreateRenderingContext(nsIView *aView,
                                              nsIRenderingContext *&aContext)
{
    // This is currently only called by the caret code
    NS_ENSURE_ARG_POINTER(aView);
    NS_PRECONDITION(aView->HasWidget(), "View has no widget!");

    nsCOMPtr<nsIWidget> widget;
    widget = aView->GetWidget();

    return CreateRenderingContext(widget, aContext);
}

NS_IMETHODIMP
nsThebesDeviceContext::CreateRenderingContext(nsIWidget *aWidget,
                                              nsIRenderingContext *&aContext)
{
    nsresult rv;

    aContext = nsnull;
    nsCOMPtr<nsIRenderingContext> pContext;
    rv = CreateRenderingContextInstance(*getter_AddRefs(pContext));
    if (NS_SUCCEEDED(rv)) {
        nsRefPtr<gfxASurface> surface(aWidget->GetThebesSurface());
        if (surface)
            rv = pContext->Init(this, surface);
        else
            rv = NS_ERROR_FAILURE;

        if (NS_SUCCEEDED(rv)) {
            aContext = pContext;
            NS_ADDREF(aContext);
        }
    }

    return rv;
}

NS_IMETHODIMP
nsThebesDeviceContext::CreateRenderingContext(nsIRenderingContext *&aContext)
{
    nsresult rv = NS_OK;

    aContext = nsnull;
    nsCOMPtr<nsIRenderingContext> pContext;
    rv = CreateRenderingContextInstance(*getter_AddRefs(pContext));
    if (NS_SUCCEEDED(rv)) {
        if (mPrintingSurface)
            rv = pContext->Init(this, mPrintingSurface);
        else
            rv = NS_ERROR_FAILURE;

        if (NS_SUCCEEDED(rv)) {
            pContext->Scale(mPrintingScale, mPrintingScale);
            aContext = pContext;
            NS_ADDREF(aContext);
        }
    }

    return rv;
}

NS_IMETHODIMP
nsThebesDeviceContext::CreateRenderingContextInstance(nsIRenderingContext *&aContext)
{
    nsCOMPtr<nsIRenderingContext> renderingContext = new nsThebesRenderingContext();
    if (!renderingContext)
        return NS_ERROR_OUT_OF_MEMORY;

    aContext = renderingContext;
    NS_ADDREF(aContext);

    return NS_OK;
}

NS_IMETHODIMP
nsThebesDeviceContext::ClearCachedSystemFonts()
{
    //clear our cache of stored system fonts
    if (gSystemFonts) {
        delete gSystemFonts;
        gSystemFonts = nsnull;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsThebesDeviceContext::GetSystemFont(nsSystemFontID aID, nsFont *aFont) const
{
    if (!gSystemFonts) {
#ifdef MOZ_ENABLE_GTK2
        gSystemFonts = new nsSystemFontsGTK2();
#elif XP_WIN
        gSystemFonts = new nsSystemFontsWin();
#elif XP_OS2
        gSystemFonts = new nsSystemFontsOS2();
#elif defined(XP_BEOS)
        gSystemFonts = new nsSystemFontsBeOS();
#elif XP_MACOSX
        gSystemFonts = new nsSystemFontsMac();
#elif defined(MOZ_WIDGET_QT)
        gSystemFonts = new nsSystemFontsQt();
#elif defined(ANDROID)
        gSystemFonts = new nsSystemFontsAndroid();
#else
#error Need to know how to create gSystemFonts, fix me!
#endif
    }

    nsString fontName;
    gfxFontStyle fontStyle;
    nsresult rv = gSystemFonts->GetSystemFont(aID, &fontName, &fontStyle);
    NS_ENSURE_SUCCESS(rv, rv);

    aFont->name = fontName;
    aFont->style = fontStyle.style;
    aFont->systemFont = fontStyle.systemFont;
    aFont->variant = NS_FONT_VARIANT_NORMAL;
    aFont->familyNameQuirks = fontStyle.familyNameQuirks;
    aFont->weight = fontStyle.weight;
    aFont->stretch = fontStyle.stretch;
    aFont->decorations = NS_FONT_DECORATION_NONE;
    aFont->size = NSFloatPixelsToAppUnits(fontStyle.size, UnscaledAppUnitsPerDevPixel());
    //aFont->langGroup = fontStyle.langGroup;
    aFont->sizeAdjust = fontStyle.sizeAdjust;

    return rv;
}

NS_IMETHODIMP
nsThebesDeviceContext::CheckFontExistence(const nsString& aFaceName)
{
    return NS_OK;
}

NS_IMETHODIMP
nsThebesDeviceContext::GetDepth(PRUint32& aDepth)
{
    if (mDepth == 0) {
        nsCOMPtr<nsIScreen> primaryScreen;
        mScreenManager->GetPrimaryScreen(getter_AddRefs(primaryScreen));
        primaryScreen->GetColorDepth(reinterpret_cast<PRInt32 *>(&mDepth));
    }

    aDepth = mDepth;
    return NS_OK;
}

NS_IMETHODIMP
nsThebesDeviceContext::GetDeviceSurfaceDimensions(nscoord &aWidth, nscoord &aHeight)
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

NS_IMETHODIMP
nsThebesDeviceContext::GetRect(nsRect &aRect)
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

NS_IMETHODIMP
nsThebesDeviceContext::GetClientRect(nsRect &aRect)
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

NS_IMETHODIMP
nsThebesDeviceContext::PrepareNativeWidget(nsIWidget* aWidget, void** aOut)
{
    *aOut = nsnull;
    return NS_OK;
}


/*
 * below methods are for printing
 */
NS_IMETHODIMP
nsThebesDeviceContext::InitForPrinting(nsIDeviceContextSpec *aDevice)
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


NS_IMETHODIMP
nsThebesDeviceContext::PrepareDocument(PRUnichar * aTitle,
                                       PRUnichar*  aPrintToFileName)
{
    return NS_OK;
}


NS_IMETHODIMP
nsThebesDeviceContext::BeginDocument(PRUnichar*  aTitle,
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


NS_IMETHODIMP
nsThebesDeviceContext::EndDocument(void)
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


NS_IMETHODIMP
nsThebesDeviceContext::AbortDocument(void)
{
    nsresult rv = mPrintingSurface->AbortPrinting();

    if (mDeviceContextSpec)
        mDeviceContextSpec->EndDocument();

    return rv;
}


NS_IMETHODIMP
nsThebesDeviceContext::BeginPage(void)
{
    nsresult rv = NS_OK;

    if (mDeviceContextSpec)
        rv = mDeviceContextSpec->BeginPage();

    if (NS_FAILED(rv)) return rv;

   /* We need to get a new surface for each page on the Mac */
#ifdef XP_MACOSX
    mDeviceContextSpec->GetSurfaceForPrinter(getter_AddRefs(mPrintingSurface));
#endif
    rv = mPrintingSurface->BeginPage();

    return rv;
}

NS_IMETHODIMP
nsThebesDeviceContext::EndPage(void)
{
    nsresult rv = mPrintingSurface->EndPage();

    /* We need to release the CGContextRef in the surface here, plus it's
       not something you would want anyway, as these CGContextRefs are only good
       for one page. */
#ifdef XP_MACOSX
    mPrintingSurface = nsnull;
#endif

    if (mDeviceContextSpec)
        mDeviceContextSpec->EndPage();

    return rv;
}

/** End printing methods **/

void
nsThebesDeviceContext::ComputeClientRectUsingScreen(nsRect* outRect)
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
nsThebesDeviceContext::ComputeFullAreaUsingScreen(nsRect* outRect)
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
nsThebesDeviceContext::FindScreen(nsIScreen** outScreen)
{
    if (mWidget && mWidget->GetNativeData(NS_NATIVE_WINDOW))
        mScreenManager->ScreenForNativeWidget(mWidget->GetNativeData(NS_NATIVE_WINDOW),
                                              outScreen);
    else
        mScreenManager->GetPrimaryScreen(outScreen);
}

void
nsThebesDeviceContext::CalcPrintingSize()
{
    if (!mPrintingSurface)
        return;

    PRBool inPoints = PR_TRUE;

    gfxSize size;
    switch (mPrintingSurface->GetType()) {
    case gfxASurface::SurfaceTypeImage:
        inPoints = PR_FALSE;
        size = reinterpret_cast<gfxImageSurface*>(mPrintingSurface.get())->GetSize();
        break;

#if defined(MOZ_ENABLE_GTK2) || defined(XP_WIN) || defined(XP_OS2)
    case gfxASurface::SurfaceTypePDF:
        inPoints = PR_TRUE;
        size = reinterpret_cast<gfxPDFSurface*>(mPrintingSurface.get())->GetSize();
        break;
#endif

#ifdef MOZ_ENABLE_GTK2
    case gfxASurface::SurfaceTypePS:
        inPoints = PR_TRUE;
        size = reinterpret_cast<gfxPSSurface*>(mPrintingSurface.get())->GetSize();
        break;
#endif

#ifdef XP_MACOSX
    case gfxASurface::SurfaceTypeQuartz:
        inPoints = PR_TRUE; // this is really only true when we're printing
        size = reinterpret_cast<gfxQuartzSurface*>(mPrintingSurface.get())->GetSize();
        break;
#endif

#ifdef XP_WIN
    case gfxASurface::SurfaceTypeWin32:
    case gfxASurface::SurfaceTypeWin32Printing:
    {
        inPoints = PR_FALSE;
        HDC dc =  GetPrintHDC();
        if (!dc)
            dc = GetDC((HWND)mWidget->GetNativeData(NS_NATIVE_WIDGET));
        size.width = NSFloatPixelsToAppUnits(::GetDeviceCaps(dc, HORZRES)/mPrintingScale, AppUnitsPerDevPixel());
        size.height = NSFloatPixelsToAppUnits(::GetDeviceCaps(dc, VERTRES)/mPrintingScale, AppUnitsPerDevPixel());
        mDepth = (PRUint32)::GetDeviceCaps(dc, BITSPIXEL);
        if (dc != (HDC)GetPrintHDC())
            ReleaseDC((HWND)mWidget->GetNativeData(NS_NATIVE_WIDGET), dc);
        break;
    }
#endif

#ifdef XP_OS2
    case gfxASurface::SurfaceTypeOS2:
    {
        inPoints = PR_FALSE;
        // we already set the size in the surface constructor we set for
        // printing, so just get those values here
        size = reinterpret_cast<gfxOS2Surface*>(mPrintingSurface.get())->GetSize();
        // as they are in pixels we need to scale them to app units
        size.width = NSFloatPixelsToAppUnits(size.width, AppUnitsPerDevPixel());
        size.height = NSFloatPixelsToAppUnits(size.height, AppUnitsPerDevPixel());
        // still need to get the depth from the device context
        HDC dc = GetPrintHDC();
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

PRBool nsThebesDeviceContext::CheckDPIChange() {
    PRInt32 oldDevPixels = mAppUnitsPerDevNotScaledPixel;
    PRInt32 oldInches = mAppUnitsPerPhysicalInch;

    SetDPI();

    return oldDevPixels != mAppUnitsPerDevNotScaledPixel ||
           oldInches != mAppUnitsPerPhysicalInch;
}

PRBool
nsThebesDeviceContext::SetPixelScale(float aScale)
{
    if (aScale <= 0) {
        NS_NOTREACHED("Invalid pixel scale value");
        return PR_FALSE;
    }
    PRInt32 oldAppUnitsPerDevPixel = mAppUnitsPerDevPixel;
    mPixelScale = aScale;
    UpdateScaledAppUnits();
    return oldAppUnitsPerDevPixel != mAppUnitsPerDevPixel;
}

void
nsThebesDeviceContext::UpdateScaledAppUnits()
{
    mAppUnitsPerDevPixel =
        PR_MAX(1, NSToIntRound(float(mAppUnitsPerDevNotScaledPixel) / mPixelScale));
}

#if defined(XP_WIN) || defined(XP_OS2)
HDC
nsThebesDeviceContext::GetPrintHDC()
{
    if (mPrintingSurface) {
        switch (mPrintingSurface->GetType()) {
#ifdef XP_WIN
            case gfxASurface::SurfaceTypeWin32:
            case gfxASurface::SurfaceTypeWin32Printing:
                return reinterpret_cast<gfxWindowsSurface*>(mPrintingSurface.get())->GetDC();
#endif

#ifdef XP_OS2
            case gfxASurface::SurfaceTypeOS2:
                return GpiQueryDevice(reinterpret_cast<gfxOS2Surface*>(mPrintingSurface.get())->GetPS());
#endif

            default:
                NS_ERROR("invalid surface type in GetPrintHDC");
                break;
        }
    }

    return nsnull;
}
#endif
