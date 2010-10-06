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
 *   Stuart Parmenter <stuart@mozilla.com>
 *   Vladimir Vukicevic <vladimir@pobox.com>
 *   Masayuki Nakano <masayuki@d-toybox.com>
 *   Masatoshi Kimura <VYV03354@nifty.ne.jp>
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

#include "gfxWindowsPlatform.h"

#include "gfxImageSurface.h"
#include "gfxWindowsSurface.h"
#include "gfxTextRunWordCache.h"

#include "nsUnicharUtils.h"

#include "nsIPrefService.h"
#include "nsIPrefBranch2.h"
#include "nsServiceManagerUtils.h"
#include "nsTArray.h"

#include "nsIWindowsRegKey.h"
#include "nsILocalFile.h"
#include "plbase64.h"
#include "nsIXULRuntime.h"

#include "nsIGfxInfo.h"

#ifdef MOZ_FT2_FONTS
#include "ft2build.h"
#include FT_FREETYPE_H
#include "gfxFT2Fonts.h"
#include "gfxFT2FontList.h"
#include "cairo-ft.h"
#include "nsAppDirectoryServiceDefs.h"
#else
#include "gfxGDIFontList.h"
#include "gfxGDIFont.h"
#ifdef CAIRO_HAS_DWRITE_FONT
#include "gfxDWriteFontList.h"
#include "gfxDWriteFonts.h"
#include "gfxDWriteCommon.h"
#include <dwrite.h>
#endif
#endif

#ifdef CAIRO_HAS_D2D_SURFACE
#include "gfxD2DSurface.h"

#include <d3d10_1.h>

#include "nsIMemoryReporter.h"
#include "nsMemory.h"

class D2DCacheReporter :
    public nsIMemoryReporter
{
public:
    D2DCacheReporter()
    { }

    NS_DECL_ISUPPORTS

    NS_IMETHOD GetPath(char **memoryPath) {
        *memoryPath = strdup("gfx/d2d/surfacecache");
        return NS_OK;
    }

    NS_IMETHOD GetDescription(char **desc) {
        *desc = strdup("Memory used by Direct2D internal surface cache.");
        return NS_OK;
    }

    NS_IMETHOD GetMemoryUsed(PRInt64 *memoryUsed) {
        *memoryUsed = cairo_d2d_get_image_surface_cache_usage();
        return NS_OK;
    }
}; 

NS_IMPL_ISUPPORTS1(D2DCacheReporter, nsIMemoryReporter)

class D2DVRAMReporter :
    public nsIMemoryReporter
{
public:
    D2DVRAMReporter()
    { }

    NS_DECL_ISUPPORTS

    NS_IMETHOD GetPath(char **memoryPath) {
        *memoryPath = strdup("gfx/d2d/surfacevram");
        return NS_OK;
    }

    NS_IMETHOD GetDescription(char **desc) {
        *desc = strdup("Video memory used by D2D surfaces");
        return NS_OK;
    }

    NS_IMETHOD GetMemoryUsed(PRInt64 *memoryUsed) {
        cairo_device_t *device =
            gfxWindowsPlatform::GetPlatform()->GetD2DDevice();
        if (device) {
            *memoryUsed = cairo_d2d_get_surface_vram_usage(device);
        } else {
            *memoryUsed = 0;
        }
        return NS_OK;
    }
};

NS_IMPL_ISUPPORTS1(D2DVRAMReporter, nsIMemoryReporter)
#endif

#ifdef WINCE
#include <shlwapi.h>

#ifdef CAIRO_HAS_DDRAW_SURFACE
#include "gfxDDrawSurface.h"
#endif
#endif

#include "gfxUserFontSet.h"

#include <string>

#define GFX_USE_CLEARTYPE_ALWAYS "gfx.font_rendering.cleartype.always_use_for_content"
#define GFX_DOWNLOADABLE_FONTS_USE_CLEARTYPE "gfx.font_rendering.cleartype.use_for_downloadable_fonts"

