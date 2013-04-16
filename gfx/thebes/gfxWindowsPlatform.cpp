/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include "gfxWindowsPlatform.h"

#include "gfxImageSurface.h"
#include "gfxWindowsSurface.h"

#include "nsUnicharUtils.h"

#include "mozilla/Preferences.h"
#include "nsServiceManagerUtils.h"
#include "nsTArray.h"

#include "nsIWindowsRegKey.h"
#include "nsIFile.h"
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
#include "nsWindowsHelpers.h"

#include <string>

using namespace mozilla;
using namespace mozilla::gfx;

#ifdef CAIRO_HAS_D2D_SURFACE
#include "gfxD2DSurface.h"

#include <d3d10_1.h>

#include "mozilla/gfx/2D.h"

#include "nsMemory.h"
#endif

/*
 * Required headers are not available in the current consumer preview Win8
 * dev kit, disabling for now.
 */
#undef MOZ_WINSDK_TARGETVER

/**
 * XXX below should be >= MOZ_NTDDI_WIN8 or such which is not defined yet
 */
#if MOZ_WINSDK_TARGETVER > MOZ_NTDDI_WIN7
#define ENABLE_GPU_MEM_REPORTER
#endif

#if defined CAIRO_HAS_D2D_SURFACE || defined ENABLE_GPU_MEM_REPORTER
#include "nsIMemoryReporter.h"
#endif

#ifdef ENABLE_GPU_MEM_REPORTER
#include <winternl.h>

/**
 * XXX need to check that extern C is really needed with Win8 SDK.
 *     It was required for files I had available at push time.
 */
extern "C" {
#include <d3dkmthk.h>
}
#endif

using namespace mozilla;

#ifdef CAIRO_HAS_D2D_SURFACE

static const char *kFeatureLevelPref =
  "gfx.direct3d.last_used_feature_level_idx";
static const int kSupportedFeatureLevels[] =
  { D3D10_FEATURE_LEVEL_10_1, D3D10_FEATURE_LEVEL_10_0,
    D3D10_FEATURE_LEVEL_9_3 };

NS_MEMORY_REPORTER_IMPLEMENT(
    D2DCache,
    "gfx-d2d-surfacecache",
    KIND_OTHER,
    UNITS_BYTES,
    cairo_d2d_get_image_surface_cache_usage,
    "Memory used by the Direct2D internal surface cache.")

namespace
{

int64_t GetD2DSurfaceVramUsage() {
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
    "Video memory used by D2D surfaces.")

#endif

namespace
{

int64_t GetD2DVRAMUsageDrawTarget() {
    return mozilla::gfx::Factory::GetD2DVRAMUsageDrawTarget();
}

int64_t GetD2DVRAMUsageSourceSurface() {
    return mozilla::gfx::Factory::GetD2DVRAMUsageSourceSurface();
}

} // anonymous namespace

NS_MEMORY_REPORTER_IMPLEMENT(
    D2DVRAMDT,
    "gfx-d2d-vram-drawtarget",
    KIND_OTHER,
    UNITS_BYTES,
    GetD2DVRAMUsageDrawTarget,
    "Video memory used by D2D DrawTargets.")

NS_MEMORY_REPORTER_IMPLEMENT(
    D2DVRAMSS,
    "gfx-d2d-vram-sourcesurface",
    KIND_OTHER,
    UNITS_BYTES,
    GetD2DVRAMUsageSourceSurface,
    "Video memory used by D2D SourceSurfaces.")

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

#ifdef ENABLE_GPU_MEM_REPORTER
class GPUAdapterMultiReporter : public nsIMemoryMultiReporter {

    // Callers must Release the DXGIAdapter after use or risk mem-leak
    static bool GetDXGIAdapter(__out IDXGIAdapter **DXGIAdapter)
    {
        ID3D10Device1 *D2D10Device;
        IDXGIDevice *DXGIDevice;
        bool result = false;
        
        if (D2D10Device = mozilla::gfx::Factory::GetDirect3D10Device()) {
            if (D2D10Device->QueryInterface(__uuidof(IDXGIDevice), (void **)&DXGIDevice) == S_OK) {
                result = (DXGIDevice->GetAdapter(DXGIAdapter) == S_OK);
                DXGIDevice->Release();
            }
        }
        
        return result;
    }
    
public:
    NS_DECL_ISUPPORTS
    
