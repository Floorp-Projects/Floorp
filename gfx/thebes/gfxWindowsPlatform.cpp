/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxWindowsPlatform.h"

#include "cairo.h"
#include "mozilla/ArrayUtils.h"

#include "gfxImageSurface.h"
#include "gfxWindowsSurface.h"

#include "nsUnicharUtils.h"

#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "mozilla/Snprintf.h"
#include "mozilla/WindowsVersion.h"
#include "nsServiceManagerUtils.h"
#include "nsTArray.h"
#include "mozilla/Telemetry.h"
#include "GeckoProfiler.h"

#include "nsIWindowsRegKey.h"
#include "nsIFile.h"
#include "plbase64.h"
#include "nsIXULRuntime.h"
#include "imgLoader.h"

#include "nsIGfxInfo.h"
#include "GfxDriverInfo.h"

#include "gfxCrashReporterUtils.h"

#include "gfxGDIFontList.h"
#include "gfxGDIFont.h"

#include "mozilla/layers/CompositorBridgeParent.h"   // for CompositorBridgeParent::IsInCompositorThread
#include "DeviceManagerD3D9.h"
#include "mozilla/layers/ReadbackManagerD3D11.h"

#include "WinUtils.h"

#include "gfxDWriteFontList.h"
#include "gfxDWriteFonts.h"
#include "gfxDWriteCommon.h"
#include <dwrite.h>

#include "gfxTextRun.h"
#include "gfxUserFontSet.h"
#include "nsWindowsHelpers.h"
#include "gfx2DGlue.h"

#include <string>

#include <d3d10_1.h>

#include "mozilla/gfx/2D.h"

#include "nsMemory.h"

#include <d3d11.h>

#include "nsIMemoryReporter.h"
#include <winternl.h>
#include "d3dkmtQueryStatistics.h"

#include "base/thread.h"
#include "SurfaceCache.h"
#include "gfxPrefs.h"
#include "gfxConfig.h"
#include "VsyncSource.h"
#include "DriverCrashGuard.h"
#include "mozilla/dom/ContentParent.h"

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::layers;
using namespace mozilla::widget;
using namespace mozilla::image;

enum class TelemetryDeviceCode : uint32_t {
  Content = 0,
  Image = 1,
  D2D1 = 2
};

DCFromDrawTarget::DCFromDrawTarget(DrawTarget& aDrawTarget)
{
  mDC = nullptr;
  if (aDrawTarget.GetBackendType() == BackendType::CAIRO) {
    cairo_t* ctx = static_cast<cairo_t*>
      (aDrawTarget.GetNativeSurface(NativeSurfaceType::CAIRO_CONTEXT));
    if (ctx) {
      cairo_surface_t* surf = cairo_get_group_target(ctx);
      if (surf) {
        cairo_surface_type_t surfaceType = cairo_surface_get_type(surf);
        if (surfaceType == CAIRO_SURFACE_TYPE_WIN32 ||
            surfaceType == CAIRO_SURFACE_TYPE_WIN32_PRINTING) {
          mDC = cairo_win32_surface_get_dc(surf);
          mNeedsRelease = false;
          SaveDC(mDC);
          cairo_scaled_font_t* scaled = cairo_get_scaled_font(ctx);
          cairo_win32_scaled_font_select_font(scaled, mDC);
        }
      }
    }
  }

  if (!mDC) {
    // Get the whole screen DC:
    mDC = GetDC(nullptr);
    SetGraphicsMode(mDC, GM_ADVANCED);
    mNeedsRelease = true;
  }
}

static const char *kFeatureLevelPref =
  "gfx.direct3d.last_used_feature_level_idx";
static const int kSupportedFeatureLevels[] =
  { D3D10_FEATURE_LEVEL_10_1, D3D10_FEATURE_LEVEL_10_0 };


class GfxD2DVramReporter final : public nsIMemoryReporter
{
    ~GfxD2DVramReporter() {}

public:
    NS_DECL_ISUPPORTS

    NS_IMETHOD CollectReports(nsIHandleReportCallback* aHandleReport,
                              nsISupports* aData, bool aAnonymize)
    {
        nsresult rv;

        rv = MOZ_COLLECT_REPORT(
            "gfx-d2d-vram-draw-target", KIND_OTHER, UNITS_BYTES,
            Factory::GetD2DVRAMUsageDrawTarget(),
            "Video memory used by D2D DrawTargets.");
        NS_ENSURE_SUCCESS(rv, rv);

        rv = MOZ_COLLECT_REPORT(
            "gfx-d2d-vram-source-surface", KIND_OTHER, UNITS_BYTES,
            Factory::GetD2DVRAMUsageSourceSurface(),
            "Video memory used by D2D SourceSurfaces.");
        NS_ENSURE_SUCCESS(rv, rv);

        return NS_OK;
    }
};

NS_IMPL_ISUPPORTS(GfxD2DVramReporter, nsIMemoryReporter)

#define GFX_USE_CLEARTYPE_ALWAYS "gfx.font_rendering.cleartype.always_use_for_content"
#define GFX_DOWNLOADABLE_FONTS_USE_CLEARTYPE "gfx.font_rendering.cleartype.use_for_downloadable_fonts"

#define GFX_CLEARTYPE_PARAMS           "gfx.font_rendering.cleartype_params."
#define GFX_CLEARTYPE_PARAMS_GAMMA     "gfx.font_rendering.cleartype_params.gamma"
#define GFX_CLEARTYPE_PARAMS_CONTRAST  "gfx.font_rendering.cleartype_params.enhanced_contrast"
#define GFX_CLEARTYPE_PARAMS_LEVEL     "gfx.font_rendering.cleartype_params.cleartype_level"
#define GFX_CLEARTYPE_PARAMS_STRUCTURE "gfx.font_rendering.cleartype_params.pixel_structure"
#define GFX_CLEARTYPE_PARAMS_MODE      "gfx.font_rendering.cleartype_params.rendering_mode"

class GPUAdapterReporter final : public nsIMemoryReporter
{
    // Callers must Release the DXGIAdapter after use or risk mem-leak
    static bool GetDXGIAdapter(IDXGIAdapter **aDXGIAdapter)
    {
        ID3D11Device *d3d11Device;
        IDXGIDevice *dxgiDevice;
        bool result = false;

        if ((d3d11Device = mozilla::gfx::Factory::GetDirect3D11Device())) {
            if (d3d11Device->QueryInterface(__uuidof(IDXGIDevice), (void **)&dxgiDevice) == S_OK) {
                result = (dxgiDevice->GetAdapter(aDXGIAdapter) == S_OK);
                dxgiDevice->Release();
            }
        }

        return result;
    }

    ~GPUAdapterReporter() {}

public:
    NS_DECL_ISUPPORTS