#ifdef MOZ_FT2_FONTS
static FT_Library gPlatformFTLibrary = NULL;
#endif

#ifdef CAIRO_HAS_DWRITE_FONT
// DirectWrite is not available on all platforms, we need to use the function
// pointer.
typedef HRESULT (WINAPI*DWriteCreateFactoryFunc)(
  __in   DWRITE_FACTORY_TYPE factoryType,
  __in   REFIID iid,
  __out  IUnknown **factory
);
#endif

#ifdef CAIRO_HAS_D2D_SURFACE
typedef HRESULT (WINAPI*D3D10CreateDevice1Func)(
  IDXGIAdapter *pAdapter,
  D3D10_DRIVER_TYPE DriverType,
  HMODULE Software,
  UINT Flags,
  D3D10_FEATURE_LEVEL1 HardwareLevel,
  UINT SDKVersion,
  ID3D10Device1 **ppDevice
);
#endif

static __inline void
BuildKeyNameFromFontName(nsAString &aName)
{
    if (aName.Length() >= LF_FACESIZE)
        aName.Truncate(LF_FACESIZE - 1);
    ToLowerCase(aName);
}

gfxWindowsPlatform::gfxWindowsPlatform()
{
    mPrefFonts.Init(50);

    mUseClearTypeForDownloadableFonts = UNINITIALIZED_VALUE;
    mUseClearTypeAlways = UNINITIALIZED_VALUE;

    /* 
     * Initialize COM 
     */ 
    CoInitialize(NULL); 

    mScreenDC = GetDC(NULL);

#ifdef MOZ_FT2_FONTS
    FT_Init_FreeType(&gPlatformFTLibrary);
#endif

#ifdef CAIRO_HAS_D2D_SURFACE
    NS_RegisterMemoryReporter(new D2DCacheReporter());
    NS_RegisterMemoryReporter(new D2DVRAMReporter());
    mD2DDevice = nsnull;
#endif

    UpdateRenderMode();
}

gfxWindowsPlatform::~gfxWindowsPlatform()
{
    ::ReleaseDC(NULL, mScreenDC);
    // not calling FT_Done_FreeType because cairo may still hold references to
    // these FT_Faces.  See bug 458169.
#ifdef CAIRO_HAS_D2D_SURFACE
    if (mD2DDevice) {
        cairo_release_device(mD2DDevice);
    }
#endif

    /* 
     * Uninitialize COM 
     */ 
    CoUninitialize(); 

}

void
gfxWindowsPlatform::UpdateRenderMode()
{
/* Pick the default render mode differently between
 * desktop, Windows Mobile, and Windows CE.
 */
#if defined(WINCE_WINDOWS_MOBILE)
    mRenderMode = RENDER_IMAGE_DDRAW16;
#elif defined(WINCE)
    mRenderMode = RENDER_DDRAW_GL;
#else
    mRenderMode = RENDER_GDI;
#endif

    OSVERSIONINFOA versionInfo;
    versionInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFOA);
    ::GetVersionExA(&versionInfo);
    bool isVistaOrHigher = versionInfo.dwMajorVersion >= 6;

    PRBool safeMode = PR_FALSE;
    nsCOMPtr<nsIXULRuntime> xr = do_GetService("@mozilla.org/xre/runtime;1");
    if (xr)
      xr->GetInSafeMode(&safeMode);

    nsCOMPtr<nsIPrefBranch2> pref = do_GetService(NS_PREFSERVICE_CONTRACTID);
    nsresult rv;

    PRBool preferDirectWrite = PR_FALSE;

    rv = pref->GetBoolPref(
        "gfx.font_rendering.directwrite.enabled", &preferDirectWrite);
    if (NS_FAILED(rv)) {
        preferDirectWrite = PR_FALSE;
    }

    mUseDirectWrite = preferDirectWrite;