    // nsIMemoryMultiReporter abstract method implementation
    NS_IMETHOD
    CollectReports(nsIMemoryMultiReporterCallback* aCb,
                   nsISupports* aClosure)
    {
        int32_t winVers, buildNum;
        HANDLE ProcessHandle = GetCurrentProcess();
        
        int64_t dedicatedBytesUsed = 0;
        int64_t sharedBytesUsed = 0;
        int64_t committedBytesUsed = 0;
        IDXGIAdapter *DXGIAdapter;
        
        HMODULE gdi32Handle;
        PFND3DKMT_QUERYSTATISTICS queryD3DKMTStatistics;
        
        winVers = gfxWindowsPlatform::WindowsOSVersion(&buildNum);
        
        // GPU memory reporting is not available before Windows 7
        if (winVers < gfxWindowsPlatform::kWindows7) 
            return NS_OK;
        
        if (gdi32Handle = LoadLibrary(TEXT("gdi32.dll")))
            queryD3DKMTStatistics = (PFND3DKMT_QUERYSTATISTICS)GetProcAddress(gdi32Handle, "D3DKMTQueryStatistics");
        
        if (queryD3DKMTStatistics && GetDXGIAdapter(&DXGIAdapter)) {
            // Most of this block is understood thanks to wj32's work on Process Hacker
            
            DXGI_ADAPTER_DESC adapterDesc;
            D3DKMT_QUERYSTATISTICS queryStatistics;
            
            DXGIAdapter->GetDesc(&adapterDesc);
            DXGIAdapter->Release();
            
            memset(&queryStatistics, 0, sizeof(D3DKMT_QUERYSTATISTICS));
            queryStatistics.Type = D3DKMT_QUERYSTATISTICS_PROCESS;
            queryStatistics.AdapterLuid = adapterDesc.AdapterLuid;
            queryStatistics.hProcess = ProcessHandle;
            if (NT_SUCCESS(queryD3DKMTStatistics(&queryStatistics))) {
                committedBytesUsed = queryStatistics.QueryResult.ProcessInformation.SystemMemory.BytesAllocated;
            }
            
            memset(&queryStatistics, 0, sizeof(D3DKMT_QUERYSTATISTICS));
            queryStatistics.Type = D3DKMT_QUERYSTATISTICS_ADAPTER;
            queryStatistics.AdapterLuid = adapterDesc.AdapterLuid;
            if (NT_SUCCESS(queryD3DKMTStatistics(&queryStatistics))) {
                ULONG i;
                ULONG segmentCount = queryStatistics.QueryResult.AdapterInformation.NbSegments;
                
                for (i = 0; i < segmentCount; i++) {
                    memset(&queryStatistics, 0, sizeof(D3DKMT_QUERYSTATISTICS));
                    queryStatistics.Type = D3DKMT_QUERYSTATISTICS_SEGMENT;
                    queryStatistics.AdapterLuid = adapterDesc.AdapterLuid;
                    queryStatistics.QuerySegment.SegmentId = i;
                    
                    if (NT_SUCCESS(queryD3DKMTStatistics(&queryStatistics))) {
                        bool aperture;
                        
                        // SegmentInformation has a different definition in Win7 than later versions
                        if (winVers > gfxWindowsPlatform::kWindows7)
                            aperture = queryStatistics.QueryResult.SegmentInformation.Aperture;
                        else
                            aperture = queryStatistics.QueryResult.SegmentInformationV1.Aperture;
                        
                        memset(&queryStatistics, 0, sizeof(D3DKMT_QUERYSTATISTICS));
                        queryStatistics.Type = D3DKMT_QUERYSTATISTICS_PROCESS_SEGMENT;
                        queryStatistics.AdapterLuid = adapterDesc.AdapterLuid;
                        queryStatistics.hProcess = ProcessHandle;
                        queryStatistics.QueryProcessSegment.SegmentId = i;
                        if (NT_SUCCESS(queryD3DKMTStatistics(&queryStatistics))) {
                            if (aperture)
                                sharedBytesUsed += queryStatistics.QueryResult
                                                                  .ProcessSegmentInformation
                                                                  .BytesCommitted;
                            else
                                dedicatedBytesUsed += queryStatistics.QueryResult
                                                                     .ProcessSegmentInformation
                                                                     .BytesCommitted;
                        }
                    }
                }
            }
        }
        
        FreeLibrary(gdi32Handle);
        
#define REPORT(_path, _amount, _desc)                                         \
    do {                                                                      \
      nsresult rv;                                                            \
      rv = aCb->Callback(EmptyCString(), NS_LITERAL_CSTRING(_path),           \
                         nsIMemoryReporter::KIND_OTHER,                       \
                         nsIMemoryReporter::UNITS_BYTES, _amount,             \
                         NS_LITERAL_CSTRING(_desc), aClosure);                \
      NS_ENSURE_SUCCESS(rv, rv);                                              \
    } while (0)

        REPORT("gpu-committed", committedBytesUsed,
               "Memory committed by the Windows graphics system.");

        REPORT("gpu-dedicated", dedicatedBytesUsed,
               "Out-of-process memory allocated for this process in a "
               "physical GPU adapter's memory.");

        REPORT("gpu-shared", sharedBytesUsed,
               "In-process memory that is shared with the GPU.");
        
#undef REPORT

        return NS_OK;
    }
};
NS_IMPL_ISUPPORTS1(GPUAdapterMultiReporter, nsIMemoryMultiReporter)
#endif // ENABLE_GPU_MEM_REPORTER

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
    mD2DDevice = nullptr;
#endif
    NS_RegisterMemoryReporter(new NS_MEMORY_REPORTER_NAME(D2DVRAMDT));
    NS_RegisterMemoryReporter(new NS_MEMORY_REPORTER_NAME(D2DVRAMSS));