    NS_IMETHOD
    CollectReports(nsIMemoryReporterCallback* aCb,
                   nsISupports* aClosure, bool aAnonymize)
    {
        HANDLE ProcessHandle = GetCurrentProcess();

        int64_t dedicatedBytesUsed = 0;
        int64_t sharedBytesUsed = 0;
        int64_t committedBytesUsed = 0;
        IDXGIAdapter *DXGIAdapter;

        HMODULE gdi32Handle;
        PFND3DKMTQS queryD3DKMTStatistics = nullptr;

        // GPU memory reporting is not available before Windows 7
        if (!IsWin7OrLater())
            return NS_OK;

        if ((gdi32Handle = LoadLibrary(TEXT("gdi32.dll"))))
            queryD3DKMTStatistics = (PFND3DKMTQS)GetProcAddress(gdi32Handle, "D3DKMTQueryStatistics");

        if (queryD3DKMTStatistics && GetDXGIAdapter(&DXGIAdapter)) {
            // Most of this block is understood thanks to wj32's work on Process Hacker

            DXGI_ADAPTER_DESC adapterDesc;
            D3DKMTQS queryStatistics;

            DXGIAdapter->GetDesc(&adapterDesc);
            DXGIAdapter->Release();

            memset(&queryStatistics, 0, sizeof(D3DKMTQS));
            queryStatistics.Type = D3DKMTQS_PROCESS;
            queryStatistics.AdapterLuid = adapterDesc.AdapterLuid;
            queryStatistics.hProcess = ProcessHandle;
            if (NT_SUCCESS(queryD3DKMTStatistics(&queryStatistics))) {
                committedBytesUsed = queryStatistics.QueryResult.ProcessInfo.SystemMemory.BytesAllocated;
            }

            memset(&queryStatistics, 0, sizeof(D3DKMTQS));
            queryStatistics.Type = D3DKMTQS_ADAPTER;
            queryStatistics.AdapterLuid = adapterDesc.AdapterLuid;
            if (NT_SUCCESS(queryD3DKMTStatistics(&queryStatistics))) {
                ULONG i;
                ULONG segmentCount = queryStatistics.QueryResult.AdapterInfo.NbSegments;

                for (i = 0; i < segmentCount; i++) {
                    memset(&queryStatistics, 0, sizeof(D3DKMTQS));
                    queryStatistics.Type = D3DKMTQS_SEGMENT;
                    queryStatistics.AdapterLuid = adapterDesc.AdapterLuid;
                    queryStatistics.QuerySegment.SegmentId = i;

                    if (NT_SUCCESS(queryD3DKMTStatistics(&queryStatistics))) {
                        bool aperture;

                        // SegmentInformation has a different definition in Win7 than later versions
                        if (!IsWin8OrLater())
                            aperture = queryStatistics.QueryResult.SegmentInfoWin7.Aperture;
                        else
                            aperture = queryStatistics.QueryResult.SegmentInfoWin8.Aperture;

                        memset(&queryStatistics, 0, sizeof(D3DKMTQS));
                        queryStatistics.Type = D3DKMTQS_PROCESS_SEGMENT;
                        queryStatistics.AdapterLuid = adapterDesc.AdapterLuid;
                        queryStatistics.hProcess = ProcessHandle;
                        queryStatistics.QueryProcessSegment.SegmentId = i;
                        if (NT_SUCCESS(queryD3DKMTStatistics(&queryStatistics))) {
                            ULONGLONG bytesCommitted;
                            if (!IsWin8OrLater())
                                bytesCommitted = queryStatistics.QueryResult.ProcessSegmentInfo.Win7.BytesCommitted;
                            else
                                bytesCommitted = queryStatistics.QueryResult.ProcessSegmentInfo.Win8.BytesCommitted;
                            if (aperture)
                                sharedBytesUsed += bytesCommitted;
                            else
                                dedicatedBytesUsed += bytesCommitted;
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
                         KIND_OTHER, UNITS_BYTES, _amount,                    \
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

NS_IMPL_ISUPPORTS(GPUAdapterReporter, nsIMemoryReporter)


Atomic<size_t> gfxWindowsPlatform::sD3D11MemoryUsed;

class D3D11TextureReporter final : public nsIMemoryReporter
{
  ~D3D11TextureReporter() {}

public:
  NS_DECL_ISUPPORTS

  NS_IMETHOD CollectReports(nsIHandleReportCallback *aHandleReport,
                            nsISupports* aData, bool aAnonymize) override
  {
      return MOZ_COLLECT_REPORT("d3d11-shared-textures", KIND_OTHER, UNITS_BYTES,
                                gfxWindowsPlatform::sD3D11MemoryUsed,
                                "Memory used for D3D11 shared textures");
  }
};

NS_IMPL_ISUPPORTS(D3D11TextureReporter, nsIMemoryReporter)

Atomic<size_t> gfxWindowsPlatform::sD3D9MemoryUsed;

class D3D9TextureReporter final : public nsIMemoryReporter
{
  ~D3D9TextureReporter() {}

public:
  NS_DECL_ISUPPORTS

  NS_IMETHOD CollectReports(nsIHandleReportCallback *aHandleReport,
                            nsISupports* aData, bool aAnonymize) override
  {
    return MOZ_COLLECT_REPORT("d3d9-shared-textures", KIND_OTHER, UNITS_BYTES,
                              gfxWindowsPlatform::sD3D9MemoryUsed,
                              "Memory used for D3D9 shared textures");
  }
};

NS_IMPL_ISUPPORTS(D3D9TextureReporter, nsIMemoryReporter)

Atomic<size_t> gfxWindowsPlatform::sD3D9SharedTextureUsed;

class D3D9SharedTextureReporter final : public nsIMemoryReporter
{
  ~D3D9SharedTextureReporter() {}

public:
  NS_DECL_ISUPPORTS

  NS_IMETHOD CollectReports(nsIHandleReportCallback *aHandleReport,
                            nsISupports* aData, bool aAnonymize) override
  {
    return MOZ_COLLECT_REPORT("d3d9-shared-texture", KIND_OTHER, UNITS_BYTES,
                              gfxWindowsPlatform::sD3D9SharedTextureUsed,
                              "Memory used for D3D9 shared textures");
  }
};

NS_IMPL_ISUPPORTS(D3D9SharedTextureReporter, nsIMemoryReporter)

gfxWindowsPlatform::gfxWindowsPlatform()
  : mRenderMode(RENDER_GDI)
  , mDeviceLock("gfxWindowsPlatform.mDeviceLock")
  , mIsWARP(false)
  , mHasDeviceReset(false)
  , mHasFakeDeviceReset(false)
  , mCompositorD3D11TextureSharingWorks(false)
  , mHasD3D9DeviceReset(false)
{
  mUseClearTypeForDownloadableFonts = UNINITIALIZED_VALUE;
  mUseClearTypeAlways = UNINITIALIZED_VALUE;

  /* 
   * Initialize COM 
   */ 
  CoInitialize(nullptr); 

  RegisterStrongMemoryReporter(new GfxD2DVramReporter());

  RegisterStrongMemoryReporter(new GPUAdapterReporter());
  RegisterStrongMemoryReporter(new D3D11TextureReporter());
  RegisterStrongMemoryReporter(new D3D9TextureReporter());
  RegisterStrongMemoryReporter(new D3D9SharedTextureReporter());
}

gfxWindowsPlatform::~gfxWindowsPlatform()
{
  mDeviceManager = nullptr;
  mD3D11Device = nullptr;
  mD3D11ContentDevice = nullptr;
  mD3D11ImageBridgeDevice = nullptr;

  mozilla::gfx::Factory::D2DCleanup();

  mAdapter = nullptr;

  /* 
   * Uninitialize COM 
   */ 
  CoUninitialize();
}

void
gfxWindowsPlatform::InitAcceleration()
{
  gfxPlatform::InitAcceleration();

  // Set up the D3D11 feature levels we can ask for.
  if (IsWin8OrLater()) {
    mFeatureLevels.AppendElement(D3D_FEATURE_LEVEL_11_1);
  }
  mFeatureLevels.AppendElement(D3D_FEATURE_LEVEL_11_0);
  mFeatureLevels.AppendElement(D3D_FEATURE_LEVEL_10_1);
  mFeatureLevels.AppendElement(D3D_FEATURE_LEVEL_10_0);
  mFeatureLevels.AppendElement(D3D_FEATURE_LEVEL_9_3);

  InitializeConfig();
  InitializeDevices();
  UpdateRenderMode();
}

bool
gfxWindowsPlatform::CanUseHardwareVideoDecoding()
{
  if (!gfxPrefs::LayersPreferD3D9() && !mCompositorD3D11TextureSharingWorks) {
    return false;
  }
  return !IsWARP() && gfxPlatform::CanUseHardwareVideoDecoding();
}

bool
gfxWindowsPlatform::InitDWriteSupport()
{
  MOZ_ASSERT(!mDWriteFactory && IsVistaOrLater());

  mozilla::ScopedGfxFeatureReporter reporter("DWrite");
  decltype(DWriteCreateFactory)* createDWriteFactory = (decltype(DWriteCreateFactory)*)
      GetProcAddress(LoadLibraryW(L"dwrite.dll"), "DWriteCreateFactory");
  if (!createDWriteFactory) {
    return false;
  }

  // I need a direct pointer to be able to cast to IUnknown**, I also need to
  // remember to release this because the nsRefPtr will AddRef it.
  RefPtr<IDWriteFactory> factory;
  HRESULT hr = createDWriteFactory(
      DWRITE_FACTORY_TYPE_SHARED,
      __uuidof(IDWriteFactory),
      (IUnknown **)((IDWriteFactory **)getter_AddRefs(factory)));
  if (FAILED(hr) || !factory) {
    return false;
  }

  mDWriteFactory = factory;

  SetupClearTypeParams();
  reporter.SetSuccessful();
  return true;
}

bool
gfxWindowsPlatform::HandleDeviceReset()
{
  DeviceResetReason resetReason = DeviceResetReason::OK;
  if (!DidRenderingDeviceReset(&resetReason)) {
    return false;
  }

  if (!mHasFakeDeviceReset) {
    Telemetry::Accumulate(Telemetry::DEVICE_RESET_REASON, uint32_t(resetReason));
  }

  // Remove devices and adapters.
  ResetD3D11Devices();
  mAdapter = nullptr;

  // Reset local state. Note: we leave feature status variables as-is. They
  // will be recomputed by InitializeDevices().
  mHasDeviceReset = false;
  mHasFakeDeviceReset = false;
  mHasD3D9DeviceReset = false;
  mCompositorD3D11TextureSharingWorks = false;
  mDeviceResetReason = DeviceResetReason::OK;

  imgLoader::Singleton()->ClearCache(true);
  imgLoader::Singleton()->ClearCache(false);
  gfxAlphaBoxBlur::ShutdownBlurCache();

  InitializeDevices();
  BumpDeviceCounter();
  return true;
}

static const BackendType SOFTWARE_BACKEND = BackendType::CAIRO;

void
gfxWindowsPlatform::UpdateBackendPrefs()
{
  uint32_t canvasMask = BackendTypeBit(SOFTWARE_BACKEND);
  uint32_t contentMask = BackendTypeBit(SOFTWARE_BACKEND);
  BackendType defaultBackend = SOFTWARE_BACKEND;
  if (gfxConfig::IsEnabled(Feature::DIRECT2D)) {
    MOZ_RELEASE_ASSERT(Factory::GetDirect3D11Device());
    MOZ_RELEASE_ASSERT(Factory::GetD2D1Device());
    MOZ_RELEASE_ASSERT(Factory::SupportsD2D1());

    contentMask |= BackendTypeBit(BackendType::DIRECT2D1_1);
    canvasMask |= BackendTypeBit(BackendType::DIRECT2D1_1);
    defaultBackend = BackendType::DIRECT2D1_1;
  } else {
    canvasMask |= BackendTypeBit(BackendType::SKIA);
  }
  contentMask |= BackendTypeBit(BackendType::SKIA);
  InitBackendPrefs(canvasMask, defaultBackend, contentMask, defaultBackend);
}

bool
gfxWindowsPlatform::IsDirect2DBackend()
{
  return GetDefaultContentBackend() == BackendType::DIRECT2D1_1;
}

void
gfxWindowsPlatform::UpdateRenderMode()
{
  bool didReset = HandleDeviceReset();

  UpdateBackendPrefs();

  if (didReset) {
    mScreenReferenceDrawTarget =
      CreateOffscreenContentDrawTarget(IntSize(1, 1), SurfaceFormat::B8G8R8A8);
    if (!mScreenReferenceDrawTarget) {
      gfxCriticalNote << "Failed to update reference draw target after device reset"
                      << ", D3D11 device:" << hexa(Factory::GetDirect3D11Device())
                      << ", D3D11 status:" << FeatureStatusToString(gfxConfig::GetValue(Feature::D3D11_COMPOSITING))
                      << ", D2D1 device:" << hexa(Factory::GetD2D1Device())
                      << ", D2D1 status:" << FeatureStatusToString(gfxConfig::GetValue(Feature::DIRECT2D))
                      << ", content:" << int(GetDefaultContentBackend())
                      << ", compositor:" << int(GetCompositorBackend());
      MOZ_CRASH("GFX: Failed to update reference draw target after device reset");
    }
  }
}

void
gfxWindowsPlatform::ForceDeviceReset(ForcedDeviceResetReason aReason)
{
  Telemetry::Accumulate(Telemetry::FORCED_DEVICE_RESET_REASON, uint32_t(aReason));

  mDeviceResetReason = DeviceResetReason::FORCED_RESET;
  mHasDeviceReset = true;
}

mozilla::gfx::BackendType
gfxWindowsPlatform::GetContentBackendFor(mozilla::layers::LayersBackend aLayers)
{
  if (aLayers == LayersBackend::LAYERS_D3D11) {
    return gfxPlatform::GetDefaultContentBackend();
  }

  // If we're not accelerated with D3D11, never use D2D.
  return SOFTWARE_BACKEND;
}

gfxPlatformFontList*
gfxWindowsPlatform::CreatePlatformFontList()
{
    gfxPlatformFontList *pfl;

    // bug 630201 - older pre-RTM versions of Direct2D/DirectWrite cause odd
    // crashers so blacklist them altogether
    if (IsNotWin7PreRTM() && GetDWriteFactory()) {
        pfl = new gfxDWriteFontList();
        if (NS_SUCCEEDED(pfl->InitFontList())) {
            return pfl;
        }
        // DWrite font initialization failed! Don't know why this would happen,
        // but apparently it can - see bug 594865.
        // So we're going to fall back to GDI fonts & rendering.
        gfxPlatformFontList::Shutdown();
        DisableD2D(FeatureStatus::Failed, "Failed to initialize fonts");
    }

    pfl = new gfxGDIFontList();

    if (NS_SUCCEEDED(pfl->InitFontList())) {
        return pfl;
    }

    gfxPlatformFontList::Shutdown();
    return nullptr;
}

// This function will permanently disable D2D for the session. It's intended to
// be used when, after initially chosing to use Direct2D, we encounter a
// scenario we can't support.
//
// This is called during gfxPlatform::Init() so at this point there should be no
// DrawTargetD2D/1 instances.
void
gfxWindowsPlatform::DisableD2D(FeatureStatus aStatus, const char* aMessage)
{
  gfxConfig::SetFailed(Feature::DIRECT2D, aStatus, aMessage);
  Factory::SetDirect3D11Device(nullptr);
  UpdateBackendPrefs();
}

already_AddRefed<gfxASurface>
gfxWindowsPlatform::CreateOffscreenSurface(const IntSize& aSize,
                                           gfxImageFormat aFormat)
{
    RefPtr<gfxASurface> surf = nullptr;

#ifdef CAIRO_HAS_WIN32_SURFACE
    if (mRenderMode == RENDER_GDI || mRenderMode == RENDER_DIRECT2D)
        surf = new gfxWindowsSurface(aSize, aFormat);
#endif

    if (!surf || surf->CairoStatus()) {
        surf = new gfxImageSurface(aSize, aFormat);
    }

    return surf.forget();
}

already_AddRefed<ScaledFont>
gfxWindowsPlatform::GetScaledFontForFont(DrawTarget* aTarget, gfxFont *aFont)
{
    if (aFont->GetType() == gfxFont::FONT_TYPE_DWRITE) {
        gfxDWriteFont *font = static_cast<gfxDWriteFont*>(aFont);

        NativeFont nativeFont;
        nativeFont.mType = NativeFontType::DWRITE_FONT_FACE;
        nativeFont.mFont = font->GetFontFace();

        if (aTarget->GetBackendType() == BackendType::CAIRO) {
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
    nativeFont.mType = NativeFontType::GDI_FONT_FACE;
    LOGFONT lf;
    GetObject(static_cast<gfxGDIFont*>(aFont)->GetHFONT(), sizeof(LOGFONT), &lf);
    nativeFont.mFont = &lf;

    if (aTarget->GetBackendType() == BackendType::CAIRO) {
      return Factory::CreateScaledFontWithCairo(nativeFont,
                                                aFont->GetAdjustedSize(),
                                                aFont->GetCairoScaledFont());
    }

    return Factory::CreateScaledFontForNativeFont(nativeFont, aFont->GetAdjustedSize());
}

static const char kFontAparajita[] = "Aparajita";
static const char kFontArabicTypesetting[] = "Arabic Typesetting";
static const char kFontArial[] = "Arial";
static const char kFontArialUnicodeMS[] = "Arial Unicode MS";
static const char kFontCambria[] = "Cambria";
static const char kFontCambriaMath[] = "Cambria Math";
static const char kFontEbrima[] = "Ebrima";
static const char kFontEstrangeloEdessa[] = "Estrangelo Edessa";
static const char kFontEuphemia[] = "Euphemia";
static const char kFontGabriola[] = "Gabriola";
static const char kFontJavaneseText[] = "Javanese Text";
static const char kFontKhmerUI[] = "Khmer UI";
static const char kFontLaoUI[] = "Lao UI";
static const char kFontLeelawadeeUI[] = "Leelawadee UI";
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
static const char kFontMyanmarText[] = "Myanmar Text";
static const char kFontNirmalaUI[] = "Nirmala UI";
static const char kFontNyala[] = "Nyala";
static const char kFontPlantagenetCherokee[] = "Plantagenet Cherokee";
static const char kFontSegoeUI[] = "Segoe UI";
static const char kFontSegoeUIEmoji[] = "Segoe UI Emoji";
static const char kFontSegoeUISymbol[] = "Segoe UI Symbol";
static const char kFontSylfaen[] = "Sylfaen";
static const char kFontTraditionalArabic[] = "Traditional Arabic";
static const char kFontUtsaah[] = "Utsaah";
static const char kFontYuGothic[] = "Yu Gothic";

void
gfxWindowsPlatform::GetCommonFallbackFonts(uint32_t aCh, uint32_t aNextCh,
                                           Script aRunScript,
                                           nsTArray<const char*>& aFontList)
{
    if (aNextCh == 0xfe0fu) {
        aFontList.AppendElement(kFontSegoeUIEmoji);
    }

    // Arial is used as the default fallback for system fallback
    aFontList.AppendElement(kFontArial);

    if (!IS_IN_BMP(aCh)) {
        uint32_t p = aCh >> 16;
        if (p == 1) { // SMP plane
            if (aNextCh == 0xfe0eu) {
                aFontList.AppendElement(kFontSegoeUISymbol);
                aFontList.AppendElement(kFontSegoeUIEmoji);
            } else {
                if (aNextCh != 0xfe0fu) {
                    aFontList.AppendElement(kFontSegoeUIEmoji);
                }
                aFontList.AppendElement(kFontSegoeUISymbol);
            }
            aFontList.AppendElement(kFontEbrima);
            aFontList.AppendElement(kFontNirmalaUI);
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
        case 0x09:
            aFontList.AppendElement(kFontNirmalaUI);
            aFontList.AppendElement(kFontUtsaah);
            aFontList.AppendElement(kFontAparajita);
            break;
        case 0x0e:
            aFontList.AppendElement(kFontLaoUI);
            break;
        case 0x10:
            aFontList.AppendElement(kFontMyanmarText);
            break;
        case 0x11:
            aFontList.AppendElement(kFontMalgunGothic);
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
            aFontList.AppendElement(kFontEuphemia);
            break;
        case 0x19:
            aFontList.AppendElement(kFontMicrosoftTaiLe);
            aFontList.AppendElement(kFontMicrosoftNewTaiLue);
            aFontList.AppendElement(kFontKhmerUI);
            break;
            break;
        case 0x1a:
            aFontList.AppendElement(kFontLeelawadeeUI);
            break;
        case 0x1c:
            aFontList.AppendElement(kFontNirmalaUI);
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
            aFontList.AppendElement(kFontSegoeUI);
            aFontList.AppendElement(kFontSegoeUISymbol);
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
        case 0x9f:
            aFontList.AppendElement(kFontMicrosoftYaHei);
            aFontList.AppendElement(kFontYuGothic);
            break;
        case 0xa0:  // Yi
        case 0xa1:
        case 0xa2:
        case 0xa3:
        case 0xa4:
            aFontList.AppendElement(kFontMicrosoftYiBaiti);
            aFontList.AppendElement(kFontSegoeUI);
            break;
        case 0xa5:
        case 0xa6:
        case 0xa7:
            aFontList.AppendElement(kFontEbrima);
            aFontList.AppendElement(kFontSegoeUI);
            aFontList.AppendElement(kFontCambriaMath);
            break;
        case 0xa8:
             aFontList.AppendElement(kFontMicrosoftPhagsPa);
             aFontList.AppendElement(kFontNirmalaUI);
             break;
        case 0xa9:
             aFontList.AppendElement(kFontMalgunGothic);
             aFontList.AppendElement(kFontJavaneseText);
             break;
        case 0xaa:
             aFontList.AppendElement(kFontMyanmarText);
             break;
        case 0xab:
             aFontList.AppendElement(kFontEbrima);
             aFontList.AppendElement(kFontNyala);
             break;
        case 0xd7:
             aFontList.AppendElement(kFontMalgunGothic);
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

gfxFontGroup *
gfxWindowsPlatform::CreateFontGroup(const FontFamilyList& aFontFamilyList,
                                    const gfxFontStyle *aStyle,
                                    gfxTextPerfMetrics* aTextPerf,
                                    gfxUserFontSet *aUserFontSet,
                                    gfxFloat aDevToCssSize)
{
    return new gfxFontGroup(aFontFamilyList, aStyle, aTextPerf,
                            aUserFontSet, aDevToCssSize);
}

bool
gfxWindowsPlatform::IsFontFormatSupported(nsIURI *aFontURI, uint32_t aFormatFlags)
{
    // check for strange format flags
    NS_ASSERTION(!(aFormatFlags & gfxUserFontSet::FLAG_FORMAT_NOT_USED),
                 "strange font format hint set");

    // accept supported formats
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

static DeviceResetReason HResultToResetReason(HRESULT hr)
{
  switch (hr) {
  case DXGI_ERROR_DEVICE_HUNG:
    return DeviceResetReason::HUNG;
  case DXGI_ERROR_DEVICE_REMOVED:
    return DeviceResetReason::REMOVED;
  case DXGI_ERROR_DEVICE_RESET:
    return DeviceResetReason::RESET;
  case DXGI_ERROR_DRIVER_INTERNAL_ERROR:
    return DeviceResetReason::DRIVER_ERROR;
  case DXGI_ERROR_INVALID_CALL:
    return DeviceResetReason::INVALID_CALL;
  case E_OUTOFMEMORY:
    return DeviceResetReason::OUT_OF_MEMORY;
  default:
    MOZ_ASSERT(false);
  }
  return DeviceResetReason::UNKNOWN;
}

bool
gfxWindowsPlatform::IsDeviceReset(HRESULT hr, DeviceResetReason* aResetReason)
{
  if (hr != S_OK) {
    mDeviceResetReason = HResultToResetReason(hr);
    mHasDeviceReset = true;
    if (aResetReason) {
      *aResetReason = mDeviceResetReason;
    }
    return true;
  }
  return false;
}

void
gfxWindowsPlatform::TestDeviceReset(DeviceResetReason aReason)
{
  if (mHasDeviceReset) {
    return;
  }
  mHasDeviceReset = true;
  mHasFakeDeviceReset = true;
  mDeviceResetReason = aReason;
}

bool
gfxWindowsPlatform::DidRenderingDeviceReset(DeviceResetReason* aResetReason)
{
  if (mHasDeviceReset) {
    if (aResetReason) {
      *aResetReason = mDeviceResetReason;
    }
    return true;
  }
  if (aResetReason) {
    *aResetReason = DeviceResetReason::OK;
  }

  if (mD3D11Device) {
    HRESULT hr = mD3D11Device->GetDeviceRemovedReason();
    if (IsDeviceReset(hr, aResetReason)) {
      return true;
    }
  }
  if (mD3D11ContentDevice) {
    HRESULT hr = mD3D11ContentDevice->GetDeviceRemovedReason();
    if (IsDeviceReset(hr, aResetReason)) {
      return true;
    }
  }
  if (mHasD3D9DeviceReset) {
    return true;
  }
  if (XRE_IsParentProcess() && gfxPrefs::DeviceResetForTesting()) {
    TestDeviceReset((DeviceResetReason)gfxPrefs::DeviceResetForTesting());
    if (aResetReason) {
      *aResetReason = mDeviceResetReason;
    }
    gfxPrefs::SetDeviceResetForTesting(0);
    return true;
  }
  return false;
}

BOOL CALLBACK
InvalidateWindowForDeviceReset(HWND aWnd, LPARAM aMsg)
{
    RedrawWindow(aWnd, nullptr, nullptr,
                 RDW_INVALIDATE|RDW_INTERNALPAINT|RDW_FRAME);
    return TRUE;
}

void
gfxWindowsPlatform::SchedulePaintIfDeviceReset()
{
  PROFILER_LABEL_FUNC(js::ProfileEntry::Category::GRAPHICS);

  if (!DidRenderingDeviceReset()) {
    return;
  }

  // Trigger an ::OnPaint for each window.
  ::EnumThreadWindows(GetCurrentThreadId(),
                      InvalidateWindowForDeviceReset,
                      0);

  gfxCriticalNote << "Detected rendering device reset on refresh";
}

void
gfxWindowsPlatform::UpdateRenderModeIfDeviceReset()
{
  PROFILER_LABEL_FUNC(js::ProfileEntry::Category::GRAPHICS);

  if (DidRenderingDeviceReset()) {
    UpdateRenderMode();
  }
}

void
gfxWindowsPlatform::GetPlatformCMSOutputProfile(void* &mem, size_t &mem_size)
{
    WCHAR str[MAX_PATH];
    DWORD size = MAX_PATH;
    BOOL res;

    mem = nullptr;
    mem_size = 0;

    HDC dc = GetDC(nullptr);
    if (!dc)
        return;

    MOZ_SEH_TRY {
        res = GetICMProfileW(dc, &size, (LPWSTR)&str);
    } MOZ_SEH_EXCEPT(GetExceptionCode() == EXCEPTION_ILLEGAL_INSTRUCTION) {
        res = FALSE;
    }

    ReleaseDC(nullptr, dc);
    if (!res)
        return;

#ifdef _WIN32
    qcms_data_from_unicode_path(str, &mem, &mem_size);

#ifdef DEBUG_tor
    if (mem_size > 0)
        fprintf(stderr,
                "ICM profile read from %s successfully\n",
                NS_ConvertUTF16toUTF8(str).get());
#endif // DEBUG_tor
#endif // _WIN32
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

void 
gfxWindowsPlatform::GetDLLVersion(char16ptr_t aDLLPath, nsAString& aVersion)
{
    DWORD versInfoSize, vers[4] = {0};
    // version info not available case
    aVersion.AssignLiteral(MOZ_UTF16("0.0.0.0"));
    versInfoSize = GetFileVersionInfoSizeW(aDLLPath, nullptr);
    AutoTArray<BYTE,512> versionInfo;
    
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
    snprintf_literal(buf, "%u.%u.%u.%u", vers[0], vers[1], vers[2], vers[3]);
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
        rv = RegEnumKeyExW(hKey, i, displayName, &size,
                           nullptr, nullptr, nullptr, nullptr);
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
            subrv = RegQueryValueExW(subKey, L"GammaLevel", nullptr, &type,
                                     (LPBYTE)&value, &size);
            if (subrv == ERROR_SUCCESS && type == REG_DWORD) {
                foundData = true;
                ctinfo.gamma = value;
            }

            size = sizeof(value);
            subrv = RegQueryValueExW(subKey, L"PixelStructure", nullptr, &type,
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
            subrv = RegQueryValueExW(subKey, L"ClearTypeLevel", nullptr, &type,
                                     (LPBYTE)&value, &size);
            if (subrv == ERROR_SUCCESS && type == REG_DWORD) {
                foundData = true;
                ctinfo.clearTypeLevel = value;
            }
      
            size = sizeof(value);
            subrv = RegQueryValueExW(subKey, L"EnhancedContrastLevel",
                                     nullptr, &type, (LPBYTE)&value, &size);
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

        RefPtr<IDWriteRenderingParams> defaultRenderingParams;
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
}

void
gfxWindowsPlatform::OnDeviceManagerDestroy(DeviceManagerD3D9* aDeviceManager)
{
  if (aDeviceManager == mDeviceManager) {
    MutexAutoLock lock(mDeviceLock);
    mDeviceManager = nullptr;
  }
}

IDirect3DDevice9*
gfxWindowsPlatform::GetD3D9Device()
{
  RefPtr<DeviceManagerD3D9> manager = GetD3D9DeviceManager();
  return manager ? manager->device() : nullptr;
}

void
gfxWindowsPlatform::D3D9DeviceReset() {
  mHasD3D9DeviceReset = true;
}

already_AddRefed<DeviceManagerD3D9>
gfxWindowsPlatform::GetD3D9DeviceManager()
{
  // We should only create the d3d9 device on the compositor thread
  // or we don't have a compositor thread.
  RefPtr<DeviceManagerD3D9> result;
  if (!mDeviceManager &&
      (!gfxPlatform::UsesOffMainThreadCompositing() ||
       CompositorBridgeParent::IsInCompositorThread())) {
    mDeviceManager = new DeviceManagerD3D9();
    if (!mDeviceManager->Init()) {
      gfxCriticalError() << "[D3D9] Could not Initialize the DeviceManagerD3D9";
      mDeviceManager = nullptr;
    }
  }

  MutexAutoLock lock(mDeviceLock);
  result = mDeviceManager;
  return result.forget();
}

bool
gfxWindowsPlatform::GetD3D11Device(RefPtr<ID3D11Device>* aOutDevice)
{
  MutexAutoLock lock(mDeviceLock);
  *aOutDevice = mD3D11Device;
  return !!mD3D11Device;
}

ID3D11Device*
gfxWindowsPlatform::GetD3D11ContentDevice()
{
  return mD3D11ContentDevice;
}

bool
gfxWindowsPlatform::GetD3D11ImageBridgeDevice(RefPtr<ID3D11Device>* aOutDevice)
{
  MutexAutoLock lock(mDeviceLock);
  *aOutDevice = mD3D11ImageBridgeDevice;
  return !!mD3D11ImageBridgeDevice;
}

bool
gfxWindowsPlatform::GetD3D11DeviceForCurrentThread(RefPtr<ID3D11Device>* aOutDevice)
{
  if (NS_IsMainThread()) {
    *aOutDevice = mD3D11ContentDevice;
    return !!mD3D11ContentDevice;
  }
  return GetD3D11ImageBridgeDevice(aOutDevice);
}

ReadbackManagerD3D11*
gfxWindowsPlatform::GetReadbackManager()
{
  if (!mD3D11ReadbackManager) {
    mD3D11ReadbackManager = new ReadbackManagerD3D11();
  }

  return mD3D11ReadbackManager;
}

bool
gfxWindowsPlatform::IsOptimus()
{
    static int knowIsOptimus = -1;
    if (knowIsOptimus == -1) {
        // other potential optimus -- nvd3d9wrapx.dll & nvdxgiwrap.dll
        if (GetModuleHandleA("nvumdshim.dll") ||
            GetModuleHandleA("nvumdshimx.dll"))
        {
            knowIsOptimus = 1;
        } else {
            knowIsOptimus = 0;
        }
    }
    return knowIsOptimus;
}

IDXGIAdapter1*
gfxWindowsPlatform::GetDXGIAdapter()
{
  if (mAdapter) {
    return mAdapter;
  }

  nsModuleHandle dxgiModule(LoadLibrarySystem32(L"dxgi.dll"));
  decltype(CreateDXGIFactory1)* createDXGIFactory1 = (decltype(CreateDXGIFactory1)*)
    GetProcAddress(dxgiModule, "CreateDXGIFactory1");

  if (!createDXGIFactory1) {
    return nullptr;
  }

  // Try to use a DXGI 1.1 adapter in order to share resources
  // across processes.
  RefPtr<IDXGIFactory1> factory1;
  HRESULT hr = createDXGIFactory1(__uuidof(IDXGIFactory1),
                                  getter_AddRefs(factory1));
  if (FAILED(hr) || !factory1) {
    // This seems to happen with some people running the iZ3D driver.
    // They won't get acceleration.
    return nullptr;
  }

  if (!XRE_IsContentProcess()) {
    // In the parent process, we pick the first adapter.
    if (FAILED(factory1->EnumAdapters1(0, getter_AddRefs(mAdapter)))) {
      return nullptr;
    }
  } else {
    const DxgiAdapterDesc& parent = GetParentDevicePrefs().adapter();

    // In the child process, we search for the adapter that matches the parent
    // process. The first adapter can be mismatched on dual-GPU systems.
    for (UINT index = 0; ; index++) {
      RefPtr<IDXGIAdapter1> adapter;
      if (FAILED(factory1->EnumAdapters1(index, getter_AddRefs(adapter)))) {
        break;
      }

      DXGI_ADAPTER_DESC desc;
      if (SUCCEEDED(adapter->GetDesc(&desc)) &&
          desc.AdapterLuid.HighPart == parent.AdapterLuid.HighPart &&
          desc.AdapterLuid.LowPart == parent.AdapterLuid.LowPart &&
          desc.VendorId == parent.VendorId &&
          desc.DeviceId == parent.DeviceId)
      {
        mAdapter = adapter.forget();
        break;
      }
    }
  }

  if (!mAdapter) {
    return nullptr;
  }

  // We leak this module everywhere, we might as well do so here as well.
  dxgiModule.disown();
  return mAdapter;
}

bool DoesD3D11DeviceWork()
{
  static bool checked = false;
  static bool result = false;

  if (checked)
      return result;
  checked = true;

  if (gfxPrefs::Direct2DForceEnabled() ||
      gfxConfig::IsForcedOnByUser(Feature::HW_COMPOSITING))
  {
    result = true;
    return true;
  }

  if (GetModuleHandleW(L"igd10umd32.dll")) {
    const wchar_t* checkModules[] = {L"dlumd32.dll",
                                     L"dlumd11.dll",
                                     L"dlumd10.dll"};
    for (int i=0; i<PR_ARRAY_SIZE(checkModules); i+=1) {
      if (GetModuleHandleW(checkModules[i])) {
        nsString displayLinkModuleVersionString;
        gfxWindowsPlatform::GetDLLVersion(checkModules[i],
                                          displayLinkModuleVersionString);
        uint64_t displayLinkModuleVersion;
        if (!ParseDriverVersion(displayLinkModuleVersionString,
                                &displayLinkModuleVersion)) {
          gfxCriticalError() << "DisplayLink: could not parse version "
                             << checkModules[i];
          return false;
        }
        if (displayLinkModuleVersion <= V(8,6,1,36484)) {
          gfxCriticalError(CriticalLog::DefaultOptions(false)) << "DisplayLink: too old version " << displayLinkModuleVersionString.get();
          return false;
        }
      }
    }
  }
  result = true;
  return true;
}

static bool
GetDxgiDesc(ID3D11Device* device, DXGI_ADAPTER_DESC* out)
{
  RefPtr<IDXGIDevice> dxgiDevice;
  HRESULT hr = device->QueryInterface(__uuidof(IDXGIDevice), getter_AddRefs(dxgiDevice));
  if (FAILED(hr)) {
    return false;
  }

  RefPtr<IDXGIAdapter> dxgiAdapter;
  if (FAILED(dxgiDevice->GetAdapter(getter_AddRefs(dxgiAdapter)))) {
    return false;
  }

  return SUCCEEDED(dxgiAdapter->GetDesc(out));
}

static void
CheckForAdapterMismatch(ID3D11Device *device)
{
  DXGI_ADAPTER_DESC desc;
  PodZero(&desc);
  GetDxgiDesc(device, &desc);

  nsCOMPtr<nsIGfxInfo> gfxInfo = services::GetGfxInfo();
  nsString vendorID;
  gfxInfo->GetAdapterVendorID(vendorID);
  nsresult ec;
  int32_t vendor = vendorID.ToInteger(&ec, 16);
  if (vendor != desc.VendorId) {
      gfxCriticalNote << "VendorIDMismatch V " << hexa(vendor) << " " << hexa(desc.VendorId);
  }
}

bool DoesRenderTargetViewNeedsRecreating(ID3D11Device *device)
{
    bool result = false;
    // CreateTexture2D is known to crash on lower feature levels, see bugs
    // 1170211 and 1089413.
    if (device->GetFeatureLevel() < D3D_FEATURE_LEVEL_10_0) {
        return true;
    }

    RefPtr<ID3D11DeviceContext> deviceContext;
    device->GetImmediateContext(getter_AddRefs(deviceContext));
    int backbufferWidth = 32; int backbufferHeight = 32;
    RefPtr<ID3D11Texture2D> offscreenTexture;
    RefPtr<IDXGIKeyedMutex> keyedMutex;

    D3D11_TEXTURE2D_DESC offscreenTextureDesc = { 0 };
    offscreenTextureDesc.Width = backbufferWidth;
    offscreenTextureDesc.Height = backbufferHeight;
    offscreenTextureDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    offscreenTextureDesc.MipLevels = 0;
    offscreenTextureDesc.ArraySize = 1;
    offscreenTextureDesc.SampleDesc.Count = 1;
    offscreenTextureDesc.SampleDesc.Quality = 0;
    offscreenTextureDesc.Usage = D3D11_USAGE_DEFAULT;
    offscreenTextureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    offscreenTextureDesc.CPUAccessFlags = 0;
    offscreenTextureDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;

    HRESULT hr = device->CreateTexture2D(&offscreenTextureDesc, NULL, getter_AddRefs(offscreenTexture));
    if (FAILED(hr)) {
        gfxCriticalNote << "DoesRecreatingCreateTexture2DFail";
        return false;
    }

    hr = offscreenTexture->QueryInterface(__uuidof(IDXGIKeyedMutex), (void**)getter_AddRefs(keyedMutex));
    if (FAILED(hr)) {
        gfxCriticalNote << "DoesRecreatingKeyedMutexFailed";
        return false;
    }
    D3D11_RENDER_TARGET_VIEW_DESC offscreenRTVDesc;
    offscreenRTVDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    offscreenRTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    offscreenRTVDesc.Texture2D.MipSlice = 0;

    RefPtr<ID3D11RenderTargetView> offscreenRTView;
    hr = device->CreateRenderTargetView(offscreenTexture, &offscreenRTVDesc, getter_AddRefs(offscreenRTView));
    if (FAILED(hr)) {
        gfxCriticalNote << "DoesRecreatingCreateRenderTargetViewFailed";
        return false;
    }

    // Acquire and clear
    keyedMutex->AcquireSync(0, INFINITE);
    FLOAT color1[4] = { 1, 1, 0.5, 1 };
    deviceContext->ClearRenderTargetView(offscreenRTView, color1);
    keyedMutex->ReleaseSync(0);


    keyedMutex->AcquireSync(0, INFINITE);
    FLOAT color2[4] = { 1, 1, 0, 1 };

    deviceContext->ClearRenderTargetView(offscreenRTView, color2);
    D3D11_TEXTURE2D_DESC desc;

    offscreenTexture->GetDesc(&desc);
    desc.Usage = D3D11_USAGE_STAGING;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.MiscFlags = 0;
    desc.BindFlags = 0;
    ID3D11Texture2D* cpuTexture;
    hr = device->CreateTexture2D(&desc, NULL, &cpuTexture);
    if (FAILED(hr)) {
        gfxCriticalNote << "DoesRecreatingCreateCPUTextureFailed";
        return false;
    }

    deviceContext->CopyResource(cpuTexture, offscreenTexture);

    D3D11_MAPPED_SUBRESOURCE mapped;
    hr = deviceContext->Map(cpuTexture, 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr)) {
        gfxCriticalNote << "DoesRecreatingMapFailed " << hexa(hr);
        return false;
    }
    int resultColor = *(int*)mapped.pData;
    deviceContext->Unmap(cpuTexture, 0);
    cpuTexture->Release();

    // XXX on some drivers resultColor will not have changed to
    // match the clear
    if (resultColor != 0xffffff00) {
        gfxCriticalNote << "RenderTargetViewNeedsRecreating";
        result = true;
    }

    keyedMutex->ReleaseSync(0);

    // It seems like this may only happen when we're using the NVIDIA gpu
    CheckForAdapterMismatch(device);
    return result;
}

static bool TryCreateTexture2D(ID3D11Device *device,
                               D3D11_TEXTURE2D_DESC* desc,
                               D3D11_SUBRESOURCE_DATA* data,
                               RefPtr<ID3D11Texture2D>& texture)
{
  // Older Intel driver version (see bug 1221348 for version #s) crash when
  // creating a texture with shared keyed mutex and data.
  MOZ_SEH_TRY {
    return !FAILED(device->CreateTexture2D(desc, data, getter_AddRefs(texture)));
  } MOZ_SEH_EXCEPT(EXCEPTION_EXECUTE_HANDLER) {
    // For now we want to aggregrate all the crash signature to a known crash.
    gfxDevCrash(LogReason::TextureCreation) << "Crash creating texture. See bug 1221348.";
    return false;
  }
}


// See bug 1083071. On some drivers, Direct3D 11 CreateShaderResourceView fails
// with E_OUTOFMEMORY.
bool DoesD3D11TextureSharingWorkInternal(ID3D11Device *device, DXGI_FORMAT format, UINT bindflags)
{
  // CreateTexture2D is known to crash on lower feature levels, see bugs
  // 1170211 and 1089413.
  if (device->GetFeatureLevel() < D3D_FEATURE_LEVEL_10_0) {
    return false;
  }

  if (gfxPrefs::Direct2DForceEnabled() ||
      gfxConfig::IsForcedOnByUser(Feature::HW_COMPOSITING))
  {
    return true;
  }

  if (GetModuleHandleW(L"atidxx32.dll")) {
    nsCOMPtr<nsIGfxInfo> gfxInfo = services::GetGfxInfo();
    if (gfxInfo) {
      nsString vendorID, vendorID2;
      gfxInfo->GetAdapterVendorID(vendorID);
      gfxInfo->GetAdapterVendorID2(vendorID2);
      if (vendorID.EqualsLiteral("0x8086") && vendorID2.IsEmpty()) {
        if (!gfxPrefs::LayersAMDSwitchableGfxEnabled()) {
          return false;
        }
        gfxCriticalError(CriticalLog::DefaultOptions(false)) << "PossiblyBrokenSurfaceSharing_UnexpectedAMDGPU";
      }
    }
  }

  RefPtr<ID3D11Texture2D> texture;
  D3D11_TEXTURE2D_DESC desc;
  const int texture_size = 32;
  desc.Width = texture_size;
  desc.Height = texture_size;
  desc.MipLevels = 1;
  desc.ArraySize = 1;
  desc.Format = format;
  desc.SampleDesc.Count = 1;
  desc.SampleDesc.Quality = 0;
  desc.Usage = D3D11_USAGE_DEFAULT;
  desc.CPUAccessFlags = 0;
  desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;
  desc.BindFlags = bindflags;

  uint32_t color[texture_size * texture_size];
  for (size_t i = 0; i < sizeof(color)/sizeof(color[0]); i++) {
    color[i] = 0xff00ffff;
  }
  // XXX If we pass the data directly at texture creation time we
  //     get a crash on Intel 8.5.10.[18xx-1994] drivers.
  //     We can work around this issue by doing UpdateSubresource.
  if (!TryCreateTexture2D(device, &desc, nullptr, texture)) {
    gfxCriticalNote << "DoesD3D11TextureSharingWork_TryCreateTextureFailure";
    return false;
  }

  RefPtr<IDXGIKeyedMutex> sourceSharedMutex;
  texture->QueryInterface(__uuidof(IDXGIKeyedMutex), (void**)getter_AddRefs(sourceSharedMutex));
  if (FAILED(sourceSharedMutex->AcquireSync(0, 30*1000))) {
    gfxCriticalError() << "DoesD3D11TextureSharingWork_SourceMutexTimeout";
    // only wait for 30 seconds
    return false;
  }

  RefPtr<ID3D11DeviceContext> deviceContext;
  device->GetImmediateContext(getter_AddRefs(deviceContext));

  int stride = texture_size * 4;
  deviceContext->UpdateSubresource(texture, 0, nullptr, color, stride, stride * texture_size);

  if (FAILED(sourceSharedMutex->ReleaseSync(0))) {
    gfxCriticalError() << "DoesD3D11TextureSharingWork_SourceReleaseSyncTimeout";
    return false;
  }

  HANDLE shareHandle;
  RefPtr<IDXGIResource> otherResource;
  if (FAILED(texture->QueryInterface(__uuidof(IDXGIResource),
                                     getter_AddRefs(otherResource))))
  {
    gfxCriticalError() << "DoesD3D11TextureSharingWork_GetResourceFailure";
    return false;
  }

  if (FAILED(otherResource->GetSharedHandle(&shareHandle))) {
    gfxCriticalError() << "DoesD3D11TextureSharingWork_GetSharedTextureFailure";
    return false;
  }

  RefPtr<ID3D11Resource> sharedResource;
  RefPtr<ID3D11Texture2D> sharedTexture;
  if (FAILED(device->OpenSharedResource(shareHandle, __uuidof(ID3D11Resource),
                                        getter_AddRefs(sharedResource))))
  {
    gfxCriticalError(CriticalLog::DefaultOptions(false)) << "OpenSharedResource failed for format " << format;
    return false;
  }

  if (FAILED(sharedResource->QueryInterface(__uuidof(ID3D11Texture2D),
                                            getter_AddRefs(sharedTexture))))
  {
    gfxCriticalError() << "DoesD3D11TextureSharingWork_GetSharedTextureFailure";
    return false;
  }

  // create a staging texture for readback
  RefPtr<ID3D11Texture2D> cpuTexture;
  desc.Usage = D3D11_USAGE_STAGING;
  desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
  desc.MiscFlags = 0;
  desc.BindFlags = 0;
  if (FAILED(device->CreateTexture2D(&desc, nullptr, getter_AddRefs(cpuTexture)))) {
    gfxCriticalError() << "DoesD3D11TextureSharingWork_CreateTextureFailure";
    return false;
  }

  RefPtr<IDXGIKeyedMutex> sharedMutex;
  sharedResource->QueryInterface(__uuidof(IDXGIKeyedMutex), (void**)getter_AddRefs(sharedMutex));
  if (FAILED(sharedMutex->AcquireSync(0, 30*1000))) {
    gfxCriticalError() << "DoesD3D11TextureSharingWork_AcquireSyncTimeout";
    // only wait for 30 seconds
    return false;
  }

  // Copy to the cpu texture so that we can readback
  deviceContext->CopyResource(cpuTexture, sharedTexture);

  D3D11_MAPPED_SUBRESOURCE mapped;
  int resultColor = 0;
  if (SUCCEEDED(deviceContext->Map(cpuTexture, 0, D3D11_MAP_READ, 0, &mapped))) {
    // read the texture
    resultColor = *(int*)mapped.pData;
    deviceContext->Unmap(cpuTexture, 0);
  } else {
    gfxCriticalError() << "DoesD3D11TextureSharingWork_MapFailed";
    return false;
  }

  sharedMutex->ReleaseSync(0);

  // check that the color we put in is the color we get out
  if (resultColor != color[0]) {
    // Shared surfaces seem to be broken on dual AMD & Intel HW when using the
    // AMD GPU
    gfxCriticalNote << "DoesD3D11TextureSharingWork_ColorMismatch";
    return false;
  }

  RefPtr<ID3D11ShaderResourceView> sharedView;

  // This if(FAILED()) is the one that actually fails on systems affected by bug 1083071.
  if (FAILED(device->CreateShaderResourceView(sharedTexture, NULL, getter_AddRefs(sharedView)))) {
    gfxCriticalNote << "CreateShaderResourceView failed for format" << format;
    return false;
  }

  return true;
}

bool DoesD3D11TextureSharingWork(ID3D11Device *device)
{
  return DoesD3D11TextureSharingWorkInternal(device, DXGI_FORMAT_B8G8R8A8_UNORM, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);
}

bool DoesD3D11AlphaTextureSharingWork(ID3D11Device *device)
{
  return DoesD3D11TextureSharingWorkInternal(device, DXGI_FORMAT_R8_UNORM, D3D11_BIND_SHADER_RESOURCE);
}

static inline bool
IsGfxInfoStatusOkay(int32_t aFeature, nsCString* aOutMessage)
{
  nsCOMPtr<nsIGfxInfo> gfxInfo = services::GetGfxInfo();
  if (!gfxInfo) {
    return true;
  }

  int32_t status;
  nsCString failureId;
  if (SUCCEEDED(gfxInfo->GetFeatureStatus(aFeature, failureId, &status)) &&
      status != nsIGfxInfo::FEATURE_STATUS_OK)
  {
    aOutMessage->AssignLiteral("#BLOCKLIST_");
    aOutMessage->AppendASCII(failureId.get());
    return false;
  }

  return true;
}

static inline bool
IsWARPStable()
{
  // It seems like nvdxgiwrap makes a mess of WARP. See bug 1154703.
  if (!IsWin8OrLater() || GetModuleHandleA("nvdxgiwrap.dll")) {
    return false;
  }
  return true;
}

void
gfxWindowsPlatform::InitializeConfig()
{
  if (!XRE_IsParentProcess()) {
    // Child processes init their configuration via UpdateDeviceInitData().
    return;
  }

  InitializeD3D9Config();
  InitializeD3D11Config();
  InitializeD2DConfig();
}

void
gfxWindowsPlatform::InitializeD3D9Config()
{
  MOZ_ASSERT(XRE_IsParentProcess());

  FeatureState& d3d9 = gfxConfig::GetFeature(Feature::D3D9_COMPOSITING);

  if (!IsVistaOrLater()) {
    d3d9.EnableByDefault();
  } else {
    d3d9.SetDefaultFromPref(
      gfxPrefs::GetLayersAllowD3D9FallbackPrefName(),
      true,
      gfxPrefs::GetLayersAllowD3D9FallbackPrefDefault());

    if (!d3d9.IsEnabled() && gfxPrefs::LayersPreferD3D9()) {
      d3d9.UserEnable("Direct3D9 enabled via layers.prefer-d3d9");
    }
  }

  nsCString message;
  if (!IsGfxInfoStatusOkay(nsIGfxInfo::FEATURE_DIRECT3D_9_LAYERS, &message)) {
    d3d9.Disable(FeatureStatus::Blacklisted, message.get());
  }

  if (gfxConfig::IsForcedOnByUser(Feature::HW_COMPOSITING)) {
    d3d9.UserForceEnable("Hardware compositing is force-enabled");
  }
}

void
gfxWindowsPlatform::InitializeD3D11Config()
{
  MOZ_ASSERT(XRE_IsParentProcess());

  FeatureState& d3d11 = gfxConfig::GetFeature(Feature::D3D11_COMPOSITING);

  if (!gfxConfig::IsEnabled(Feature::HW_COMPOSITING)) {
    d3d11.DisableByDefault(FeatureStatus::Unavailable, "Hardware compositing is disabled");
    return;
  }

  d3d11.EnableByDefault();

  // If the user prefers D3D9, act as though they disabled D3D11.
  if (gfxPrefs::LayersPreferD3D9()) {
    d3d11.UserDisable("Disabled due to user preference for Direct3D 9");
    return;
  }

  nsCString message;
  if (!IsGfxInfoStatusOkay(nsIGfxInfo::FEATURE_DIRECT3D_11_LAYERS, &message)) {
    if (IsWARPStable() && !gfxPrefs::LayersD3D11DisableWARP()) {
      // We do not expect hardware D3D11 to work, so we'll try WARP.
      gfxConfig::EnableFallback(Fallback::USE_D3D11_WARP_COMPOSITOR, message.get());
    } else {
      // There is little to no chance of D3D11 working, so just disable it.
      d3d11.Disable(FeatureStatus::Blacklisted, message.get());
    }
  }

  // Check if the user really, really wants WARP.
  if (gfxPrefs::LayersD3D11ForceWARP()) {
    // Force D3D11 on even if we disabled it.
    d3d11.UserForceEnable("User force-enabled WARP on disabled hardware");

    gfxConfig::EnableFallback(
      Fallback::USE_D3D11_WARP_COMPOSITOR,
      "Force-enabled by user preference");
  }
}

bool
gfxWindowsPlatform::UpdateDeviceInitData()
{
  if (!gfxPlatform::UpdateDeviceInitData()) {
    return false;
  }

  if (gfxConfig::InitOrUpdate(Feature::D3D11_COMPOSITING,
                              GetParentDevicePrefs().useD3D11(),
                              FeatureStatus::Disabled,
                              "Disabled by parent process"))
  {
    if (GetParentDevicePrefs().useD3D11WARP()) {
      gfxConfig::EnableFallback(Fallback::USE_D3D11_WARP_COMPOSITOR, "Requested by parent process");
    }
  }

  gfxConfig::InitOrUpdate(
    Feature::DIRECT2D,
    GetParentDevicePrefs().useD2D1(),
    FeatureStatus::Disabled,
    "Disabled by parent process");

  return true;
}

// We don't have access to the D3D11CreateDevice type in gfxWindowsPlatform.h,
// since it doesn't include d3d11.h, so we use a static here. It should only
// be used within InitializeD3D11.
decltype(D3D11CreateDevice)* sD3D11CreateDeviceFn = nullptr;

bool
gfxWindowsPlatform::AttemptD3D11DeviceCreationHelper(
  IDXGIAdapter1* aAdapter, RefPtr<ID3D11Device>& aOutDevice, HRESULT& aResOut)
{
  MOZ_SEH_TRY {
    aResOut =
      sD3D11CreateDeviceFn(
        aAdapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr,
        // Use D3D11_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS
        // to prevent bug 1092260. IE 11 also uses this flag.
        D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS,
        mFeatureLevels.Elements(), mFeatureLevels.Length(),
        D3D11_SDK_VERSION, getter_AddRefs(aOutDevice), nullptr, nullptr);
  } MOZ_SEH_EXCEPT (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
  return true;
}

void
gfxWindowsPlatform::AttemptD3D11DeviceCreation(FeatureState& d3d11)
{
  RefPtr<IDXGIAdapter1> adapter = GetDXGIAdapter();
  if (!adapter) {
    d3d11.SetFailed(FeatureStatus::Unavailable, "Failed to acquire a DXGI adapter");
    return;
  }

  HRESULT hr;
  RefPtr<ID3D11Device> device;
  if (!AttemptD3D11DeviceCreationHelper(adapter, device, hr)) {
    gfxCriticalError() << "Crash during D3D11 device creation";
    d3d11.SetFailed(FeatureStatus::CrashedInHandler, "Crashed trying to acquire a D3D11 device");
    return;
  }

  if (FAILED(hr) || !device) {
    gfxCriticalError() << "D3D11 device creation failed: " << hexa(hr);
    d3d11.SetFailed(FeatureStatus::Failed, "Failed to acquire a D3D11 device");
    return;
  }
  if (!DoesD3D11DeviceWork()) {
    d3d11.SetFailed(FeatureStatus::Broken, "Direct3D11 device was determined to be broken");
    return;
  }

  {
    MutexAutoLock lock(mDeviceLock);
    mD3D11Device = device;
  }

  // Only test this when not using WARP since it can fail and cause
  // GetDeviceRemovedReason to return weird values.
  mCompositorD3D11TextureSharingWorks = ::DoesD3D11TextureSharingWork(mD3D11Device);

  if (!mCompositorD3D11TextureSharingWorks) {
    gfxConfig::SetFailed(Feature::D3D11_ANGLE,
                         FeatureStatus::Broken,
                         "Texture sharing doesn't work");
  }

  if (DoesRenderTargetViewNeedsRecreating(mD3D11Device)) {
    gfxConfig::SetFailed(Feature::D3D11_ANGLE,
                         FeatureStatus::Broken,
                         "RenderTargetViews need recreating");
  }

  mD3D11Device->SetExceptionMode(0);
  mIsWARP = false;
}

bool
gfxWindowsPlatform::AttemptWARPDeviceCreationHelper(
  ScopedGfxFeatureReporter& aReporterWARP,
  RefPtr<ID3D11Device>& aOutDevice,
  HRESULT& aResOut)
{
  MOZ_SEH_TRY {
    aResOut =
      sD3D11CreateDeviceFn(
        nullptr, D3D_DRIVER_TYPE_WARP, nullptr,
        // Use D3D11_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS
        // to prevent bug 1092260. IE 11 also uses this flag.
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        mFeatureLevels.Elements(), mFeatureLevels.Length(),
        D3D11_SDK_VERSION, getter_AddRefs(aOutDevice), nullptr, nullptr);

    aReporterWARP.SetSuccessful();
  } MOZ_SEH_EXCEPT (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
  return true;
}

void
gfxWindowsPlatform::AttemptWARPDeviceCreation()
{
  ScopedGfxFeatureReporter reporterWARP("D3D11-WARP", gfxPrefs::LayersD3D11ForceWARP());
  FeatureState& d3d11 = gfxConfig::GetFeature(Feature::D3D11_COMPOSITING);

  HRESULT hr;
  RefPtr<ID3D11Device> device;
  if (!AttemptWARPDeviceCreationHelper(reporterWARP, device, hr)) {
    gfxCriticalError() << "Exception occurred initializing WARP D3D11 device!";
    d3d11.SetFailed(FeatureStatus::CrashedInHandler, "Crashed creating a D3D11 WARP device");
    return;
  }

  if (FAILED(hr) || !device) {
    // This should always succeed... in theory.
    gfxCriticalError() << "Failed to initialize WARP D3D11 device! " << hexa(hr);
    d3d11.SetFailed(FeatureStatus::Failed, "Failed to create a D3D11 WARP device");
    return;
  }

  {
    MutexAutoLock lock(mDeviceLock);
    mD3D11Device = device;
  }

  // Only test for texture sharing on Windows 8 since it puts the device into
  // an unusable state if used on Windows 7
  if (IsWin8OrLater()) {
    mCompositorD3D11TextureSharingWorks = ::DoesD3D11TextureSharingWork(mD3D11Device);
  }
  mD3D11Device->SetExceptionMode(0);
  mIsWARP = true;
}

bool
gfxWindowsPlatform::ContentAdapterIsParentAdapter(ID3D11Device* device)
{
  DXGI_ADAPTER_DESC desc;
  if (!GetDxgiDesc(device, &desc)) {
    gfxCriticalNote << "Could not query device DXGI adapter info";
    return false;
  }

  const DxgiAdapterDesc& parent = GetParentDevicePrefs().adapter();
  if (desc.VendorId != parent.VendorId ||
      desc.DeviceId != parent.DeviceId ||
      desc.SubSysId != parent.SubSysId ||
      desc.AdapterLuid.HighPart != parent.AdapterLuid.HighPart ||
      desc.AdapterLuid.LowPart != parent.AdapterLuid.LowPart)
  {
    gfxCriticalNote << "VendorIDMismatch P " << hexa(parent.VendorId) << " " << hexa(desc.VendorId);
    return false;
  }

  return true;
}

static void
RecordContentDeviceFailure(TelemetryDeviceCode aDevice)
{
  // If the parent process fails to acquire a device, we record this
  // normally as part of the environment. The exceptional case we're
  // looking for here is when the parent process successfully acquires
  // a device, but the content process fails to acquire the same device.
  // This would not normally be displayed in about:support.
  if (!XRE_IsContentProcess()) {
    return;
  }
  Telemetry::Accumulate(Telemetry::GFX_CONTENT_FAILED_TO_ACQUIRE_DEVICE, uint32_t(aDevice));
}

bool
gfxWindowsPlatform::AttemptD3D11ContentDeviceCreationHelper(
  IDXGIAdapter1* aAdapter, HRESULT& aResOut)
{
  MOZ_SEH_TRY {
    aResOut =
      sD3D11CreateDeviceFn(
        aAdapter, mIsWARP ? D3D_DRIVER_TYPE_WARP : D3D_DRIVER_TYPE_UNKNOWN,
        nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        mFeatureLevels.Elements(), mFeatureLevels.Length(),
        D3D11_SDK_VERSION, getter_AddRefs(mD3D11ContentDevice), nullptr, nullptr);

  } MOZ_SEH_EXCEPT (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
  return true;
}

FeatureStatus
gfxWindowsPlatform::AttemptD3D11ContentDeviceCreation()
{
  RefPtr<IDXGIAdapter1> adapter;
  if (!mIsWARP) {
    adapter = GetDXGIAdapter();
    if (!adapter) {
      return FeatureStatus::Unavailable;
    }
  }

  HRESULT hr;
  if (!AttemptD3D11ContentDeviceCreationHelper(adapter, hr)) {
    gfxCriticalNote << "Recovered from crash while creating a D3D11 content device";
    RecordContentDeviceFailure(TelemetryDeviceCode::Content);
    return FeatureStatus::CrashedInHandler;
  }

  if (FAILED(hr) || !mD3D11ContentDevice) {
    gfxCriticalNote << "Failed to create a D3D11 content device: " << hexa(hr);
    RecordContentDeviceFailure(TelemetryDeviceCode::Content);
    return FeatureStatus::Failed;
  }

  // InitializeD2D() will abort early if the compositor device did not support
  // texture sharing. If we're in the content process, we can't rely on the
  // parent device alone: some systems have dual GPUs that are capable of
  // binding the parent and child processes to different GPUs. As a safety net,
  // we re-check texture sharing against the newly created D3D11 content device.
  // If it fails, we won't use Direct2D.
  if (XRE_IsContentProcess()) {
    if (!DoesD3D11TextureSharingWork(mD3D11ContentDevice)) {
      mD3D11ContentDevice = nullptr;
      return FeatureStatus::Failed;
    }

    DebugOnly<bool> ok = ContentAdapterIsParentAdapter(mD3D11ContentDevice);
    MOZ_ASSERT(ok);
  }

  mD3D11ContentDevice->SetExceptionMode(0);

  RefPtr<ID3D10Multithread> multi;
  hr = mD3D11ContentDevice->QueryInterface(__uuidof(ID3D10Multithread), getter_AddRefs(multi));
  if (SUCCEEDED(hr) && multi) {
    multi->SetMultithreadProtected(TRUE);
  }
  return FeatureStatus::Available;
}

bool
gfxWindowsPlatform::AttemptD3D11ImageBridgeDeviceCreationHelper(
  IDXGIAdapter1* aAdapter,
  HRESULT& aResOut)
{
  MOZ_SEH_TRY {
    aResOut =
      sD3D11CreateDeviceFn(GetDXGIAdapter(), D3D_DRIVER_TYPE_UNKNOWN, nullptr,
                           D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                           mFeatureLevels.Elements(), mFeatureLevels.Length(),
                           D3D11_SDK_VERSION, getter_AddRefs(mD3D11ImageBridgeDevice), nullptr, nullptr);
  } MOZ_SEH_EXCEPT(EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
  return true;
}

FeatureStatus
gfxWindowsPlatform::AttemptD3D11ImageBridgeDeviceCreation()
{
  HRESULT hr;
  if (!AttemptD3D11ImageBridgeDeviceCreationHelper(GetDXGIAdapter(), hr)) {
    gfxCriticalNote << "Recovered from crash while creating a D3D11 image bridge device";
    RecordContentDeviceFailure(TelemetryDeviceCode::Image);
    return FeatureStatus::CrashedInHandler;
  }

  if (FAILED(hr) || !mD3D11ImageBridgeDevice) {
    gfxCriticalNote << "Failed to create a content image bridge device: " << hexa(hr);
    RecordContentDeviceFailure(TelemetryDeviceCode::Image);
    return FeatureStatus::Failed;
  }

  mD3D11ImageBridgeDevice->SetExceptionMode(0);
  if (!DoesD3D11AlphaTextureSharingWork(mD3D11ImageBridgeDevice)) {
    mD3D11ImageBridgeDevice = nullptr;
    return FeatureStatus::Failed;
  }

  if (XRE_IsContentProcess()) {
    ContentAdapterIsParentAdapter(mD3D11ImageBridgeDevice);
  }
  return FeatureStatus::Available;
}

void
gfxWindowsPlatform::InitializeDevices()
{
  // Ask the parent process for an updated list of which devices to create.
  UpdateDeviceInitData();

  // If acceleration is disabled, we refuse to initialize anything.
  if (!gfxConfig::IsEnabled(Feature::HW_COMPOSITING)) {
    return;
  }

  MOZ_ASSERT(!InSafeMode());

  // If we previously crashed initializing devices, bail out now.
  D3D11LayersCrashGuard detectCrashes;
  if (detectCrashes.Crashed()) {
    gfxConfig::SetFailed(Feature::HW_COMPOSITING,
                         FeatureStatus::CrashedOnStartup,
                         "Crashed during startup in a previous session");
    gfxConfig::SetFailed(Feature::D3D11_COMPOSITING,
                         FeatureStatus::CrashedOnStartup,
                         "Harware acceleration crashed during startup in a previous session");
    gfxConfig::SetFailed(Feature::DIRECT2D,
                         FeatureStatus::CrashedOnStartup,
                         "Harware acceleration crashed during startup in a previous session");
    return;
  }

  // First, initialize D3D11. If this succeeds we attempt to use Direct2D.
  InitializeD3D11();
  InitializeD2D();

  if (!gfxConfig::IsEnabled(Feature::D3D11_COMPOSITING)) {
    gfxConfig::DisableByDefault(Feature::D3D11_ANGLE, FeatureStatus::Disabled, "D3D11 compositing is disabled");
  } else {
    gfxConfig::EnableByDefault(Feature::D3D11_ANGLE);
  }

  if (!gfxConfig::IsEnabled(Feature::DIRECT2D)) {
    if (XRE_IsContentProcess() && GetParentDevicePrefs().useD2D1()) {
      RecordContentDeviceFailure(TelemetryDeviceCode::D2D1);
    }

    // Usually we want D2D in order to use DWrite, but if the users have it
    // forced, we'll let them have it, as unsupported configuration.
    if (gfxPrefs::DirectWriteFontRenderingForceEnabled() && !mDWriteFactory) {
      gfxCriticalNote << "Attempting DWrite without D2D support";
      InitDWriteSupport();
    }
  }
}

bool
gfxWindowsPlatform::CanUseD3D11ImageBridge()
{
  if (XRE_IsContentProcess()) {
    if (!GetParentDevicePrefs().useD3D11ImageBridge()) {
      return false;
    }
  }
  return !mIsWARP;
}

void
gfxWindowsPlatform::InitializeD3D11()
{
  // This function attempts to initialize our D3D11 devices, if the hardware
  // is not blacklisted for D3D11 layers. This first attempt will try to create
  // a hardware accelerated device. If this creation fails or the hardware is
  // blacklisted, then this function will abort if WARP is disabled, causing us
  // to fallback to D3D9 or Basic layers. If WARP is not disabled it will use
  // a WARP device which should always be available on Windows 7 and higher.

  // Check if D3D11 is supported on this hardware.
  FeatureState& d3d11 = gfxConfig::GetFeature(Feature::D3D11_COMPOSITING);
  if (!d3d11.IsEnabled()) {
    return;
  }

  // Check if D3D11 is available on this system.
  nsModuleHandle d3d11Module(LoadLibrarySystem32(L"d3d11.dll"));
  sD3D11CreateDeviceFn =
    (decltype(D3D11CreateDevice)*)GetProcAddress(d3d11Module, "D3D11CreateDevice");
  if (!sD3D11CreateDeviceFn) {
    // We should just be on Windows Vista or XP in this case.
    d3d11.SetFailed(FeatureStatus::Unavailable, "Direct3D11 not available on this computer");
    return;
  }

  // Check if a failure was injected for testing.
  if (gfxPrefs::DeviceFailForTesting()) {
    d3d11.SetFailed(FeatureStatus::Failed, "Direct3D11 device failure simulated by preference");
    return;
  }

  if (XRE_IsParentProcess()) {
    if (!gfxConfig::UseFallback(Fallback::USE_D3D11_WARP_COMPOSITOR)) {
      AttemptD3D11DeviceCreation(d3d11);
      if (d3d11.GetValue() == FeatureStatus::CrashedInHandler) {
        return;
      }

      // If we failed to get a device, but WARP is allowed and might work,
      // re-enable D3D11 and switch to WARP.
      if (!mD3D11Device && IsWARPStable() && !gfxPrefs::LayersD3D11DisableWARP()) {
        gfxConfig::Reenable(Feature::D3D11_COMPOSITING, Fallback::USE_D3D11_WARP_COMPOSITOR);
      }
    }

    // If that failed, see if we can use WARP.
    if (gfxConfig::UseFallback(Fallback::USE_D3D11_WARP_COMPOSITOR)) {
      MOZ_ASSERT(d3d11.IsEnabled());
      MOZ_ASSERT(!mD3D11Device);
      MOZ_ASSERT(IsWARPStable() || gfxPrefs::LayersD3D11ForceWARP());

      AttemptWARPDeviceCreation();
      if (d3d11.GetValue() == FeatureStatus::CrashedInHandler) {
        return;
      }
    }

    // If we still have no device by now, exit.
    if (!mD3D11Device) {
      MOZ_ASSERT(!gfxConfig::IsEnabled(Feature::D3D11_COMPOSITING));
      return;
    }

    // Either device creation function should have returned Available.
    MOZ_ASSERT(d3d11.IsEnabled());
  } else {
    // Child processes do not need a compositor, but they do need to know
    // whether the parent process is using WARP and whether or not texture
    // sharing works.
    mIsWARP = gfxConfig::UseFallback(Fallback::USE_D3D11_WARP_COMPOSITOR);
    mCompositorD3D11TextureSharingWorks = GetParentDevicePrefs().d3d11TextureSharingWorks();
  }

  if (CanUseD3D11ImageBridge()) {
    if (AttemptD3D11ImageBridgeDeviceCreation() == FeatureStatus::CrashedInHandler) {
      DisableD3D11AfterCrash();
      return;
    }
  }

  if (AttemptD3D11ContentDeviceCreation() == FeatureStatus::CrashedInHandler) {
    DisableD3D11AfterCrash();
    return;
  }

  // We leak these everywhere and we need them our entire runtime anyway, let's
  // leak it here as well. We keep the pointer to sD3D11CreateDeviceFn around
  // as well for D2D1 and device resets.
  d3d11Module.disown();
}

void
gfxWindowsPlatform::DisableD3D11AfterCrash()
{
  gfxConfig::Disable(Feature::D3D11_COMPOSITING,
    FeatureStatus::CrashedInHandler,
    "Crashed while acquiring a Direct3D11 device");
  ResetD3D11Devices();
}

void
gfxWindowsPlatform::ResetD3D11Devices()
{
  MutexAutoLock lock(mDeviceLock);

  mD3D11Device = nullptr;
  mD3D11ContentDevice = nullptr;
  mD3D11ImageBridgeDevice = nullptr;
  Factory::SetDirect3D11Device(nullptr);
}

void
gfxWindowsPlatform::InitializeD2DConfig()
{
  FeatureState& d2d1 = gfxConfig::GetFeature(Feature::DIRECT2D);

  if (!gfxConfig::IsEnabled(Feature::D3D11_COMPOSITING)) {
    d2d1.DisableByDefault(FeatureStatus::Unavailable, "Direct2D requires Direct3D 11 compositing");
  } else if (!IsVistaOrLater()) {
    d2d1.DisableByDefault(FeatureStatus::Unavailable, "Direct2D is not available on Windows XP");
  } else {
    d2d1.SetDefaultFromPref(
      gfxPrefs::GetDirect2DDisabledPrefName(),
      false,
      gfxPrefs::GetDirect2DDisabledPrefDefault());
  }

  nsCString message;
  if (!IsGfxInfoStatusOkay(nsIGfxInfo::FEATURE_DIRECT2D, &message)) {
    d2d1.Disable(FeatureStatus::Blacklisted, message.get());
  }

  if (!d2d1.IsEnabled() && gfxPrefs::Direct2DForceEnabled()) {
    d2d1.UserForceEnable("Force-enabled via user-preference");
  }
}

void
gfxWindowsPlatform::InitializeD2D()
{
  ScopedGfxFeatureReporter d2d1_1("D2D1.1");

  FeatureState& d2d1 = gfxConfig::GetFeature(Feature::DIRECT2D);

  // We don't know this value ahead of time, but the user can force-override
  // it, so we use Disable instead of SetFailed.
  if (mIsWARP) {
    d2d1.Disable(FeatureStatus::Blocked, "Direct2D is not compatible with Direct3D11 WARP");
  }

  // If we pass all the initial checks, we can proceed to runtime decisions.
  if (!d2d1.IsEnabled()) {
    return;
  }

  if (!Factory::SupportsD2D1()) {
    d2d1.SetFailed(FeatureStatus::Unavailable, "Failed to acquire a Direct2D 1.1 factory");
    return;
  }

  if (!mD3D11ContentDevice) {
    d2d1.SetFailed(FeatureStatus::Failed, "Failed to acquire a Direct3D 11 content device");
    return;
  }

  if (!mCompositorD3D11TextureSharingWorks) {
    d2d1.SetFailed(FeatureStatus::Failed, "Direct3D11 device does not support texture sharing");
    return;
  }

  // Using Direct2D depends on DWrite support.
  if (!mDWriteFactory && !InitDWriteSupport()) {
    d2d1.SetFailed(FeatureStatus::Failed, "Failed to initialize DirectWrite support");
    return;
  }

  // Verify that Direct2D device creation succeeded.
  if (!Factory::SetDirect3D11Device(mD3D11ContentDevice)) {
    d2d1.SetFailed(FeatureStatus::Failed, "Failed to create a Direct2D device");
    return;
  }

  MOZ_ASSERT(d2d1.IsEnabled());
  d2d1_1.SetSuccessful();
}

bool
gfxWindowsPlatform::CreateD3D11DecoderDeviceHelper(
  IDXGIAdapter1* aAdapter, RefPtr<ID3D11Device>& aDevice, HRESULT& aResOut)
{
  MOZ_SEH_TRY{
    aResOut =
      sD3D11CreateDeviceFn(
        aAdapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr,
        D3D11_CREATE_DEVICE_VIDEO_SUPPORT,
        mFeatureLevels.Elements(), mFeatureLevels.Length(),
        D3D11_SDK_VERSION, getter_AddRefs(aDevice), nullptr, nullptr);

  } MOZ_SEH_EXCEPT(EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
  return true;
}

already_AddRefed<ID3D11Device>
gfxWindowsPlatform::CreateD3D11DecoderDevice()
{
   if (!sD3D11CreateDeviceFn) {
    // We should just be on Windows Vista or XP in this case.
    return nullptr;
  }

  RefPtr<IDXGIAdapter1> adapter = GetDXGIAdapter();

  if (!adapter) {
    return nullptr;
  }

  RefPtr<ID3D11Device> device;

  HRESULT hr;
  if (!CreateD3D11DecoderDeviceHelper(adapter, device, hr)) {
    return nullptr;
  }

  if (FAILED(hr) || !device || !DoesD3D11DeviceWork()) {
    return nullptr;
  }

  RefPtr<ID3D10Multithread> multi;
  device->QueryInterface(__uuidof(ID3D10Multithread), getter_AddRefs(multi));

  multi->SetMultithreadProtected(TRUE);

  return device.forget();
}

bool
gfxWindowsPlatform::DwmCompositionEnabled()
{
  if (!IsVistaOrLater()) {
    return false;
  }

  MOZ_ASSERT(WinUtils::dwmIsCompositionEnabledPtr);
  BOOL dwmEnabled = false;

  if (FAILED(WinUtils::dwmIsCompositionEnabledPtr(&dwmEnabled))) {
    return false;
  }

  return dwmEnabled;
}

class D3DVsyncSource final : public VsyncSource
{
public:

  class D3DVsyncDisplay final : public VsyncSource::Display
  {
    NS_INLINE_DECL_THREADSAFE_REFCOUNTING(D3DVsyncDisplay)
    public:
      D3DVsyncDisplay()
        : mPrevVsync(TimeStamp::Now())
        , mVsyncEnabledLock("D3DVsyncEnabledLock")
        , mVsyncEnabled(false)
      {
        mVsyncThread = new base::Thread("WindowsVsyncThread");
        const double rate = 1000 / 60.0;
        mSoftwareVsyncRate = TimeDuration::FromMilliseconds(rate);
        MOZ_RELEASE_ASSERT(mVsyncThread->Start(), "Could not start Windows vsync thread");
        SetVsyncRate();
      }

      void SetVsyncRate()
      {
        if (!gfxWindowsPlatform::GetPlatform()->DwmCompositionEnabled()) {
          mVsyncRate = TimeDuration::FromMilliseconds(1000.0 / 60.0);
          return;
        }

        DWM_TIMING_INFO vblankTime;
        // Make sure to init the cbSize, otherwise GetCompositionTiming will fail
        vblankTime.cbSize = sizeof(DWM_TIMING_INFO);
        HRESULT hr = WinUtils::dwmGetCompositionTimingInfoPtr(0, &vblankTime);
        if (SUCCEEDED(hr)) {
          UNSIGNED_RATIO refreshRate = vblankTime.rateRefresh;
          // We get the rate in hertz / time, but we want the rate in ms.
          float rate = ((float) refreshRate.uiDenominator
                       / (float) refreshRate.uiNumerator) * 1000;
          mVsyncRate = TimeDuration::FromMilliseconds(rate);
        } else {
          mVsyncRate = TimeDuration::FromMilliseconds(1000.0 / 60.0);
        }
      }

      virtual void EnableVsync() override
      {
        MOZ_ASSERT(NS_IsMainThread());
        MOZ_ASSERT(mVsyncThread->IsRunning());
        { // scope lock
          MonitorAutoLock lock(mVsyncEnabledLock);
          if (mVsyncEnabled) {
            return;
          }
          mVsyncEnabled = true;
        }

        mVsyncThread->message_loop()->PostTask(
            NewRunnableMethod(this, &D3DVsyncDisplay::VBlankLoop));
      }

      virtual void DisableVsync() override
      {
        MOZ_ASSERT(NS_IsMainThread());
        MOZ_ASSERT(mVsyncThread->IsRunning());
        MonitorAutoLock lock(mVsyncEnabledLock);
        if (!mVsyncEnabled) {
          return;
        }
        mVsyncEnabled = false;
      }

      virtual bool IsVsyncEnabled() override
      {
        MOZ_ASSERT(NS_IsMainThread());
        MonitorAutoLock lock(mVsyncEnabledLock);
        return mVsyncEnabled;
      }

      virtual TimeDuration GetVsyncRate() override
      {
        return mVsyncRate;
      }

      void ScheduleSoftwareVsync(TimeStamp aVsyncTimestamp)
      {
        MOZ_ASSERT(IsInVsyncThread());
        NS_WARNING("DwmComposition dynamically disabled, falling back to software timers");

        TimeStamp nextVsync = aVsyncTimestamp + mSoftwareVsyncRate;
        TimeDuration delay = nextVsync - TimeStamp::Now();
        if (delay.ToMilliseconds() < 0) {
          delay = mozilla::TimeDuration::FromMilliseconds(0);
        }

        mVsyncThread->message_loop()->PostDelayedTask(
            NewRunnableMethod(this, &D3DVsyncDisplay::VBlankLoop),
            delay.ToMilliseconds());
      }

      TimeStamp GetAdjustedVsyncTimeStamp(LARGE_INTEGER& aFrequency,
                                          QPC_TIME& aQpcVblankTime)
      {
        TimeStamp vsync = TimeStamp::Now();
        LARGE_INTEGER qpcNow;
        QueryPerformanceCounter(&qpcNow);

        const int microseconds = 1000000;
        int64_t adjust = qpcNow.QuadPart - aQpcVblankTime;
        int64_t usAdjust = (adjust * microseconds) / aFrequency.QuadPart;
        vsync -= TimeDuration::FromMicroseconds((double) usAdjust);

        if (IsWin10OrLater()) {
          // On Windows 10 and on, DWMGetCompositionTimingInfo, mostly
          // reports the upcoming vsync time, which is in the future.
          // It can also sometimes report a vblank time in the past.
          // Since large parts of Gecko assume TimeStamps can't be in future,
          // use the previous vsync.

          // Windows 10 and Intel HD vsync timestamps are messy and
          // all over the place once in a while. Most of the time,
          // it reports the upcoming vsync. Sometimes, that upcoming
          // vsync is in the past. Sometimes that upcoming vsync is before
          // the previously seen vsync. Sometimes, the previous vsync
          // is still in the future. In these error cases,
          // we try to normalize to Now().
          TimeStamp upcomingVsync = vsync;
          if (upcomingVsync < mPrevVsync) {
            // Windows can report a vsync that's before
            // the previous one. So update it to sometime in the future.
            upcomingVsync = TimeStamp::Now() + TimeDuration::FromMilliseconds(1);
          }

          vsync = mPrevVsync;
          mPrevVsync = upcomingVsync;
        }
        // On Windows 7 and 8, DwmFlush wakes up AFTER qpcVBlankTime
        // from DWMGetCompositionTimingInfo. We can return the adjusted vsync.

        // Once in a while, the reported vsync timestamp can be in the future.
        // Normalize the reported timestamp to now.
        if (vsync >= TimeStamp::Now()) {
          vsync = TimeStamp::Now();
        }
        return vsync;
      }

      void VBlankLoop()
      {
        MOZ_ASSERT(IsInVsyncThread());
        MOZ_ASSERT(sizeof(int64_t) == sizeof(QPC_TIME));

        DWM_TIMING_INFO vblankTime;
        // Make sure to init the cbSize, otherwise GetCompositionTiming will fail
        vblankTime.cbSize = sizeof(DWM_TIMING_INFO);

        LARGE_INTEGER frequency;
        QueryPerformanceFrequency(&frequency);
        TimeStamp vsync = TimeStamp::Now();
        // On Windows 10, DwmGetCompositionInfo returns the upcoming vsync.
        // See GetAdjustedVsyncTimestamp.
        // On start, set mPrevVsync to the "next" vsync
        // So we'll use this timestamp on the 2nd loop iteration.
        mPrevVsync = vsync + mSoftwareVsyncRate;

        for (;;) {
          { // scope lock
            MonitorAutoLock lock(mVsyncEnabledLock);
            if (!mVsyncEnabled) return;
          }

          // Large parts of gecko assume that the refresh driver timestamp
          // must be <= Now() and cannot be in the future.
          MOZ_ASSERT(vsync <= TimeStamp::Now());
          Display::NotifyVsync(vsync);

          // DwmComposition can be dynamically enabled/disabled
          // so we have to check every time that it's available.
          // When it is unavailable, we fallback to software but will try
          // to get back to dwm rendering once it's re-enabled
          if (!gfxWindowsPlatform::GetPlatform()->DwmCompositionEnabled()) {
            ScheduleSoftwareVsync(vsync);
            return;
          }

          // Use a combination of DwmFlush + DwmGetCompositionTimingInfoPtr
          // Using WaitForVBlank, the whole system dies :/
          HRESULT hr = WinUtils::dwmFlushProcPtr();
          if (!SUCCEEDED(hr)) {
            // We don't actually know how long we had to wait on DWMFlush
            // Instead of trying to calculate how long DwmFlush actually took
            // Fallback to software vsync.
            ScheduleSoftwareVsync(TimeStamp::Now());
            return;
          }

          hr = WinUtils::dwmGetCompositionTimingInfoPtr(0, &vblankTime);
          vsync = SUCCEEDED(hr) ?
                    GetAdjustedVsyncTimeStamp(frequency, vblankTime.qpcVBlank) :
                    TimeStamp::Now();
        } // end for
      }

    private:
      virtual ~D3DVsyncDisplay()
      {
        MOZ_ASSERT(NS_IsMainThread());
        DisableVsync();
        mVsyncThread->Stop();
        delete mVsyncThread;
      }

      bool IsInVsyncThread()
      {
        return mVsyncThread->thread_id() == PlatformThread::CurrentId();
      }

      TimeDuration mSoftwareVsyncRate;
      TimeStamp mPrevVsync; // Only used on Windows 10
      Monitor mVsyncEnabledLock;
      base::Thread* mVsyncThread;
      TimeDuration mVsyncRate;
      bool mVsyncEnabled;
  }; // end d3dvsyncdisplay

  D3DVsyncSource()
  {
    mPrimaryDisplay = new D3DVsyncDisplay();
  }

  virtual Display& GetGlobalDisplay() override
  {
    return *mPrimaryDisplay;
  }

private:
  virtual ~D3DVsyncSource()
  {
  }
  RefPtr<D3DVsyncDisplay> mPrimaryDisplay;
}; // end D3DVsyncSource

already_AddRefed<mozilla::gfx::VsyncSource>
gfxWindowsPlatform::CreateHardwareVsyncSource()
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  if (!WinUtils::dwmIsCompositionEnabledPtr) {
    NS_WARNING("Dwm composition not available, falling back to software vsync");
    return gfxPlatform::CreateHardwareVsyncSource();
  }

  BOOL dwmEnabled = false;
  WinUtils::dwmIsCompositionEnabledPtr(&dwmEnabled);
  if (!dwmEnabled) {
    NS_WARNING("DWM not enabled, falling back to software vsync");
    return gfxPlatform::CreateHardwareVsyncSource();
  }

  RefPtr<VsyncSource> d3dVsyncSource = new D3DVsyncSource();
  return d3dVsyncSource.forget();
}

bool
gfxWindowsPlatform::SupportsApzTouchInput() const
{
  int value = gfxPrefs::TouchEventsEnabled();
  return value == 1 || value == 2;
}

void
gfxWindowsPlatform::GetAcceleratedCompositorBackends(nsTArray<LayersBackend>& aBackends)
{
  if (gfxPrefs::LayersPreferOpenGL()) {
    aBackends.AppendElement(LayersBackend::LAYERS_OPENGL);
  }

  if (gfxConfig::IsEnabled(Feature::D3D9_COMPOSITING) && gfxPrefs::LayersPreferD3D9()) {
    aBackends.AppendElement(LayersBackend::LAYERS_D3D9);
  }

  if (mD3D11Device) {
    aBackends.AppendElement(LayersBackend::LAYERS_D3D11);
  } else {
    NS_WARNING("Direct3D 11-accelerated layers are not supported on this system.");
  }

  if (gfxConfig::IsEnabled(Feature::D3D9_COMPOSITING) && !gfxPrefs::LayersPreferD3D9()) {
    aBackends.AppendElement(LayersBackend::LAYERS_D3D9);
  }
}

unsigned
gfxWindowsPlatform::GetD3D11Version()
{
  RefPtr<ID3D11Device> device;
  if (!GetD3D11Device(&device)) {
    return 0;
  }
  return device->GetFeatureLevel();
}

void
gfxWindowsPlatform::GetDeviceInitData(DeviceInitData* aOut)
{
  // Check for device resets before giving back new graphics information.
  UpdateRenderMode();

  gfxPlatform::GetDeviceInitData(aOut);

  // IPDL initializes each field to false for us so we can early return.
  if (!gfxConfig::IsEnabled(Feature::D3D11_COMPOSITING)) {
    return;
  }

  aOut->useD3D11() = true;
  aOut->useD3D11ImageBridge() = !!mD3D11ImageBridgeDevice;
  aOut->d3d11TextureSharingWorks() = mCompositorD3D11TextureSharingWorks;
  aOut->useD3D11WARP() = mIsWARP;
  aOut->useD2D1() = gfxConfig::IsEnabled(Feature::DIRECT2D);

  if (mD3D11Device) {
    DXGI_ADAPTER_DESC desc;
    if (!GetDxgiDesc(mD3D11Device, &desc)) {
      return;
    }
    aOut->adapter() = DxgiAdapterDesc::From(desc);
  }
}

bool
gfxWindowsPlatform::SupportsPluginDirectDXGIDrawing()
{
  if (!GetD3D11ContentDevice() || !CompositorD3D11TextureSharingWorks()) {
    return false;
  }
  return true;
}