#ifdef CAIRO_HAS_D2D_SURFACE
    PRBool d2dDisabled = PR_FALSE;
    PRBool d2dForceEnabled = PR_FALSE;
    PRBool d2dBlocked = PR_FALSE;

    nsCOMPtr<nsIGfxInfo> gfxInfo = do_GetService("@mozilla.org/gfx/info;1");
    if (gfxInfo) {
        PRInt32 status;
        if (NS_SUCCEEDED(gfxInfo->GetFeatureStatus(nsIGfxInfo::FEATURE_DIRECT2D, &status))) {
            if (status != nsIGfxInfo::FEATURE_NO_INFO) {
                d2dDisabled = PR_TRUE;
                if (status == nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION ||
                    status == nsIGfxInfo::FEATURE_BLOCKED_DEVICE)
                {
                    d2dBlocked = PR_TRUE;
                }
            }
        }
    }

    rv = pref->GetBoolPref("gfx.direct2d.disabled", &d2dDisabled);
    if (NS_FAILED(rv))
        d2dDisabled = PR_FALSE;
    rv = pref->GetBoolPref("gfx.direct2d.force-enabled", &d2dForceEnabled);
    if (NS_FAILED(rv))
        d2dDisabled = PR_FALSE;

    bool tryD2D = !d2dBlocked || d2dForceEnabled;
    
    // Do not ever try if d2d is explicitly disabled.
    if (d2dDisabled) {
        tryD2D = false;
    }

    if (isVistaOrHigher  && !safeMode && tryD2D) {
        VerifyD2DDevice(d2dForceEnabled);
        if (mD2DDevice) {
            mRenderMode = RENDER_DIRECT2D;
            mUseDirectWrite = PR_TRUE;
        }
    } else {
        mD2DDevice = nsnull;
    }
#endif

#ifdef CAIRO_HAS_DWRITE_FONT
    // Enable when it's preffed on -and- we're using Vista or higher. Or when
    // we're going to use D2D.
    if (!mDWriteFactory && (mUseDirectWrite && isVistaOrHigher)) {
        DWriteCreateFactoryFunc createDWriteFactory = (DWriteCreateFactoryFunc)
            GetProcAddress(LoadLibraryW(L"dwrite.dll"), "DWriteCreateFactory");

        if (createDWriteFactory) {
            /**
             * I need a direct pointer to be able to cast to IUnknown**, I also
             * need to remember to release this because the nsRefPtr will
             * AddRef it.
             */
            IDWriteFactory *factory;
            HRESULT hr = createDWriteFactory(
                DWRITE_FACTORY_TYPE_SHARED,
                __uuidof(IDWriteFactory),
                reinterpret_cast<IUnknown**>(&factory));
            mDWriteFactory = factory;
            factory->Release();
        }
    }
#endif
}