    UpdateRenderMode();

#ifdef ENABLE_GPU_MEM_REPORTER
    mGPUAdapterMultiReporter = new GPUAdapterMultiReporter();
    NS_RegisterMemoryMultiReporter(mGPUAdapterMultiReporter);
#endif
}

gfxWindowsPlatform::~gfxWindowsPlatform()
{
#ifdef ENABLE_GPU_MEM_REPORTER
    NS_UnregisterMemoryMultiReporter(mGPUAdapterMultiReporter);
#endif
    
    ::ReleaseDC(NULL, mScreenDC);
    // not calling FT_Done_FreeType because cairo may still hold references to
    // these FT_Faces.  See bug 458169.
#ifdef CAIRO_HAS_D2D_SURFACE
    if (mD2DDevice) {
        cairo_release_device(mD2DDevice);
    }
#endif

    mozilla::gfx::Factory::D2DCleanup();

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
        int32_t status;
        if (NS_SUCCEEDED(gfxInfo->GetFeatureStatus(nsIGfxInfo::FEATURE_DIRECT2D, &status))) {
            if (status != nsIGfxInfo::FEATURE_NO_INFO) {
                d2dBlocked = true;
            }
        }
    }

    d2dDisabled = Preferences::GetBool("gfx.direct2d.disabled", false);
    d2dForceEnabled = Preferences::GetBool("gfx.direct2d.force-enabled", false);

#ifdef MOZ_METRO
    // In Metro mode there is no fallback available
    d2dForceEnabled |= IsRunningInWindowsMetro();
#endif

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
        mD2DDevice = nullptr;
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

            if (SUCCEEDED(hr) && factory) {
                mDWriteFactory = factory;
                factory->Release();
                hr = mDWriteFactory->CreateTextAnalyzer(
                    getter_AddRefs(mDWriteAnalyzer));
            }

            SetupClearTypeParams();

            if (hr == S_OK)
              reporter.SetSuccessful();
        }
    }
#endif

    uint32_t canvasMask = 1 << BACKEND_CAIRO;
    uint32_t contentMask = 0;
    if (mRenderMode == RENDER_DIRECT2D) {
      canvasMask |= 1 << BACKEND_DIRECT2D;
      contentMask |= 1 << BACKEND_DIRECT2D;
    } else {
      canvasMask |= 1 << BACKEND_SKIA;
    }
    InitBackendPrefs(canvasMask, contentMask);
}

