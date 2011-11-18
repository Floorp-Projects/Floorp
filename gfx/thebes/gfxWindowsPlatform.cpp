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

#include "mozilla/Util.h"

#include "gfxWindowsPlatform.h"

#include "gfxImageSurface.h"
#include "gfxWindowsSurface.h"
#include "gfxTextRunWordCache.h"

#include "nsUnicharUtils.h"

#include "mozilla/Preferences.h"
#include "nsServiceManagerUtils.h"
#include "nsTArray.h"

#include "nsIWindowsRegKey.h"
#include "nsILocalFile.h"
#include "plbase64.h"
#include "nsIXULRuntime.h"

#include "nsIGfxInfo.h"

#include "gfxCrashReporterUtils.h"

#include "gfxGDIFontList.h"
#include "gfxGDIFont.h"

#ifdef CAIRO_HAS_DWRITE_FONT
#include "gfxDWriteFontList.h"
#include "gfxDWriteFonts.h"
#include "gfxDWriteCommon.h"
#include <dwrite.h>
#endif

#include "gfxUserFontSet.h"

#include <string>

using namespace mozilla;
using namespace mozilla::gfx;

#ifdef CAIRO_HAS_D2D_SURFACE
#include "gfxD2DSurface.h"

#include <d3d10_1.h>
#include <dxgi.h>

#include "mozilla/gfx/2D.h"

#include "nsIMemoryReporter.h"
#include "nsMemory.h"
#endif

using namespace mozilla;

#ifdef CAIRO_HAS_D2D_SURFACE

NS_MEMORY_REPORTER_IMPLEMENT(
    D2DCache,
    "gfx-d2d-surfacecache",
    KIND_OTHER,
    UNITS_BYTES,
    cairo_d2d_get_image_surface_cache_usage,
    "Memory used by the Direct2D internal surface cache.")

namespace
{

PRInt64 GetD2DSurfaceVramUsage() {
  cairo_device_t *device =
      gfxWindowsPlatform::GetPlatform()->GetD2DDevice();
  if (device) {
      return cairo_d2d_get_surface_vram_usage(device);
  }
  return 0;
}

} // anonymous namespace

NS_MEMORY_REPORTER_IMPLEMENT(
    D2DVram,
    "gfx-d2d-surfacevram",
    KIND_OTHER,
    UNITS_BYTES,
    GetD2DSurfaceVramUsage,
    "Video memory used by D2D surfaces")

#endif

#define GFX_USE_CLEARTYPE_ALWAYS "gfx.font_rendering.cleartype.always_use_for_content"
#define GFX_DOWNLOADABLE_FONTS_USE_CLEARTYPE "gfx.font_rendering.cleartype.use_for_downloadable_fonts"

#define GFX_CLEARTYPE_PARAMS           "gfx.font_rendering.cleartype_params."
#define GFX_CLEARTYPE_PARAMS_GAMMA     "gfx.font_rendering.cleartype_params.gamma"
#define GFX_CLEARTYPE_PARAMS_CONTRAST  "gfx.font_rendering.cleartype_params.enhanced_contrast"
#define GFX_CLEARTYPE_PARAMS_LEVEL     "gfx.font_rendering.cleartype_params.cleartype_level"
#define GFX_CLEARTYPE_PARAMS_STRUCTURE "gfx.font_rendering.cleartype_params.pixel_structure"
#define GFX_CLEARTYPE_PARAMS_MODE      "gfx.font_rendering.cleartype_params.rendering_mode"