void
gfxWindowsPlatform::VerifyD2DDevice(PRBool aAttemptForce)
{
#ifdef CAIRO_HAS_D2D_SURFACE
    if (mD2DDevice) {
        ID3D10Device1 *device = cairo_d2d_device_get_device(mD2DDevice);

        if (SUCCEEDED(device->GetDeviceRemovedReason())) {
            return;
        }
        mD2DDevice = nsnull;
    }

    HMODULE d3d10module = LoadLibraryA("d3d10_1.dll");
    D3D10CreateDevice1Func createD3DDevice = (D3D10CreateDevice1Func)
        GetProcAddress(d3d10module, "D3D10CreateDevice1");
    nsRefPtr<ID3D10Device1> device;

    if (createD3DDevice) {
        // We try 10.0 first even though we prefer 10.1, since we want to
        // fail as fast as possible if 10.x isn't supported.
        HRESULT hr = createD3DDevice(
            NULL, 
            D3D10_DRIVER_TYPE_HARDWARE,
            NULL,
            D3D10_CREATE_DEVICE_BGRA_SUPPORT |
            D3D10_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS,
            D3D10_FEATURE_LEVEL_10_0,
            D3D10_1_SDK_VERSION,
            getter_AddRefs(device));

        if (SUCCEEDED(hr)) {
            // We have 10.0, let's try 10.1.
            // XXX - This adds an additional 10-20ms for people who are
            // getting direct2d. We'd really like to do something more
            // clever.
            nsRefPtr<ID3D10Device1> device1;
            hr = createD3DDevice(
                NULL, 
                D3D10_DRIVER_TYPE_HARDWARE,
                NULL,
                D3D10_CREATE_DEVICE_BGRA_SUPPORT |
                D3D10_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS,
                D3D10_FEATURE_LEVEL_10_1,
                D3D10_1_SDK_VERSION,
                getter_AddRefs(device1));

            if (SUCCEEDED(hr)) {
                device = device1;
            }

            mD2DDevice = cairo_d2d_create_device_from_d3d10device(device);
        }
    }

    if (!mD2DDevice && aAttemptForce) {
        mD2DDevice = cairo_d2d_create_device();
    }
#endif
}
gfxPlatformFontList*
gfxWindowsPlatform::CreatePlatformFontList()
{
#ifdef MOZ_FT2_FONTS
    return new gfxFT2FontList();
#else
#ifdef CAIRO_HAS_DWRITE_FONT
    if (!GetDWriteFactory()) {
#endif
        return new gfxGDIFontList();
#ifdef CAIRO_HAS_DWRITE_FONT
    } else {
        return new gfxDWriteFontList();
    }
#endif
#endif
}

already_AddRefed<gfxASurface>
gfxWindowsPlatform::CreateOffscreenSurface(const gfxIntSize& size,
                                           gfxASurface::gfxContentType contentType)
{
    gfxASurface *surf = nsnull;

#ifdef CAIRO_HAS_DDRAW_SURFACE
    if (mRenderMode == RENDER_DDRAW || mRenderMode == RENDER_DDRAW_GL)
        surf = new gfxDDrawSurface(NULL, size, gfxASurface::FormatFromContent(contentType));
#endif

#ifdef CAIRO_HAS_WIN32_SURFACE
    if (mRenderMode == RENDER_GDI)
        surf = new gfxWindowsSurface(size, gfxASurface::FormatFromContent(contentType));
#endif

#ifdef CAIRO_HAS_D2D_SURFACE
    if (mRenderMode == RENDER_DIRECT2D)
        surf = new gfxD2DSurface(size, gfxASurface::FormatFromContent(contentType));
#endif

    if (surf == nsnull)
        surf = new gfxImageSurface(size, gfxASurface::FormatFromContent(contentType));

    NS_IF_ADDREF(surf);

    return surf;
}

nsresult
gfxWindowsPlatform::GetFontList(nsIAtom *aLangGroup,
                                const nsACString& aGenericFamily,
                                nsTArray<nsString>& aListOfFonts)
{
    gfxPlatformFontList::PlatformFontList()->GetFontList(aLangGroup, aGenericFamily, aListOfFonts);

    return NS_OK;
}

static void
RemoveCharsetFromFontSubstitute(nsAString &aName)
{
    PRInt32 comma = aName.FindChar(PRUnichar(','));
    if (comma >= 0)
        aName.Truncate(comma);
}

nsresult
gfxWindowsPlatform::UpdateFontList()
{
    gfxPlatformFontList::PlatformFontList()->UpdateFontList();

    return NS_OK;
}

struct ResolveData {
    ResolveData(gfxPlatform::FontResolverCallback aCallback,
                gfxWindowsPlatform *aCaller, const nsAString *aFontName,
                void *aClosure) :
        mFoundCount(0), mCallback(aCallback), mCaller(aCaller),
        mFontName(aFontName), mClosure(aClosure) {}
    PRUint32 mFoundCount;
    gfxPlatform::FontResolverCallback mCallback;
    gfxWindowsPlatform *mCaller;
    const nsAString *mFontName;
    void *mClosure;
};