#ifdef CAIRO_HAS_D2D_SURFACE
HRESULT
gfxWindowsPlatform::CreateDevice(nsRefPtr<IDXGIAdapter1> &adapter1,
                                 int featureLevelIndex)
{
  nsModuleHandle d3d10module(LoadLibrarySystem32(L"d3d10_1.dll"));
  if (!d3d10module)
    return E_FAIL;
  D3D10CreateDevice1Func createD3DDevice =
    (D3D10CreateDevice1Func)GetProcAddress(d3d10module, "D3D10CreateDevice1");
  if (!createD3DDevice)
    return E_FAIL;

  nsRefPtr<ID3D10Device1> device;
  HRESULT hr =
    createD3DDevice(adapter1, D3D10_DRIVER_TYPE_HARDWARE, NULL,
                    D3D10_CREATE_DEVICE_BGRA_SUPPORT |
                    D3D10_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS,
                    static_cast<D3D10_FEATURE_LEVEL1>(kSupportedFeatureLevels[featureLevelIndex]),
                    D3D10_1_SDK_VERSION, getter_AddRefs(device));

  // If we fail here, the DirectX version or video card probably
  // changed.  We previously could use 10.1 but now we can't
  // anymore.  Revert back to doing a 10.0 check first before
  // the 10.1 check.
  if (device) {
    mD2DDevice = cairo_d2d_create_device_from_d3d10device(device);

    // Setup a pref for future launch optimizaitons
    Preferences::SetInt(kFeatureLevelPref, featureLevelIndex);
  }

  return device ? S_OK : hr;
}
#endif