#ifdef CAIRO_HAS_DWRITE_FONT
// DirectWrite is not available on all platforms, we need to use the function
// pointer.
typedef HRESULT (WINAPI*DWriteCreateFactoryFunc)(
  DWRITE_FACTORY_TYPE factoryType,
  REFIID iid,
  IUnknown **factory
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

typedef HRESULT(WINAPI*CreateDXGIFactory1Func)(
  REFIID riid,
  void **ppFactory
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

    mUsingGDIFonts = false;

    /* 
     * Initialize COM 
     */ 
    CoInitialize(NULL); 

    mScreenDC = GetDC(NULL);

#ifdef CAIRO_HAS_D2D_SURFACE
    NS_RegisterMemoryReporter(new NS_MEMORY_REPORTER_NAME(D2DCache));
    NS_RegisterMemoryReporter(new NS_MEMORY_REPORTER_NAME(D2DVram));
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
/* Pick the default render mode for
 * desktop.
 */
    mRenderMode = RENDER_GDI;

    OSVERSIONINFOA versionInfo;
    versionInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFOA);
    ::GetVersionExA(&versionInfo);
    bool isVistaOrHigher = versionInfo.dwMajorVersion >= 6;

    bool safeMode = false;
    nsCOMPtr<nsIXULRuntime> xr = do_GetService("@mozilla.org/xre/runtime;1");
    if (xr)
      xr->GetInSafeMode(&safeMode);

    mUseDirectWrite = Preferences::GetBool("gfx.font_rendering.directwrite.enabled", false);

#ifdef CAIRO_HAS_D2D_SURFACE
    bool d2dDisabled = false;
    bool d2dForceEnabled = false;
    bool d2dBlocked = false;

    nsCOMPtr<nsIGfxInfo> gfxInfo = do_GetService("@mozilla.org/gfx/info;1");
    if (gfxInfo) {
        PRInt32 status;
        if (NS_SUCCEEDED(gfxInfo->GetFeatureStatus(nsIGfxInfo::FEATURE_DIRECT2D, &status))) {
            if (status != nsIGfxInfo::FEATURE_NO_INFO) {
                d2dDisabled = true;
                if (status == nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION ||
                    status == nsIGfxInfo::FEATURE_BLOCKED_DEVICE)
                {
                    d2dBlocked = true;
                }
            }
        }
    }

    d2dDisabled = Preferences::GetBool("gfx.direct2d.disabled", false);
    d2dForceEnabled = Preferences::GetBool("gfx.direct2d.force-enabled", false);

    bool tryD2D = !d2dBlocked || d2dForceEnabled;
    
    // Do not ever try if d2d is explicitly disabled,
    // or if we're not using DWrite fonts.
    if (d2dDisabled || mUsingGDIFonts) {
        tryD2D = false;
    }

    if (isVistaOrHigher  && !safeMode && tryD2D) {
        VerifyD2DDevice(d2dForceEnabled);
        if (mD2DDevice) {
            mRenderMode = RENDER_DIRECT2D;
            mUseDirectWrite = true;
        }
    } else {
        mD2DDevice = nsnull;
    }
#endif

#ifdef CAIRO_HAS_DWRITE_FONT
    // Enable when it's preffed on -and- we're using Vista or higher. Or when
    // we're going to use D2D.
    if (!mDWriteFactory && (mUseDirectWrite && isVistaOrHigher)) {
        mozilla::ScopedGfxFeatureReporter reporter("DWrite");
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

            SetupClearTypeParams();

            if (hr == S_OK)
              reporter.SetSuccessful();
        }
    }
#endif
}