nsresult
gfxWindowsPlatform::ResolveFontName(const nsAString& aFontName,
                                    FontResolverCallback aCallback,
                                    void *aClosure,
                                    PRBool& aAborted)
{
    nsAutoString resolvedName;
    if (!gfxPlatformFontList::PlatformFontList()->
             ResolveFontName(aFontName, resolvedName)) {
        aAborted = PR_FALSE;
        return NS_OK;
    }
    aAborted = !(*aCallback)(resolvedName, aClosure);
    return NS_OK;
}

nsresult
gfxWindowsPlatform::GetStandardFamilyName(const nsAString& aFontName, nsAString& aFamilyName)
{
    gfxPlatformFontList::PlatformFontList()->GetStandardFamilyName(aFontName, aFamilyName);
    return NS_OK;
}

gfxFontGroup *
gfxWindowsPlatform::CreateFontGroup(const nsAString &aFamilies,
                                    const gfxFontStyle *aStyle,
                                    gfxUserFontSet *aUserFontSet)
{
#ifdef MOZ_FT2_FONTS
    return new gfxFT2FontGroup(aFamilies, aStyle);
#else
    return new gfxFontGroup(aFamilies, aStyle, aUserFontSet);
#endif
}

gfxFontEntry* 
gfxWindowsPlatform::LookupLocalFont(const gfxProxyFontEntry *aProxyEntry,
                                    const nsAString& aFontName)
{
    return gfxPlatformFontList::PlatformFontList()->LookupLocalFont(aProxyEntry, 
                                                                    aFontName);
}

gfxFontEntry* 
gfxWindowsPlatform::MakePlatformFont(const gfxProxyFontEntry *aProxyEntry,
                                     const PRUint8 *aFontData, PRUint32 aLength)
{
    return gfxPlatformFontList::PlatformFontList()->MakePlatformFont(aProxyEntry,
                                                                     aFontData,
                                                                     aLength);
}

PRBool
gfxWindowsPlatform::IsFontFormatSupported(nsIURI *aFontURI, PRUint32 aFormatFlags)
{
    // check for strange format flags
    NS_ASSERTION(!(aFormatFlags & gfxUserFontSet::FLAG_FORMAT_NOT_USED),
                 "strange font format hint set");

    // accept supported formats
    if (aFormatFlags & (gfxUserFontSet::FLAG_FORMAT_WOFF     |
                        gfxUserFontSet::FLAG_FORMAT_OPENTYPE | 
                        gfxUserFontSet::FLAG_FORMAT_TRUETYPE)) {
        return PR_TRUE;
    }

    // reject all other formats, known and unknown
    if (aFormatFlags != 0) {
        return PR_FALSE;
    }

    // no format hint set, need to look at data
    return PR_TRUE;
}

gfxFontFamily *
gfxWindowsPlatform::FindFontFamily(const nsAString& aName)
{
    return gfxPlatformFontList::PlatformFontList()->FindFamily(aName);
}

gfxFontEntry *
gfxWindowsPlatform::FindFontEntry(const nsAString& aName, const gfxFontStyle& aFontStyle)
{
    nsRefPtr<gfxFontFamily> ff = FindFontFamily(aName);
    if (!ff)
        return nsnull;

    PRBool aNeedsBold;
    return ff->FindFontForStyle(aFontStyle, aNeedsBold);
}