void
gfxWindowsPlatform::VerifyD2DDevice(bool aAttemptForce)
{
#ifdef CAIRO_HAS_D2D_SURFACE
    if (mD2DDevice) {
        ID3D10Device1 *device = cairo_d2d_device_get_device(mD2DDevice);

        if (SUCCEEDED(device->GetDeviceRemovedReason())) {
            return;
        }
        mD2DDevice = nullptr;
    }

    mozilla::ScopedGfxFeatureReporter reporter("D2D", aAttemptForce);

    nsRefPtr<ID3D10Device1> device;

    nsModuleHandle dxgiModule(LoadLibrarySystem32(L"dxgi.dll"));
    CreateDXGIFactory1Func createDXGIFactory1 = (CreateDXGIFactory1Func)
        GetProcAddress(dxgiModule, "CreateDXGIFactory1");

    int supportedFeatureLevelsCount = ArrayLength(kSupportedFeatureLevels);
    // If we're not running in Metro don't allow DX9.3
    if (!IsRunningInWindowsMetro()) {
      supportedFeatureLevelsCount--;
    }

    // It takes a lot of time (5-10% of startup time or ~100ms) to do both
    // a createD3DDevice on D3D10_FEATURE_LEVEL_10_0.  We therefore store
    // the last used feature level to go direct to that.
    int featureLevelIndex = Preferences::GetInt(kFeatureLevelPref, 0);
    if (featureLevelIndex >= supportedFeatureLevelsCount || featureLevelIndex < 0)
      featureLevelIndex = 0;

    // Try to use a DXGI 1.1 adapter in order to share resources
    // across processes.
    nsRefPtr<IDXGIAdapter1> adapter1;
    if (createDXGIFactory1) {
        nsRefPtr<IDXGIFactory1> factory1;
        HRESULT hr = createDXGIFactory1(__uuidof(IDXGIFactory1),
                                        getter_AddRefs(factory1));

        if (FAILED(hr) || !factory1) {
          // This seems to happen with some people running the iZ3D driver.
          // They won't get acceleration.
          return;
        }

        hr = factory1->EnumAdapters1(0, getter_AddRefs(adapter1));
        if (FAILED(hr) || !adapter1) {
          // We should return and not accelerate if we can't obtain
          // an adapter.
          return;
        }
    }

    // Start with the last used feature level, and move to lower DX versions
    // until we find one that works.
    HRESULT hr = E_FAIL;
    for (int i = featureLevelIndex; i < supportedFeatureLevelsCount; i++) {
      hr = CreateDevice(adapter1, i);
      // If it succeeded we found the first available feature level
      if (SUCCEEDED(hr))
        break;
    }

    // If we succeeded in creating a device, try for a newer device
    // that we haven't tried yet.
    if (SUCCEEDED(hr)) {
      for (int i = featureLevelIndex - 1; i >= 0; i--) {
        hr = CreateDevice(adapter1, i);
        // If it failed then we don't have new hardware
        if (FAILED(hr)) {
          break;
        }
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
    int32_t winVers, buildNum;

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
    return nullptr;
}

already_AddRefed<gfxASurface>
gfxWindowsPlatform::CreateOffscreenSurface(const gfxIntSize& size,
                                           gfxASurface::gfxContentType contentType)
{
    nsRefPtr<gfxASurface> surf = nullptr;

#ifdef CAIRO_HAS_WIN32_SURFACE
    if (mRenderMode == RENDER_GDI)
        surf = new gfxWindowsSurface(size, OptimalFormatForContent(contentType));
#endif

#ifdef CAIRO_HAS_D2D_SURFACE
    if (mRenderMode == RENDER_DIRECT2D)
        surf = new gfxD2DSurface(size, OptimalFormatForContent(contentType));
#endif

    if (!surf || surf->CairoStatus()) {
        surf = new gfxImageSurface(size, OptimalFormatForContent(contentType));
    }

    return surf.forget();
}

already_AddRefed<gfxASurface>
gfxWindowsPlatform::CreateOffscreenImageSurface(const gfxIntSize& aSize,
                                                gfxASurface::gfxContentType aContentType)
{
#ifdef CAIRO_HAS_D2D_SURFACE
    if (mRenderMode == RENDER_DIRECT2D) {
        return new gfxImageSurface(aSize, OptimalFormatForContent(aContentType));
    }
#endif

    nsRefPtr<gfxASurface> surface = CreateOffscreenSurface(aSize, aContentType);
#ifdef DEBUG
    nsRefPtr<gfxImageSurface> imageSurface = surface->GetAsImageSurface();
    NS_ASSERTION(imageSurface, "Surface cannot be converted to a gfxImageSurface");
#endif
    return surface.forget();
}

TemporaryRef<ScaledFont>
gfxWindowsPlatform::GetScaledFontForFont(DrawTarget* aTarget, gfxFont *aFont)
{
    if (aFont->GetType() == gfxFont::FONT_TYPE_DWRITE) {
        gfxDWriteFont *font = static_cast<gfxDWriteFont*>(aFont);

        NativeFont nativeFont;
        nativeFont.mType = NATIVE_FONT_DWRITE_FONT_FACE;
        nativeFont.mFont = font->GetFontFace();

        if (aTarget->GetType() == BACKEND_CAIRO) {
          return Factory::CreateScaledFontWithCairo(nativeFont,
                                                    font->GetAdjustedSize(),
                                                    font->GetCairoScaledFont());
        }

        return Factory::CreateScaledFontForNativeFont(nativeFont,
                                                      font->GetAdjustedSize());
    }

    NS_ASSERTION(aFont->GetType() == gfxFont::FONT_TYPE_GDI,
        "Fonts on windows should be GDI or DWrite!");

    NativeFont nativeFont;
    nativeFont.mType = NATIVE_FONT_GDI_FONT_FACE;
    LOGFONT lf;
    GetObject(static_cast<gfxGDIFont*>(aFont)->GetHFONT(), sizeof(LOGFONT), &lf);
    nativeFont.mFont = &lf;

    if (aTarget->GetType() == BACKEND_CAIRO) {
      return Factory::CreateScaledFontWithCairo(nativeFont,
                                                aFont->GetAdjustedSize(),
                                                aFont->GetCairoScaledFont());
    }

    return Factory::CreateScaledFontForNativeFont(nativeFont, aFont->GetAdjustedSize());
}

already_AddRefed<gfxASurface>
gfxWindowsPlatform::GetThebesSurfaceForDrawTarget(DrawTarget *aTarget)
{
#ifdef XP_WIN
  if (aTarget->GetType() == BACKEND_DIRECT2D) {
    if (!GetD2DDevice()) {
      // We no longer have a D2D device, can't do this.
      return NULL;
    }

    RefPtr<ID3D10Texture2D> texture =
      static_cast<ID3D10Texture2D*>(aTarget->GetNativeSurface(NATIVE_SURFACE_D3D10_TEXTURE));

    if (!texture) {
      return gfxPlatform::GetThebesSurfaceForDrawTarget(aTarget);
    }

    aTarget->Flush();

    nsRefPtr<gfxASurface> surf =
      new gfxD2DSurface(texture, ContentForFormat(aTarget->GetFormat()));

    // shouldn't this hold a reference?
    surf->SetData(&kDrawTarget, aTarget, NULL);
    return surf.forget();
  }
#endif

  return gfxPlatform::GetThebesSurfaceForDrawTarget(aTarget);
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
    int32_t comma = aName.FindChar(PRUnichar(','));
    if (comma >= 0)
        aName.Truncate(comma);
}

nsresult
gfxWindowsPlatform::UpdateFontList()
{
    gfxPlatformFontList::PlatformFontList()->UpdateFontList();

    return NS_OK;
}

static const char kFontArabicTypesetting[] = "Arabic Typesetting";
static const char kFontArial[] = "Arial";
static const char kFontArialUnicodeMS[] = "Arial Unicode MS";
static const char kFontCambria[] = "Cambria";
static const char kFontCambriaMath[] = "Cambria Math";
static const char kFontEbrima[] = "Ebrima";
static const char kFontEstrangeloEdessa[] = "Estrangelo Edessa";
static const char kFontEuphemia[] = "Euphemia";
static const char kFontGabriola[] = "Gabriola";
static const char kFontKhmerUI[] = "Khmer UI";
static const char kFontLaoUI[] = "Lao UI";
static const char kFontLucidaSansUnicode[] = "Lucida Sans Unicode";
static const char kFontMVBoli[] = "MV Boli";
static const char kFontMalgunGothic[] = "Malgun Gothic";
static const char kFontMicrosoftJhengHei[] = "Microsoft JhengHei";
static const char kFontMicrosoftNewTaiLue[] = "Microsoft New Tai Lue";
static const char kFontMicrosoftPhagsPa[] = "Microsoft PhagsPa";
static const char kFontMicrosoftTaiLe[] = "Microsoft Tai Le";
static const char kFontMicrosoftUighur[] = "Microsoft Uighur";
static const char kFontMicrosoftYaHei[] = "Microsoft YaHei";
static const char kFontMicrosoftYiBaiti[] = "Microsoft Yi Baiti";
static const char kFontMeiryo[] = "Meiryo";
static const char kFontMongolianBaiti[] = "Mongolian Baiti";
static const char kFontNyala[] = "Nyala";
static const char kFontPlantagenetCherokee[] = "Plantagenet Cherokee";
static const char kFontSegoeUI[] = "Segoe UI";
static const char kFontSegoeUISymbol[] = "Segoe UI Symbol";
static const char kFontSylfaen[] = "Sylfaen";
static const char kFontTraditionalArabic[] = "Traditional Arabic";

void
gfxWindowsPlatform::GetCommonFallbackFonts(const uint32_t aCh,
                                           int32_t aRunScript,
                                           nsTArray<const char*>& aFontList)
{
    // Arial is used as the default fallback for system fallback
    aFontList.AppendElement(kFontArial);

    if (!IS_IN_BMP(aCh)) {
        uint32_t p = aCh >> 16;
        if (p == 1) { // SMP plane
            aFontList.AppendElement(kFontSegoeUISymbol);
            aFontList.AppendElement(kFontEbrima);
            aFontList.AppendElement(kFontCambriaMath);
        }
    } else {
        uint32_t b = (aCh >> 8) & 0xff;

        switch (b) {
        case 0x05:
            aFontList.AppendElement(kFontEstrangeloEdessa);
            aFontList.AppendElement(kFontCambria);
            break;
        case 0x06:
            aFontList.AppendElement(kFontMicrosoftUighur);
            break;
        case 0x07:
            aFontList.AppendElement(kFontEstrangeloEdessa);
            aFontList.AppendElement(kFontMVBoli);
            aFontList.AppendElement(kFontEbrima);
            break;
        case 0x0e:
            aFontList.AppendElement(kFontLaoUI);
            break;
        case 0x12:
        case 0x13:
            aFontList.AppendElement(kFontNyala);
            aFontList.AppendElement(kFontPlantagenetCherokee);
            break;
        case 0x14:
        case 0x15:
        case 0x16:
            aFontList.AppendElement(kFontEuphemia);
            aFontList.AppendElement(kFontSegoeUISymbol);
            break;
        case 0x17:
            aFontList.AppendElement(kFontKhmerUI);
            break;
        case 0x18:  // Mongolian
            aFontList.AppendElement(kFontMongolianBaiti);
            break;
        case 0x19:
            aFontList.AppendElement(kFontMicrosoftTaiLe);
            aFontList.AppendElement(kFontMicrosoftNewTaiLue);
            aFontList.AppendElement(kFontKhmerUI);
            break;
            break;
        case 0x20:  // Symbol ranges
        case 0x21:
        case 0x22:
        case 0x23:
        case 0x24:
        case 0x25:
        case 0x26:
        case 0x27:
        case 0x29:
        case 0x2a:
        case 0x2b:
        case 0x2c:
            aFontList.AppendElement(kFontSegoeUI);
            aFontList.AppendElement(kFontSegoeUISymbol);
            aFontList.AppendElement(kFontCambria);
            aFontList.AppendElement(kFontMeiryo);
            aFontList.AppendElement(kFontArial);
            aFontList.AppendElement(kFontLucidaSansUnicode);
            aFontList.AppendElement(kFontEbrima);
            break;
        case 0x2d:
        case 0x2e:
        case 0x2f:
            aFontList.AppendElement(kFontEbrima);
            aFontList.AppendElement(kFontNyala);
            aFontList.AppendElement(kFontMeiryo);
            break;
        case 0x28:  // Braille
            aFontList.AppendElement(kFontSegoeUISymbol);
            break;
        case 0x30:
        case 0x31:
            aFontList.AppendElement(kFontMicrosoftYaHei);
            break;
        case 0x32:
            aFontList.AppendElement(kFontMalgunGothic);
            break;
        case 0x4d:
            aFontList.AppendElement(kFontSegoeUISymbol);
            break;
        case 0xa0:  // Yi
        case 0xa1:
        case 0xa2:
        case 0xa3:
        case 0xa4:
            aFontList.AppendElement(kFontMicrosoftYiBaiti);
            break;
        case 0xa5:
        case 0xa6:
        case 0xa7:
            aFontList.AppendElement(kFontEbrima);
            aFontList.AppendElement(kFontCambriaMath);
            break;
        case 0xa8:
             aFontList.AppendElement(kFontMicrosoftPhagsPa);
             break;
        case 0xfb:
            aFontList.AppendElement(kFontMicrosoftUighur);
            aFontList.AppendElement(kFontGabriola);
            aFontList.AppendElement(kFontSylfaen);
            break;
        case 0xfc:
        case 0xfd:
            aFontList.AppendElement(kFontTraditionalArabic);
            aFontList.AppendElement(kFontArabicTypesetting);
            break;
        case 0xfe:
            aFontList.AppendElement(kFontTraditionalArabic);
            aFontList.AppendElement(kFontMicrosoftJhengHei);
           break;
       case 0xff:
            aFontList.AppendElement(kFontMicrosoftJhengHei);
            break;
        default:
            break;
        }
    }

    // Arial Unicode MS has lots of glyphs for obscure characters,
    // use it as a last resort
    aFontList.AppendElement(kFontArialUnicodeMS);
}

struct ResolveData {
    ResolveData(gfxPlatform::FontResolverCallback aCallback,
                gfxWindowsPlatform *aCaller, const nsAString *aFontName,
                void *aClosure) :
        mFoundCount(0), mCallback(aCallback), mCaller(aCaller),
        mFontName(aFontName), mClosure(aClosure) {}
    uint32_t mFoundCount;
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
                                     const uint8_t *aFontData, uint32_t aLength)
{
    return gfxPlatformFontList::PlatformFontList()->MakePlatformFont(aProxyEntry,
                                                                     aFontData,
                                                                     aLength);
}

bool
gfxWindowsPlatform::IsFontFormatSupported(nsIURI *aFontURI, uint32_t aFormatFlags)
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
        return nullptr;

    bool aNeedsBold;
    return ff->FindFontForStyle(aFontStyle, aNeedsBold);
}

qcms_profile*
gfxWindowsPlatform::GetPlatformCMSOutputProfile()
{
    WCHAR str[MAX_PATH];
    DWORD size = MAX_PATH;
    BOOL res;

    HDC dc = GetDC(nullptr);
    if (!dc)
        return nullptr;

#if _MSC_VER
    __try {
        res = GetICMProfileW(dc, &size, (LPWSTR)&str);
    } __except(GetExceptionCode() == EXCEPTION_ILLEGAL_INSTRUCTION) {
        res = FALSE;
    }
#else
    res = GetICMProfileW(dc, &size, (LPWSTR)&str);
#endif

    ReleaseDC(nullptr, dc);
    if (!res)
        return nullptr;

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

int32_t
gfxWindowsPlatform::WindowsOSVersion(int32_t *aBuildNum)
{
    static int32_t winVersion = UNINITIALIZED_VALUE;
    static int32_t buildNum = UNINITIALIZED_VALUE;

    OSVERSIONINFO vinfo;

    if (winVersion == UNINITIALIZED_VALUE) {
        vinfo.dwOSVersionInfoSize = sizeof (vinfo);
        if (!GetVersionEx(&vinfo)) {
            winVersion = kWindowsUnknown;
            buildNum = 0;
        } else {
            winVersion = int32_t(vinfo.dwMajorVersion << 16) + vinfo.dwMinorVersion;
            buildNum = int32_t(vinfo.dwBuildNumber);
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
        !versionInfo.AppendElements(uint32_t(versInfoSize)))
    {
        return;
    }

    if (!GetFileVersionInfoW(aDLLPath, 0, versInfoSize, 
           LPBYTE(versionInfo.Elements())))
    {
        return;
    } 

    UINT len = 0;
    VS_FIXEDFILEINFO *fileInfo = nullptr;
    if (!VerQueryValue(LPBYTE(versionInfo.Elements()), TEXT("\\"),
           (LPVOID *)&fileInfo, &len) ||
        len == 0 ||
        fileInfo == nullptr)
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
    }
}

#define ENHANCED_CONTRAST_REGISTRY_KEY \
    HKEY_CURRENT_USER, "Software\\Microsoft\\Avalon.Graphics\\DISPLAY1\\EnhancedContrastLevel"

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
        int32_t value;
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

        nsRefPtr<IDWriteRenderingParams> defaultRenderingParams;
        GetDWriteFactory()->CreateRenderingParams(getter_AddRefs(defaultRenderingParams));
        // For EnhancedContrast, we override the default if the user has not set it
        // in the registry (by using the ClearType Tuner).
        if (contrast >= 0.0 && contrast <= 10.0) {
	    contrast = contrast;
        } else {
            HKEY hKey;
            if (RegOpenKeyExA(ENHANCED_CONTRAST_REGISTRY_KEY,
                              0, KEY_READ, &hKey) == ERROR_SUCCESS)
            {
                contrast = defaultRenderingParams->GetEnhancedContrast();
                RegCloseKey(hKey);
            } else {
                contrast = 1.0;
            }
        }

        // For parameters that have not been explicitly set,
        // we copy values from default params (or our overridden value for contrast)
        if (gamma < 1.0 || gamma > 2.2) {
            gamma = defaultRenderingParams->GetGamma();
        }

        if (level < 0.0 || level > 1.0) {
            level = defaultRenderingParams->GetClearTypeLevel();
        }

        DWRITE_PIXEL_GEOMETRY dwriteGeometry =
          static_cast<DWRITE_PIXEL_GEOMETRY>(geometry);
        DWRITE_RENDERING_MODE renderMode =
          static_cast<DWRITE_RENDERING_MODE>(mode);

        if (dwriteGeometry < DWRITE_PIXEL_GEOMETRY_FLAT ||
            dwriteGeometry > DWRITE_PIXEL_GEOMETRY_BGR) {
            dwriteGeometry = defaultRenderingParams->GetPixelGeometry();
        }

        if (renderMode < DWRITE_RENDERING_MODE_DEFAULT ||
            renderMode > DWRITE_RENDERING_MODE_CLEARTYPE_NATURAL_SYMMETRIC) {
            renderMode = defaultRenderingParams->GetRenderingMode();
        }

        mRenderingParams[TEXT_RENDERING_NO_CLEARTYPE] = defaultRenderingParams;

        GetDWriteFactory()->CreateCustomRenderingParams(gamma, contrast, level,
	    dwriteGeometry, renderMode,
            getter_AddRefs(mRenderingParams[TEXT_RENDERING_NORMAL]));

        GetDWriteFactory()->CreateCustomRenderingParams(gamma, contrast, level,
	    dwriteGeometry, DWRITE_RENDERING_MODE_CLEARTYPE_GDI_CLASSIC,
            getter_AddRefs(mRenderingParams[TEXT_RENDERING_GDI_CLASSIC]));
    }
#endif
}

bool
gfxWindowsPlatform::IsOptimus()
{
  return GetModuleHandleA("nvumdshim.dll");
}