void
gfxWindowsPlatform::VerifyD2DDevice(bool aAttemptForce)
{
#ifdef CAIRO_HAS_D2D_SURFACE
    if (mD2DDevice) {
        ID3D10Device1 *device = cairo_d2d_device_get_device(mD2DDevice);

        if (SUCCEEDED(device->GetDeviceRemovedReason())) {
            return;
        }
        mD2DDevice = nsnull;
    }

    mozilla::ScopedGfxFeatureReporter reporter("D2D");

    HMODULE d3d10module = LoadLibraryA("d3d10_1.dll");
    D3D10CreateDevice1Func createD3DDevice = (D3D10CreateDevice1Func)
        GetProcAddress(d3d10module, "D3D10CreateDevice1");
    nsRefPtr<ID3D10Device1> device;

    if (createD3DDevice) {
        HMODULE dxgiModule = LoadLibraryA("dxgi.dll");
        CreateDXGIFactory1Func createDXGIFactory1 = (CreateDXGIFactory1Func)
            GetProcAddress(dxgiModule, "CreateDXGIFactory1");

        // Try to use a DXGI 1.1 adapter in order to share resources
        // across processes.
        nsRefPtr<IDXGIAdapter1> adapter1;
        if (createDXGIFactory1) {
            nsRefPtr<IDXGIFactory1> factory1;
            HRESULT hr = createDXGIFactory1(__uuidof(IDXGIFactory1),
                                            getter_AddRefs(factory1));
    
            nsRefPtr<IDXGIAdapter1> adapter1; 
            hr = factory1->EnumAdapters1(0, getter_AddRefs(adapter1));

            if (SUCCEEDED(hr) && adapter1) {
                hr = adapter1->CheckInterfaceSupport(__uuidof(ID3D10Device1),
                                                     nsnull);
                if (FAILED(hr)) {
                    adapter1 = nsnull;
                }
            }
        }

        // We try 10.0 first even though we prefer 10.1, since we want to
        // fail as fast as possible if 10.x isn't supported.
        HRESULT hr = createD3DDevice(
            adapter1, 
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
                adapter1, 
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

    if (mD2DDevice) {
        reporter.SetSuccessful();
        mozilla::gfx::Factory::SetDirect3D10Device(cairo_d2d_device_get_device(mD2DDevice));
    }
#endif
}

// bug 630201 - older pre-RTM versions of Direct2D/DirectWrite cause odd
// crashers so blacklist them altogether

#ifdef CAIRO_HAS_DWRITE_FONT
#define WINDOWS7_RTM_BUILD 7600

static bool
AllowDirectWrite()
{
    PRInt32 winVers, buildNum;

    winVers = gfxWindowsPlatform::WindowsOSVersion(&buildNum);
    if (winVers == gfxWindowsPlatform::kWindows7 &&
        buildNum < WINDOWS7_RTM_BUILD)
    {
        // don't use Direct2D/DirectWrite on older versions of Windows 7
        return false;
    }

    return true;
}
#endif

gfxPlatformFontList*
gfxWindowsPlatform::CreatePlatformFontList()
{
    mUsingGDIFonts = false;
    gfxPlatformFontList *pfl;
#ifdef CAIRO_HAS_DWRITE_FONT
    if (AllowDirectWrite() && GetDWriteFactory()) {
        pfl = new gfxDWriteFontList();
        if (NS_SUCCEEDED(pfl->InitFontList())) {
            return pfl;
        }
        // DWrite font initialization failed! Don't know why this would happen,
        // but apparently it can - see bug 594865.
        // So we're going to fall back to GDI fonts & rendering.
        gfxPlatformFontList::Shutdown();
        SetRenderMode(RENDER_GDI);
    }
#endif
    pfl = new gfxGDIFontList();
    mUsingGDIFonts = true;

    if (NS_SUCCEEDED(pfl->InitFontList())) {
        return pfl;
    }

    gfxPlatformFontList::Shutdown();
    return nsnull;
}

already_AddRefed<gfxASurface>
gfxWindowsPlatform::CreateOffscreenSurface(const gfxIntSize& size,
                                           gfxASurface::gfxContentType contentType)
{
    gfxASurface *surf = nsnull;

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

RefPtr<ScaledFont>
gfxWindowsPlatform::GetScaledFontForFont(gfxFont *aFont)
{
  if(mUseDirectWrite) {
    gfxDWriteFont *font = static_cast<gfxDWriteFont*>(aFont);

    NativeFont nativeFont;
    nativeFont.mType = NATIVE_FONT_DWRITE_FONT_FACE;
    nativeFont.mFont = font->GetFontFace();
    RefPtr<ScaledFont> scaledFont =
      mozilla::gfx::Factory::CreateScaledFontForNativeFont(nativeFont, font->GetAdjustedSize());

    return scaledFont;
  }

  NativeFont nativeFont;
  nativeFont.mType = NATIVE_FONT_GDI_FONT_FACE;
  nativeFont.mFont = aFont;
  RefPtr<ScaledFont> scaledFont =
    Factory::CreateScaledFontForNativeFont(nativeFont, aFont->GetAdjustedSize());

  return scaledFont;
}

already_AddRefed<gfxASurface>
gfxWindowsPlatform::GetThebesSurfaceForDrawTarget(DrawTarget *aTarget)
{
#ifdef XP_WIN
  if (aTarget->GetType() == BACKEND_DIRECT2D) {
    RefPtr<ID3D10Texture2D> texture =
      static_cast<ID3D10Texture2D*>(aTarget->GetNativeSurface(NATIVE_SURFACE_D3D10_TEXTURE));

    if (!texture) {
      return gfxPlatform::GetThebesSurfaceForDrawTarget(aTarget);
    }

    aTarget->Flush();

    nsRefPtr<gfxASurface> surf =
      new gfxD2DSurface(texture, ContentForFormat(aTarget->GetFormat()));

    surf->SetData(&kDrawTarget, aTarget, NULL);

    return surf.forget();
  }
#endif

  return gfxPlatform::GetThebesSurfaceForDrawTarget(aTarget);
}

bool
gfxWindowsPlatform::SupportsAzure(BackendType& aBackend)
{
#ifdef CAIRO_HAS_D2D_SURFACE
  if (mRenderMode == RENDER_DIRECT2D) {
      aBackend = BACKEND_DIRECT2D;
      return true;
  }
#endif
  
  if (Preferences::GetBool("gfx.canvas.azure.prefer-skia", false)) {
    aBackend = BACKEND_SKIA;
    return true;
  }
  return false;
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
                                    bool& aAborted)
{
    nsAutoString resolvedName;
    if (!gfxPlatformFontList::PlatformFontList()->
             ResolveFontName(aFontName, resolvedName)) {
        aAborted = false;
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
    return new gfxFontGroup(aFamilies, aStyle, aUserFontSet);
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

bool
gfxWindowsPlatform::IsFontFormatSupported(nsIURI *aFontURI, PRUint32 aFormatFlags)
{
    // check for strange format flags
    NS_ASSERTION(!(aFormatFlags & gfxUserFontSet::FLAG_FORMAT_NOT_USED),
                 "strange font format hint set");

    // accept supported formats
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

    bool aNeedsBold;
    return ff->FindFontForStyle(aFontStyle, aNeedsBold);
}

qcms_profile*
gfxWindowsPlatform::GetPlatformCMSOutputProfile()
{
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

    qcms_profile* profile = qcms_profile_from_unicode_path(str);
#ifdef DEBUG_tor
    if (profile)
        fprintf(stderr,
                "ICM profile read from %s successfully\n",
                NS_ConvertUTF16toUTF8(str).get());
#endif
    return profile;
}

bool
gfxWindowsPlatform::GetPrefFontEntries(const nsCString& aKey, nsTArray<nsRefPtr<gfxFontEntry> > *array)
{
    return mPrefFonts.Get(aKey, array);
}

void
gfxWindowsPlatform::SetPrefFontEntries(const nsCString& aKey, nsTArray<nsRefPtr<gfxFontEntry> >& array)
{
    mPrefFonts.Put(aKey, array);
}

bool
gfxWindowsPlatform::UseClearTypeForDownloadableFonts()
{
    if (mUseClearTypeForDownloadableFonts == UNINITIALIZED_VALUE) {
        mUseClearTypeForDownloadableFonts = Preferences::GetBool(GFX_DOWNLOADABLE_FONTS_USE_CLEARTYPE, true);
    }

    return mUseClearTypeForDownloadableFonts;
}

bool
gfxWindowsPlatform::UseClearTypeAlways()
{
    if (mUseClearTypeAlways == UNINITIALIZED_VALUE) {
        mUseClearTypeAlways = Preferences::GetBool(GFX_USE_CLEARTYPE_ALWAYS, false);
    }

    return mUseClearTypeAlways;
}

PRInt32
gfxWindowsPlatform::WindowsOSVersion(PRInt32 *aBuildNum)
{
    static PRInt32 winVersion = UNINITIALIZED_VALUE;
    static PRInt32 buildNum = UNINITIALIZED_VALUE;

    OSVERSIONINFO vinfo;

    if (winVersion == UNINITIALIZED_VALUE) {
        vinfo.dwOSVersionInfoSize = sizeof (vinfo);
        if (!GetVersionEx(&vinfo)) {
            winVersion = kWindowsUnknown;
            buildNum = 0;
        } else {
            winVersion = PRInt32(vinfo.dwMajorVersion << 16) + vinfo.dwMinorVersion;
            buildNum = PRInt32(vinfo.dwBuildNumber);
        }
    }

    if (aBuildNum) {
        *aBuildNum = buildNum;
    }

    return winVersion;
}

void 
gfxWindowsPlatform::GetDLLVersion(const PRUnichar *aDLLPath, nsAString& aVersion)
{
    DWORD versInfoSize, vers[4] = {0};
    // version info not available case
    aVersion.Assign(NS_LITERAL_STRING("0.0.0.0"));
    versInfoSize = GetFileVersionInfoSizeW(aDLLPath, NULL);
    nsAutoTArray<BYTE,512> versionInfo;
    
    if (versInfoSize == 0 ||
        !versionInfo.AppendElements(PRUint32(versInfoSize)))
    {
        return;
    }

    if (!GetFileVersionInfoW(aDLLPath, 0, versInfoSize, 
           LPBYTE(versionInfo.Elements())))
    {
        return;
    } 

    UINT len = 0;
    VS_FIXEDFILEINFO *fileInfo = nsnull;
    if (!VerQueryValue(LPBYTE(versionInfo.Elements()), TEXT("\\"),
           (LPVOID *)&fileInfo, &len) ||
        len == 0 ||
        fileInfo == nsnull)
    {
        return;
    }

    DWORD fileVersMS = fileInfo->dwFileVersionMS; 
    DWORD fileVersLS = fileInfo->dwFileVersionLS;

    vers[0] = HIWORD(fileVersMS);
    vers[1] = LOWORD(fileVersMS);
    vers[2] = HIWORD(fileVersLS);
    vers[3] = LOWORD(fileVersLS);

    char buf[256];
    sprintf(buf, "%d.%d.%d.%d", vers[0], vers[1], vers[2], vers[3]);
    aVersion.Assign(NS_ConvertUTF8toUTF16(buf));
}

void 
gfxWindowsPlatform::GetCleartypeParams(nsTArray<ClearTypeParameterInfo>& aParams)
{
    HKEY  hKey, subKey;
    DWORD i, rv, size, type;
    WCHAR displayName[256], subkeyName[256];

    aParams.Clear();

    // construct subkeys based on HKLM subkeys, assume they are same for HKCU
    rv = RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                       L"Software\\Microsoft\\Avalon.Graphics",
                       0, KEY_READ, &hKey);

    if (rv != ERROR_SUCCESS) {
        return;
    }

    // enumerate over subkeys
    for (i = 0, rv = ERROR_SUCCESS; rv != ERROR_NO_MORE_ITEMS; i++) {
        size = ArrayLength(displayName);
        rv = RegEnumKeyExW(hKey, i, displayName, &size, NULL, NULL, NULL, NULL);
        if (rv != ERROR_SUCCESS) {
            continue;
        }

        ClearTypeParameterInfo ctinfo;
        ctinfo.displayName.Assign(displayName);

        DWORD subrv, value;
        bool foundData = false;

        swprintf_s(subkeyName, ArrayLength(subkeyName),
                   L"Software\\Microsoft\\Avalon.Graphics\\%s", displayName);

        // subkey for gamma, pixel structure
        subrv = RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                              subkeyName, 0, KEY_QUERY_VALUE, &subKey);

        if (subrv == ERROR_SUCCESS) {
            size = sizeof(value);
            subrv = RegQueryValueExW(subKey, L"GammaLevel", NULL, &type,
                                     (LPBYTE)&value, &size);
            if (subrv == ERROR_SUCCESS && type == REG_DWORD) {
                foundData = true;
                ctinfo.gamma = value;
            }

            size = sizeof(value);
            subrv = RegQueryValueExW(subKey, L"PixelStructure", NULL, &type,
                                     (LPBYTE)&value, &size);
            if (subrv == ERROR_SUCCESS && type == REG_DWORD) {
                foundData = true;
                ctinfo.pixelStructure = value;
            }

            RegCloseKey(subKey);
        }

        // subkey for cleartype level, enhanced contrast
        subrv = RegOpenKeyExW(HKEY_CURRENT_USER,
                              subkeyName, 0, KEY_QUERY_VALUE, &subKey);

        if (subrv == ERROR_SUCCESS) {
            size = sizeof(value);
            subrv = RegQueryValueExW(subKey, L"ClearTypeLevel", NULL, &type,
                                     (LPBYTE)&value, &size);
            if (subrv == ERROR_SUCCESS && type == REG_DWORD) {
                foundData = true;
                ctinfo.clearTypeLevel = value;
            }
      
            size = sizeof(value);
            subrv = RegQueryValueExW(subKey, L"EnhancedContrastLevel",
                                     NULL, &type, (LPBYTE)&value, &size);
            if (subrv == ERROR_SUCCESS && type == REG_DWORD) {
                foundData = true;
                ctinfo.enhancedContrast = value;
            }

            RegCloseKey(subKey);
        }

        if (foundData) {
            aParams.AppendElement(ctinfo);
        }
    }

    RegCloseKey(hKey);
}

void
gfxWindowsPlatform::FontsPrefsChanged(const char *aPref)
{
    bool clearTextFontCaches = true;

    gfxPlatform::FontsPrefsChanged(aPref);

    if (!aPref) {
        mUseClearTypeForDownloadableFonts = UNINITIALIZED_VALUE;
        mUseClearTypeAlways = UNINITIALIZED_VALUE;
    } else if (!strcmp(GFX_DOWNLOADABLE_FONTS_USE_CLEARTYPE, aPref)) {
        mUseClearTypeForDownloadableFonts = UNINITIALIZED_VALUE;
    } else if (!strcmp(GFX_USE_CLEARTYPE_ALWAYS, aPref)) {
        mUseClearTypeAlways = UNINITIALIZED_VALUE;
    } else if (!strncmp(GFX_CLEARTYPE_PARAMS, aPref, strlen(GFX_CLEARTYPE_PARAMS))) {
        SetupClearTypeParams();
    } else {
        clearTextFontCaches = false;
    }

    if (clearTextFontCaches) {    
        gfxFontCache *fc = gfxFontCache::GetCache();
        if (fc) {
            fc->Flush();
        }
        gfxTextRunWordCache::Flush();
    }
}

void
gfxWindowsPlatform::SetupClearTypeParams()
{
#if CAIRO_HAS_DWRITE_FONT
    if (GetDWriteFactory()) {
        // any missing prefs will default to invalid (-1) and be ignored;
        // out-of-range values will also be ignored
        FLOAT gamma = -1.0;
        FLOAT contrast = -1.0;
        FLOAT level = -1.0;
        int geometry = -1;
        int mode = -1;
        PRInt32 value;
        if (NS_SUCCEEDED(Preferences::GetInt(GFX_CLEARTYPE_PARAMS_GAMMA, &value))) {
            if (value >= 1000 && value <= 2200) {
                gamma = FLOAT(value / 1000.0);
            }
        }

        if (NS_SUCCEEDED(Preferences::GetInt(GFX_CLEARTYPE_PARAMS_CONTRAST, &value))) {
            if (value >= 0 && value <= 1000) {
                contrast = FLOAT(value / 100.0);
            }
        }

        if (NS_SUCCEEDED(Preferences::GetInt(GFX_CLEARTYPE_PARAMS_LEVEL, &value))) {
            if (value >= 0 && value <= 100) {
                level = FLOAT(value / 100.0);
            }
        }

        if (NS_SUCCEEDED(Preferences::GetInt(GFX_CLEARTYPE_PARAMS_STRUCTURE, &value))) {
            if (value >= 0 && value <= 2) {
                geometry = value;
            }
        }

        if (NS_SUCCEEDED(Preferences::GetInt(GFX_CLEARTYPE_PARAMS_MODE, &value))) {
            if (value >= 0 && value <= 5) {
                mode = value;
            }
        }

        cairo_dwrite_set_cleartype_params(gamma, contrast, level, geometry, mode);

        switch (mode) {
        case DWRITE_RENDERING_MODE_ALIASED:
        case DWRITE_RENDERING_MODE_CLEARTYPE_GDI_CLASSIC:
            mMeasuringMode = DWRITE_MEASURING_MODE_GDI_CLASSIC;
            break;
        case DWRITE_RENDERING_MODE_CLEARTYPE_GDI_NATURAL:
            mMeasuringMode = DWRITE_MEASURING_MODE_GDI_NATURAL;
            break;
        default:
            mMeasuringMode = DWRITE_MEASURING_MODE_NATURAL;
            break;
        }
    }
#endif
}

bool
gfxWindowsPlatform::IsOptimus()
{
  return GetModuleHandleA("nvumdshim.dll");
}