qcms_profile*
gfxWindowsPlatform::GetPlatformCMSOutputProfile()
{
#ifndef MOZ_FT2_FONTS
    WCHAR str[MAX_PATH];
    DWORD size = MAX_PATH;
    BOOL res;

    HDC dc = GetDC(nsnull);
    if (!dc)
        return nsnull;

#if _MSC_VER
    __try {
        res = GetICMProfileW(dc, &size, (LPWSTR)&str);
    } __except(GetExceptionCode() == EXCEPTION_ILLEGAL_INSTRUCTION) {
        res = FALSE;
    }
#else
    res = GetICMProfileW(dc, &size, (LPWSTR)&str);
#endif

    ReleaseDC(nsnull, dc);
    if (!res)
        return nsnull;

    qcms_profile* profile =
        qcms_profile_from_path(NS_ConvertUTF16toUTF8(str).get());
#ifdef DEBUG_tor
    if (profile)
        fprintf(stderr,
                "ICM profile read from %s successfully\n",
                NS_ConvertUTF16toUTF8(str).get());
#endif
    return profile;
#else
    return nsnull;
#endif
}

PRBool
gfxWindowsPlatform::GetPrefFontEntries(const nsCString& aKey, nsTArray<nsRefPtr<gfxFontEntry> > *array)
{
    return mPrefFonts.Get(aKey, array);
}

void
gfxWindowsPlatform::SetPrefFontEntries(const nsCString& aKey, nsTArray<nsRefPtr<gfxFontEntry> >& array)
{
    mPrefFonts.Put(aKey, array);
}

#ifdef MOZ_FT2_FONTS
FT_Library
gfxWindowsPlatform::GetFTLibrary()
{
    return gPlatformFTLibrary;
}
#endif

PRBool
gfxWindowsPlatform::UseClearTypeForDownloadableFonts()
{
    if (mUseClearTypeForDownloadableFonts == UNINITIALIZED_VALUE) {
        mUseClearTypeForDownloadableFonts = GetBoolPref(GFX_DOWNLOADABLE_FONTS_USE_CLEARTYPE, PR_TRUE);
    }

    return mUseClearTypeForDownloadableFonts;
}

PRBool
gfxWindowsPlatform::UseClearTypeAlways()
{
    if (mUseClearTypeAlways == UNINITIALIZED_VALUE) {
        mUseClearTypeAlways = GetBoolPref(GFX_USE_CLEARTYPE_ALWAYS, PR_FALSE);
    }

    return mUseClearTypeAlways;
}

PRInt32
gfxWindowsPlatform::WindowsOSVersion()
{
    static PRInt32 winVersion = UNINITIALIZED_VALUE;

    OSVERSIONINFO vinfo;

    if (winVersion == UNINITIALIZED_VALUE) {
        vinfo.dwOSVersionInfoSize = sizeof (vinfo);
        if (!GetVersionEx(&vinfo)) {
            winVersion = kWindowsUnknown;
        } else {
            winVersion = PRInt32(vinfo.dwMajorVersion << 16) + vinfo.dwMinorVersion;
        }
    }
    return winVersion;
}

void
gfxWindowsPlatform::FontsPrefsChanged(nsIPrefBranch *aPrefBranch, const char *aPref)
{
    PRBool clearTextFontCaches = PR_TRUE;

    gfxPlatform::FontsPrefsChanged(aPrefBranch, aPref);

    if (!aPref) {
        mUseClearTypeForDownloadableFonts = UNINITIALIZED_VALUE;
        mUseClearTypeAlways = UNINITIALIZED_VALUE;
    } else if (!strcmp(GFX_DOWNLOADABLE_FONTS_USE_CLEARTYPE, aPref)) {
        mUseClearTypeForDownloadableFonts = UNINITIALIZED_VALUE;
    } else if (!strcmp(GFX_USE_CLEARTYPE_ALWAYS, aPref)) {
        mUseClearTypeAlways = UNINITIALIZED_VALUE;
    } else {
        clearTextFontCaches = PR_FALSE;
    }

    if (clearTextFontCaches) {    
        gfxFontCache *fc = gfxFontCache::GetCache();
        if (fc) {
            fc->Flush();
        }
        gfxTextRunWordCache::Flush();
    }
}
