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
#include "mozilla/Sprintf.h"
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

#include "gfxCrashReporterUtils.h"

#include "gfxGDIFontList.h"
#include "gfxGDIFont.h"

#include "mozilla/layers/CompositorThread.h"
#include "mozilla/layers/ReadbackManagerD3D11.h"

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

#include <dwmapi.h>
#include <d3d11.h>

#include "nsIMemoryReporter.h"
#include <winternl.h>
#include "d3dkmtQueryStatistics.h"

#include "base/thread.h"
#include "gfxPrefs.h"
#include "gfxConfig.h"
#include "VsyncSource.h"
#include "DriverCrashGuard.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/gfx/DeviceManagerDx.h"
#include "mozilla/layers/DeviceAttachmentsD3D11.h"
#include "D3D11Checks.h"

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::layers;
using namespace mozilla::widget;
using namespace mozilla::image;

IDWriteRenderingParams* GetDwriteRenderingParams(bool aGDI)
{
  gfxWindowsPlatform::TextRenderingMode mode = aGDI ?
    gfxWindowsPlatform::TEXT_RENDERING_GDI_CLASSIC :
    gfxWindowsPlatform::TEXT_RENDERING_NORMAL;
  return gfxWindowsPlatform::GetPlatform()->GetRenderingParams(mode);
}

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

class GfxD2DVramReporter final : public nsIMemoryReporter
{
    ~GfxD2DVramReporter() {}

public:
    NS_DECL_ISUPPORTS

    NS_IMETHOD CollectReports(nsIHandleReportCallback* aHandleReport,
                              nsISupports* aData, bool aAnonymize) override
    {
        MOZ_COLLECT_REPORT(
            "gfx-d2d-vram-draw-target", KIND_OTHER, UNITS_BYTES,
            Factory::GetD2DVRAMUsageDrawTarget(),
            "Video memory used by D2D DrawTargets.");

        MOZ_COLLECT_REPORT(
            "gfx-d2d-vram-source-surface", KIND_OTHER, UNITS_BYTES,
            Factory::GetD2DVRAMUsageSourceSurface(),
            "Video memory used by D2D SourceSurfaces.");

        return NS_OK;
    }
};

NS_IMPL_ISUPPORTS(GfxD2DVramReporter, nsIMemoryReporter)

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
    CollectReports(nsIHandleReportCallback* aHandleReport, nsISupports* aData,
                   bool aAnonymize) override
    {
        HANDLE ProcessHandle = GetCurrentProcess();

        int64_t dedicatedBytesUsed = 0;
        int64_t sharedBytesUsed = 0;
        int64_t committedBytesUsed = 0;
        IDXGIAdapter *DXGIAdapter;

        HMODULE gdi32Handle;
        PFND3DKMTQS queryD3DKMTStatistics = nullptr;

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

        MOZ_COLLECT_REPORT(
            "gpu-committed", KIND_OTHER, UNITS_BYTES, committedBytesUsed,
            "Memory committed by the Windows graphics system.");

        MOZ_COLLECT_REPORT(
            "gpu-dedicated", KIND_OTHER, UNITS_BYTES,
            dedicatedBytesUsed,
            "Out-of-process memory allocated for this process in a physical "
            "GPU adapter's memory.");

        MOZ_COLLECT_REPORT(
            "gpu-shared", KIND_OTHER, UNITS_BYTES,
            sharedBytesUsed,
            "In-process memory that is shared with the GPU.");

        return NS_OK;
    }
};

NS_IMPL_ISUPPORTS(GPUAdapterReporter, nsIMemoryReporter)

Atomic<size_t> gfxWindowsPlatform::sD3D11SharedTextures;
Atomic<size_t> gfxWindowsPlatform::sD3D9SharedTextures;

class D3DSharedTexturesReporter final : public nsIMemoryReporter
{
  ~D3DSharedTexturesReporter() {}

public:
  NS_DECL_ISUPPORTS

  NS_IMETHOD CollectReports(nsIHandleReportCallback *aHandleReport,
                            nsISupports* aData, bool aAnonymize) override
  {
    if (gfxWindowsPlatform::sD3D11SharedTextures > 0) {
      MOZ_COLLECT_REPORT(
        "d3d11-shared-textures", KIND_OTHER, UNITS_BYTES,
        gfxWindowsPlatform::sD3D11SharedTextures,
        "D3D11 shared textures.");
    }

    if (gfxWindowsPlatform::sD3D9SharedTextures > 0) {
      MOZ_COLLECT_REPORT(
        "d3d9-shared-textures", KIND_OTHER, UNITS_BYTES,
        gfxWindowsPlatform::sD3D9SharedTextures,
        "D3D9 shared textures.");
    }

    return NS_OK;
  }
};

NS_IMPL_ISUPPORTS(D3DSharedTexturesReporter, nsIMemoryReporter)

gfxWindowsPlatform::gfxWindowsPlatform()
  : mRenderMode(RENDER_GDI)
{
  /*
   * Initialize COM
   */
  CoInitialize(nullptr);

  RegisterStrongMemoryReporter(new GfxD2DVramReporter());
  RegisterStrongMemoryReporter(new GPUAdapterReporter());
  RegisterStrongMemoryReporter(new D3DSharedTexturesReporter());
}

gfxWindowsPlatform::~gfxWindowsPlatform()
{
  mozilla::gfx::Factory::D2DCleanup();

  DeviceManagerDx::Shutdown();

  /*
   * Uninitialize COM
   */
  CoUninitialize();
}

static void
UpdateANGLEConfig()
{
  if (!gfxConfig::IsEnabled(Feature::D3D11_COMPOSITING)) {
    gfxConfig::Disable(Feature::D3D11_HW_ANGLE, FeatureStatus::Disabled, "D3D11 compositing is disabled");
  }
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

  DeviceManagerDx::Init();

  InitializeConfig();
  InitializeDevices();
  UpdateANGLEConfig();
  UpdateRenderMode();

  // If we have Skia and we didn't init dwrite already, do it now.
  if (!mDWriteFactory && GetDefaultContentBackend() == BackendType::SKIA) {
    InitDWriteSupport();
  }

  // CanUseHardwareVideoDecoding depends on DeviceManagerDx state,
  // so update the cached value now.
  UpdateCanUseHardwareVideoDecoding();
}

bool
gfxWindowsPlatform::CanUseHardwareVideoDecoding()
{
  DeviceManagerDx* dm = DeviceManagerDx::Get();
  if (!dm) {
    return false;
  }
  if (!dm->TextureSharingWorks()) {
    return false;
  }
  return !dm->IsWARP() && gfxPlatform::CanUseHardwareVideoDecoding();
}

bool
gfxWindowsPlatform::InitDWriteSupport()
{
  // DWrite is only supported on Windows 7 with the platform update and higher.
  // We check this by seeing if D2D1 support is available.
  if (!Factory::SupportsD2D1()) {
    return false;
  }

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
  Factory::SetDWriteFactory(mDWriteFactory);

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

  if (resetReason != DeviceResetReason::FORCED_RESET) {
    Telemetry::Accumulate(Telemetry::DEVICE_RESET_REASON, uint32_t(resetReason));
  }

  // Remove devices and adapters.
  DeviceManagerDx::Get()->ResetDevices();

  imgLoader::NormalLoader()->ClearCache(true);
  imgLoader::NormalLoader()->ClearCache(false);
  imgLoader::PrivateBrowsingLoader()->ClearCache(true);
  imgLoader::PrivateBrowsingLoader()->ClearCache(false);
  gfxAlphaBoxBlur::ShutdownBlurCache();

  gfxConfig::Reset(Feature::D3D11_COMPOSITING);
  gfxConfig::Reset(Feature::ADVANCED_LAYERS);
  gfxConfig::Reset(Feature::D3D11_HW_ANGLE);
  gfxConfig::Reset(Feature::DIRECT2D);

  InitializeConfig();
  InitializeDevices();
  UpdateANGLEConfig();
  return true;
}

void
gfxWindowsPlatform::UpdateBackendPrefs()
{
  uint32_t canvasMask = BackendTypeBit(BackendType::CAIRO) |
                        BackendTypeBit(BackendType::SKIA);
  uint32_t contentMask = BackendTypeBit(BackendType::CAIRO) |
                         BackendTypeBit(BackendType::SKIA);
  BackendType defaultBackend = BackendType::SKIA;
  if (gfxConfig::IsEnabled(Feature::DIRECT2D) && Factory::GetD2D1Device()) {
    contentMask |= BackendTypeBit(BackendType::DIRECT2D1_1);
    canvasMask |= BackendTypeBit(BackendType::DIRECT2D1_1);
    defaultBackend = BackendType::DIRECT2D1_1;
  }
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

mozilla::gfx::BackendType
gfxWindowsPlatform::GetContentBackendFor(mozilla::layers::LayersBackend aLayers)
{
  mozilla::gfx::BackendType defaultBackend = gfxPlatform::GetDefaultContentBackend();
  if (aLayers == LayersBackend::LAYERS_D3D11) {
    return defaultBackend;
  }

  if (defaultBackend == BackendType::DIRECT2D1_1) {
    // We can't have D2D without D3D11 layers, so fallback to Skia.
    return BackendType::SKIA;
  }

  // Otherwise we have some non-accelerated backend and that's ok.
  return defaultBackend;
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
        DisableD2D(FeatureStatus::Failed, "Failed to initialize fonts",
                   NS_LITERAL_CSTRING("FEATURE_FAILURE_FONT_FAIL"));
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
gfxWindowsPlatform::DisableD2D(FeatureStatus aStatus, const char* aMessage,
                               const nsACString& aFailureId)
{
  gfxConfig::SetFailed(Feature::DIRECT2D, aStatus, aMessage, aFailureId);
  Factory::SetDirect3D11Device(nullptr);
  UpdateBackendPrefs();
}

already_AddRefed<gfxASurface>
gfxWindowsPlatform::CreateOffscreenSurface(const IntSize& aSize,
                                           gfxImageFormat aFormat)
{
    if (!Factory::AllowedSurfaceSize(aSize)) {
        return nullptr;
    }

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
                                                    font->GetUnscaledFont(),
                                                    font->GetAdjustedSize(),
                                                    font->GetCairoScaledFont());
        }

        return Factory::CreateScaledFontForNativeFont(nativeFont,
                                                      font->GetUnscaledFont(),
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
                                                aFont->GetUnscaledFont(),
                                                aFont->GetAdjustedSize(),
                                                aFont->GetCairoScaledFont());
    }

    return Factory::CreateScaledFontForNativeFont(nativeFont,
                                                  aFont->GetUnscaledFont(),
                                                  aFont->GetAdjustedSize());
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
static const char kFontEmojiOneMozilla[] = "EmojiOne Mozilla";
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
        aFontList.AppendElement(kFontEmojiOneMozilla);
    }

    // Arial is used as the default fallback for system fallback
    aFontList.AppendElement(kFontArial);

    if (!IS_IN_BMP(aCh)) {
        uint32_t p = aCh >> 16;
        if (p == 1) { // SMP plane
            if (aNextCh == 0xfe0eu) {
                aFontList.AppendElement(kFontSegoeUISymbol);
                aFontList.AppendElement(kFontSegoeUIEmoji);
                aFontList.AppendElement(kFontEmojiOneMozilla);
            } else {
                if (aNextCh != 0xfe0fu) {
                    aFontList.AppendElement(kFontSegoeUIEmoji);
                    aFontList.AppendElement(kFontEmojiOneMozilla);
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
        case 0x0a:
        case 0x0b:
        case 0x0c:
        case 0x0d:
            aFontList.AppendElement(kFontNirmalaUI);
            break;
        case 0x0e:
            aFontList.AppendElement(kFontLaoUI);
            aFontList.AppendElement(kFontLeelawadeeUI);
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
            aFontList.AppendElement(kFontLeelawadeeUI);
            break;
        case 0x18:  // Mongolian
            aFontList.AppendElement(kFontMongolianBaiti);
            aFontList.AppendElement(kFontEuphemia);
            break;
        case 0x19:
            aFontList.AppendElement(kFontMicrosoftTaiLe);
            aFontList.AppendElement(kFontMicrosoftNewTaiLue);
            aFontList.AppendElement(kFontKhmerUI);
            aFontList.AppendElement(kFontLeelawadeeUI);
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
             aFontList.AppendElement(kFontLeelawadeeUI);
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

bool
gfxWindowsPlatform::DidRenderingDeviceReset(DeviceResetReason* aResetReason)
{
  DeviceManagerDx* dm = DeviceManagerDx::Get();
  if (!dm) {
    return false;
  }
  return dm->HasDeviceReset(aResetReason);
}

void
gfxWindowsPlatform::CompositorUpdated()
{
  DeviceManagerDx::Get()->ForceDeviceReset(ForcedDeviceResetReason::COMPOSITOR_UPDATED);
  UpdateRenderMode();
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
  AUTO_PROFILER_LABEL("gfxWindowsPlatform::SchedulePaintIfDeviceReset",
                      GRAPHICS);

  DeviceResetReason resetReason = DeviceResetReason::OK;
  if (!DidRenderingDeviceReset(&resetReason)) {
    return;
  }

  gfxCriticalNote << "(gfxWindowsPlatform) Detected device reset: " << (int)resetReason;

  // Trigger an ::OnPaint for each window.
  ::EnumThreadWindows(GetCurrentThreadId(),
                      InvalidateWindowForDeviceReset,
                      0);

  gfxCriticalNote << "(gfxWindowsPlatform) Finished device reset.";
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

void
gfxWindowsPlatform::GetDLLVersion(char16ptr_t aDLLPath, nsAString& aVersion)
{
    DWORD versInfoSize, vers[4] = {0};
    // version info not available case
    aVersion.AssignLiteral(u"0.0.0.0");
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
    SprintfLiteral(buf, "%u.%u.%u.%u", vers[0], vers[1], vers[2], vers[3]);
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

    if (aPref && !strncmp(GFX_CLEARTYPE_PARAMS, aPref, strlen(GFX_CLEARTYPE_PARAMS))) {
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

#define DISPLAY1_REGISTRY_KEY \
    HKEY_CURRENT_USER, L"Software\\Microsoft\\Avalon.Graphics\\DISPLAY1"

#define ENHANCED_CONTRAST_VALUE_NAME L"EnhancedContrastLevel"

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
        if (contrast < 0.0 || contrast > 10.0) {
            HKEY hKey;
            LONG res = RegOpenKeyExW(DISPLAY1_REGISTRY_KEY,
                                     0, KEY_READ, &hKey);
            if (res == ERROR_SUCCESS) {
                res = RegQueryValueExW(hKey, ENHANCED_CONTRAST_VALUE_NAME,
                                       nullptr, nullptr, nullptr, nullptr);
                if (res == ERROR_SUCCESS) {
                    contrast = defaultRenderingParams->GetEnhancedContrast();
                }
                RegCloseKey(hKey);
            }

            if (contrast < 0.0 || contrast > 10.0) {
                contrast = 1.0;
            }
        }

        if (GetDefaultContentBackend() == BackendType::SKIA) {
          // Skia doesn't support a contrast value outside of 0-1, so default to 1.0
          if (contrast < 0.0 || contrast > 1.0) {
            NS_WARNING("Custom dwrite contrast not supported in Skia. Defaulting to 1.0.");
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

        HRESULT hr = GetDWriteFactory()->CreateCustomRenderingParams(
            gamma, contrast, level, dwriteGeometry, renderMode,
            getter_AddRefs(mRenderingParams[TEXT_RENDERING_NORMAL]));
        if (FAILED(hr) || !mRenderingParams[TEXT_RENDERING_NORMAL]) {
            mRenderingParams[TEXT_RENDERING_NORMAL] = defaultRenderingParams;
        }

        hr = GetDWriteFactory()->CreateCustomRenderingParams(
            gamma, contrast, level,
            dwriteGeometry, DWRITE_RENDERING_MODE_CLEARTYPE_GDI_CLASSIC,
            getter_AddRefs(mRenderingParams[TEXT_RENDERING_GDI_CLASSIC]));
        if (FAILED(hr) || !mRenderingParams[TEXT_RENDERING_GDI_CLASSIC]) {
            mRenderingParams[TEXT_RENDERING_GDI_CLASSIC] =
                defaultRenderingParams;
        }
    }
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

static inline bool
IsWARPStable()
{
  // It seems like nvdxgiwrap makes a mess of WARP. See bug 1154703.
  if (!IsWin8OrLater() || GetModuleHandleA("nvdxgiwrap.dll")) {
    return false;
  }
  return true;
}

static void
InitializeANGLEConfig()
{
  FeatureState& d3d11ANGLE = gfxConfig::GetFeature(Feature::D3D11_HW_ANGLE);

  if (!gfxConfig::IsEnabled(Feature::D3D11_COMPOSITING)) {
    d3d11ANGLE.DisableByDefault(FeatureStatus::Unavailable, "D3D11 compositing is disabled",
                                NS_LITERAL_CSTRING("FEATURE_FAILURE_D3D11_DISABLED"));
    return;
  }

  d3d11ANGLE.EnableByDefault();

  nsCString message;
  nsCString failureId;
  if (!gfxPlatform::IsGfxInfoStatusOkay(nsIGfxInfo::FEATURE_DIRECT3D_11_ANGLE, &message,
                           failureId)) {
    d3d11ANGLE.Disable(FeatureStatus::Blacklisted, message.get(), failureId);
  }

}

void
gfxWindowsPlatform::InitializeDirectDrawConfig()
{
  MOZ_ASSERT(XRE_IsParentProcess());

  FeatureState& ddraw = gfxConfig::GetFeature(Feature::DIRECT_DRAW);
  ddraw.EnableByDefault();
}

void
gfxWindowsPlatform::InitializeConfig()
{
  if (XRE_IsParentProcess()) {
    // The parent process first determines which features can be attempted.
    // This information is relayed to content processes and the GPU process.
    InitializeD3D11Config();
    InitializeANGLEConfig();
    InitializeD2DConfig();
  } else {
    FetchAndImportContentDeviceData();
    InitializeANGLEConfig();
  }
}

void
gfxWindowsPlatform::InitializeD3D11Config()
{
  MOZ_ASSERT(XRE_IsParentProcess());

  FeatureState& d3d11 = gfxConfig::GetFeature(Feature::D3D11_COMPOSITING);

  if (!gfxConfig::IsEnabled(Feature::HW_COMPOSITING)) {
    d3d11.DisableByDefault(FeatureStatus::Unavailable, "Hardware compositing is disabled",
                           NS_LITERAL_CSTRING("FEATURE_FAILURE_D3D11_NEED_HWCOMP"));
    return;
  }

  d3d11.EnableByDefault();

  nsCString message;
  nsCString failureId;
  if (!gfxPlatform::IsGfxInfoStatusOkay(nsIGfxInfo::FEATURE_DIRECT3D_11_LAYERS, &message, failureId)) {
    d3d11.Disable(FeatureStatus::Blacklisted, message.get(), failureId);
  }

  // Check if the user really, really wants WARP.
  if (gfxPrefs::LayersD3D11ForceWARP()) {
    // Force D3D11 on even if we disabled it.
    d3d11.UserForceEnable("User force-enabled WARP");
  }

  InitializeAdvancedLayersConfig();
}

/* static */ void
gfxWindowsPlatform::InitializeAdvancedLayersConfig()
{
  // Only enable Advanced Layers if D3D11 succeeded.
  if (!gfxConfig::IsEnabled(Feature::D3D11_COMPOSITING)) {
    return;
  }

  FeatureState& al = gfxConfig::GetFeature(Feature::ADVANCED_LAYERS);
  al.SetDefaultFromPref(
    gfxPrefs::GetAdvancedLayersEnabledDoNotUseDirectlyPrefName(),
    true /* aIsEnablePref */,
    gfxPrefs::GetAdvancedLayersEnabledDoNotUseDirectlyPrefDefault());

  // Windows 7 has an extra pref since it uses totally different buffer paths
  // that haven't been performance tested yet.
  if (al.IsEnabled() && !IsWin8OrLater()) {
    if (gfxPrefs::AdvancedLayersEnableOnWindows7()) {
      al.UserEnable("Enabled for Windows 7 via user-preference");
    } else {
      al.Disable(FeatureStatus::Disabled,
                 "Advanced Layers is disabled on Windows 7 by default",
                 NS_LITERAL_CSTRING("FEATURE_FAILURE_DISABLED_ON_WIN7"));
    }
  }

  nsCString message, failureId;
  if (!IsGfxInfoStatusOkay(nsIGfxInfo::FEATURE_ADVANCED_LAYERS, &message, failureId)) {
    al.Disable(FeatureStatus::Blacklisted, message.get(), failureId);
  } else if (Preferences::GetBool("layers.mlgpu.sanity-test-failed", false)) {
    al.Disable(
      FeatureStatus::Broken,
      "Failed to render sanity test",
      NS_LITERAL_CSTRING("FEATURE_FAILURE_FAILED_TO_RENDER"));
  }
}

/* static */ void
gfxWindowsPlatform::RecordContentDeviceFailure(TelemetryDeviceCode aDevice)
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

void
gfxWindowsPlatform::InitializeDevices()
{
  if (XRE_IsParentProcess()) {
    // If we're the UI process, and the GPU process is enabled, then we don't
    // initialize any DirectX devices. We do leave them enabled in gfxConfig
    // though. If the GPU process fails to create these devices it will send
    // a message back and we'll update their status.
    if (InitGPUProcessSupport()) {
      return;
    }

    // No GPU process, continue initializing devices as normal.
  }

  // If acceleration is disabled, we refuse to initialize anything.
  if (!gfxConfig::IsEnabled(Feature::HW_COMPOSITING)) {
    return;
  }

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

  bool shouldUseD2D = gfxConfig::IsEnabled(Feature::DIRECT2D);

  // First, initialize D3D11. If this succeeds we attempt to use Direct2D.
  InitializeD3D11();
  InitializeD2D();

  if (!gfxConfig::IsEnabled(Feature::DIRECT2D) &&
      XRE_IsContentProcess() &&
      shouldUseD2D)
  {
    RecordContentDeviceFailure(TelemetryDeviceCode::D2D1);
  }
}

void
gfxWindowsPlatform::InitializeD3D11()
{
  // This function attempts to initialize our D3D11 devices, if the hardware
  // is not blacklisted for D3D11 layers. This first attempt will try to create
  // a hardware accelerated device. If this creation fails or the hardware is
  // blacklisted, then this function will abort if WARP is disabled, causing us
  // to fallback to Basic layers. If WARP is not disabled it will use a WARP
  // device which should always be available on Windows 7 and higher.
  if (!gfxConfig::IsEnabled(Feature::D3D11_COMPOSITING)) {
    return;
  }

  DeviceManagerDx* dm = DeviceManagerDx::Get();
  if (XRE_IsParentProcess()) {
    if (!dm->CreateCompositorDevices()) {
      return;
    }
  }

  dm->CreateContentDevices();

  // Content process failed to create the d3d11 device while parent process
  // succeed.
  if (XRE_IsContentProcess() &&
      !gfxConfig::IsEnabled(Feature::D3D11_COMPOSITING)) {
    gfxCriticalError() << "[D3D11] Failed to create the D3D11 device in content \
                           process.";
  }
}

void
gfxWindowsPlatform::InitializeD2DConfig()
{
  FeatureState& d2d1 = gfxConfig::GetFeature(Feature::DIRECT2D);

  if (!gfxConfig::IsEnabled(Feature::D3D11_COMPOSITING)) {
    d2d1.DisableByDefault(FeatureStatus::Unavailable, "Direct2D requires Direct3D 11 compositing",
                          NS_LITERAL_CSTRING("FEATURE_FAILURE_D2D_D3D11_COMP"));
    return;
  }

  d2d1.SetDefaultFromPref(
    gfxPrefs::GetDirect2DDisabledPrefName(),
    false,
    gfxPrefs::GetDirect2DDisabledPrefDefault());

  nsCString message;
  nsCString failureId;
  if (!gfxPlatform::IsGfxInfoStatusOkay(nsIGfxInfo::FEATURE_DIRECT2D, &message, failureId)) {
    d2d1.Disable(FeatureStatus::Blacklisted, message.get(), failureId);
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

  DeviceManagerDx* dm = DeviceManagerDx::Get();

  // We don't know this value ahead of time, but the user can force-override
  // it, so we use Disable instead of SetFailed.
  if (dm->IsWARP()) {
    d2d1.Disable(FeatureStatus::Blocked, "Direct2D is not compatible with Direct3D11 WARP",
                 NS_LITERAL_CSTRING("FEATURE_FAILURE_D2D_WARP_BLOCK"));
  }

  // If we pass all the initial checks, we can proceed to runtime decisions.
  if (!d2d1.IsEnabled()) {
    return;
  }

  if (!Factory::SupportsD2D1()) {
    d2d1.SetFailed(FeatureStatus::Unavailable, "Failed to acquire a Direct2D 1.1 factory",
                   NS_LITERAL_CSTRING("FEATURE_FAILURE_D2D_FACTORY"));
    return;
  }

  if (!dm->GetContentDevice()) {
    d2d1.SetFailed(FeatureStatus::Failed, "Failed to acquire a Direct3D 11 content device",
                   NS_LITERAL_CSTRING("FEATURE_FAILURE_D2D_DEVICE"));
    return;
  }

  if (!dm->TextureSharingWorks()) {
    d2d1.SetFailed(FeatureStatus::Failed, "Direct3D11 device does not support texture sharing",
                   NS_LITERAL_CSTRING("FEATURE_FAILURE_D2D_TXT_SHARING"));
    return;
  }

  // Using Direct2D depends on DWrite support.
  if (!mDWriteFactory && !InitDWriteSupport()) {
    d2d1.SetFailed(FeatureStatus::Failed, "Failed to initialize DirectWrite support",
                   NS_LITERAL_CSTRING("FEATURE_FAILURE_D2D_DWRITE"));
    return;
  }

  // Verify that Direct2D device creation succeeded.
  RefPtr<ID3D11Device> contentDevice = dm->GetContentDevice();
  if (!Factory::SetDirect3D11Device(contentDevice)) {
    d2d1.SetFailed(FeatureStatus::Failed, "Failed to create a Direct2D device",
                   NS_LITERAL_CSTRING("FEATURE_FAILURE_D2D_CREATE_FAILED"));
    return;
  }

  MOZ_ASSERT(d2d1.IsEnabled());
  d2d1_1.SetSuccessful();
}

bool
gfxWindowsPlatform::InitGPUProcessSupport()
{
  FeatureState& gpuProc = gfxConfig::GetFeature(Feature::GPU_PROCESS);

  if (!gpuProc.IsEnabled()) {
    return false;
  }

  if (!gfxConfig::IsEnabled(Feature::D3D11_COMPOSITING)) {
    // Don't use the GPU process if not using D3D11, unless software
    // compositor is allowed
    if (gfxPrefs::GPUProcessAllowSoftware()) {
      return gpuProc.IsEnabled();
    }
    gpuProc.Disable(
      FeatureStatus::Unavailable,
      "Not using GPU Process since D3D11 is unavailable",
      NS_LITERAL_CSTRING("FEATURE_FAILURE_NO_D3D11"));
  } else if (!IsWin7SP1OrLater()) {
    // On Windows 7 Pre-SP1, DXGI 1.2 is not available and remote presentation
    // for D3D11 will not work. Rather than take a regression we revert back
    // to in-process rendering.
    gpuProc.Disable(
      FeatureStatus::Unavailable,
      "Windows 7 Pre-SP1 cannot use the GPU process",
      NS_LITERAL_CSTRING("FEATURE_FAILURE_OLD_WINDOWS"));
  } else if (!IsWin8OrLater()) {
    // Windows 7 SP1 can have DXGI 1.2 only via the Platform Update, so we
    // explicitly check for that here.
    if (!DeviceManagerDx::Get()->CheckRemotePresentSupport()) {
      gpuProc.Disable(
        FeatureStatus::Unavailable,
        "GPU Process requires the Windows 7 Platform Update",
        NS_LITERAL_CSTRING("FEATURE_FAILURE_PLATFORM_UPDATE"));
    } else {
      // Clear anything cached by the above call since we don't need it.
      DeviceManagerDx::Get()->ResetDevices();
    }
  }

  // If we're still enabled at this point, the user set the force-enabled pref.
  return gpuProc.IsEnabled();
}

bool
gfxWindowsPlatform::DwmCompositionEnabled()
{
  BOOL dwmEnabled = false;

  if (FAILED(DwmIsCompositionEnabled(&dwmEnabled))) {
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
        MOZ_RELEASE_ASSERT(mVsyncThread->Start(), "GFX: Could not start Windows vsync thread");
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
        HRESULT hr = DwmGetCompositionTimingInfo(0, &vblankTime);
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

      virtual void Shutdown() override
      {
        MOZ_ASSERT(NS_IsMainThread());
        DisableVsync();
        mVsyncThread->Stop();
        delete mVsyncThread;
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
            NewRunnableMethod("D3DVsyncDisplay::VBlankLoop",
                              this, &D3DVsyncDisplay::VBlankLoop));
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

        TimeStamp nextVsync = aVsyncTimestamp + mVsyncRate;
        TimeDuration delay = nextVsync - TimeStamp::Now();
        if (delay.ToMilliseconds() < 0) {
          delay = mozilla::TimeDuration::FromMilliseconds(0);
        }

        mVsyncThread->message_loop()->PostDelayedTask(
            NewRunnableMethod("D3DVsyncDisplay::VBlankLoop",
                              this, &D3DVsyncDisplay::VBlankLoop),
            delay.ToMilliseconds());
      }

      // Returns the timestamp for the just happened vsync
      TimeStamp GetVBlankTime()
      {
        TimeStamp vsync = TimeStamp::Now();
        TimeStamp now = vsync;

        DWM_TIMING_INFO vblankTime;
        // Make sure to init the cbSize, otherwise
        // GetCompositionTiming will fail
        vblankTime.cbSize = sizeof(DWM_TIMING_INFO);
        HRESULT hr = DwmGetCompositionTimingInfo(0, &vblankTime);
        if (!SUCCEEDED(hr)) {
            return vsync;
        }

        LARGE_INTEGER frequency;
        QueryPerformanceFrequency(&frequency);

        LARGE_INTEGER qpcNow;
        QueryPerformanceCounter(&qpcNow);

        const int microseconds = 1000000;
        int64_t adjust = qpcNow.QuadPart - vblankTime.qpcVBlank;
        int64_t usAdjust = (adjust * microseconds) / frequency.QuadPart;
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
          // the previously seen vsync.
          // In these error cases, normalize to Now();
          if (vsync >= now) {
            vsync = vsync - mVsyncRate;
          }
        }

        // On Windows 7 and 8, DwmFlush wakes up AFTER qpcVBlankTime
        // from DWMGetCompositionTimingInfo. We can return the adjusted vsync.
        if (vsync >= now) {
            vsync = now;
        }

        // Our vsync time is some time very far in the past, adjust to Now.
        // 4 ms is arbitrary, so feel free to pick something else if this isn't
        // working. See the comment above within IsWin10OrLater().
        if ((now - vsync).ToMilliseconds() > 4.0) {
            vsync = now;
        }

        return vsync;
      }

      void VBlankLoop()
      {
        MOZ_ASSERT(IsInVsyncThread());
        MOZ_ASSERT(sizeof(int64_t) == sizeof(QPC_TIME));

        TimeStamp vsync = TimeStamp::Now();
        mPrevVsync = TimeStamp();
        TimeStamp flushTime = TimeStamp::Now();
        TimeDuration longVBlank = mVsyncRate * 2;

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

          // Using WaitForVBlank, the whole system dies because WaitForVBlank
          // only works if it's run on the same thread as the Present();
          HRESULT hr = DwmFlush();
          if (!SUCCEEDED(hr)) {
            // DWMFlush isn't working, fallback to software vsync.
            ScheduleSoftwareVsync(TimeStamp::Now());
            return;
          }

          TimeStamp now = TimeStamp::Now();
          TimeDuration flushDiff = now - flushTime;
          flushTime = now;
          if ((flushDiff > longVBlank) || mPrevVsync.IsNull()) {
            // Our vblank took longer than 2 intervals, readjust our timestamps
            vsync = GetVBlankTime();
            mPrevVsync = vsync;
          } else {
            // Instead of giving the actual vsync time, a constant interval
            // between vblanks instead of the noise generated via hardware
            // is actually what we want. Most apps just care about the diff
            // between vblanks to animate, so a clean constant interval is
            // smoother.
            vsync = mPrevVsync + mVsyncRate;
            if (vsync > now) {
              // DWMFlush woke up very early, so readjust our times again
              vsync = GetVBlankTime();
            }

            if (vsync <= mPrevVsync) {
              vsync = TimeStamp::Now();
            }

            if ((now - vsync).ToMilliseconds() > 2.0) {
              // Account for time drift here where vsync never quite catches up to
              // Now and we'd fall ever so slightly further behind Now().
              vsync = GetVBlankTime();
            }

            mPrevVsync = vsync;
          }
        } // end for
      }

    private:
      virtual ~D3DVsyncDisplay()
      {
        MOZ_ASSERT(NS_IsMainThread());
      }

      bool IsInVsyncThread()
      {
        return mVsyncThread->thread_id() == PlatformThread::CurrentId();
      }

      TimeStamp mPrevVsync;
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
  MOZ_RELEASE_ASSERT(NS_IsMainThread(), "GFX: Not in main thread.");

  BOOL dwmEnabled = false;
  DwmIsCompositionEnabled(&dwmEnabled);
  if (!dwmEnabled) {
    NS_WARNING("DWM not enabled, falling back to software vsync");
    return gfxPlatform::CreateHardwareVsyncSource();
  }

  RefPtr<VsyncSource> d3dVsyncSource = new D3DVsyncSource();
  return d3dVsyncSource.forget();
}

void
gfxWindowsPlatform::GetAcceleratedCompositorBackends(nsTArray<LayersBackend>& aBackends)
{
  if (gfxConfig::IsEnabled(Feature::OPENGL_COMPOSITING) && gfxPrefs::LayersPreferOpenGL()) {
    aBackends.AppendElement(LayersBackend::LAYERS_OPENGL);
  }

  if (gfxConfig::IsEnabled(Feature::D3D11_COMPOSITING)) {
    aBackends.AppendElement(LayersBackend::LAYERS_D3D11);
  }
}

void
gfxWindowsPlatform::ImportGPUDeviceData(const mozilla::gfx::GPUDeviceData& aData)
{
  MOZ_ASSERT(XRE_IsParentProcess());

  gfxPlatform::ImportGPUDeviceData(aData);

  gfxConfig::ImportChange(Feature::D3D11_COMPOSITING, aData.d3d11Compositing());

  DeviceManagerDx* dm = DeviceManagerDx::Get();
  if (gfxConfig::IsEnabled(Feature::D3D11_COMPOSITING)) {
    dm->ImportDeviceInfo(aData.gpuDevice().get_D3D11DeviceStatus());
  } else {
    // There should be no devices, so this just takes away the device status.
    dm->ResetDevices();

    // Make sure we disable D2D if content processes might use it.
    FeatureState& d2d1 = gfxConfig::GetFeature(Feature::DIRECT2D);
    if (d2d1.IsEnabled()) {
      d2d1.SetFailed(
        FeatureStatus::Unavailable,
        "Direct2D requires Direct3D 11 compositing",
        NS_LITERAL_CSTRING("FEATURE_FAILURE_D2D_D3D11_COMP"));
    }
  }

  // CanUseHardwareVideoDecoding depends on d3d11 state, so update
  // the cached value now.
  UpdateCanUseHardwareVideoDecoding();

  // For completeness (and messaging in about:support). Content recomputes this
  // on its own, and we won't use ANGLE in the UI process if we're using a GPU
  // process.
  UpdateANGLEConfig();
}

void
gfxWindowsPlatform::ImportContentDeviceData(const mozilla::gfx::ContentDeviceData& aData)
{
  MOZ_ASSERT(XRE_IsContentProcess());

  gfxPlatform::ImportContentDeviceData(aData);

  const DevicePrefs& prefs = aData.prefs();
  gfxConfig::Inherit(Feature::D3D11_COMPOSITING, prefs.d3d11Compositing());
  gfxConfig::Inherit(Feature::DIRECT2D, prefs.useD2D1());

  if (gfxConfig::IsEnabled(Feature::D3D11_COMPOSITING)) {
    DeviceManagerDx* dm = DeviceManagerDx::Get();
    dm->ImportDeviceInfo(aData.d3d11());
  }
}

void
gfxWindowsPlatform::BuildContentDeviceData(ContentDeviceData* aOut)
{
  // Check for device resets before giving back new graphics information.
  UpdateRenderMode();

  gfxPlatform::BuildContentDeviceData(aOut);

  const FeatureState& d3d11 = gfxConfig::GetFeature(Feature::D3D11_COMPOSITING);
  aOut->prefs().d3d11Compositing() = d3d11.GetValue();
  aOut->prefs().useD2D1() = gfxConfig::GetValue(Feature::DIRECT2D);

  if (d3d11.IsEnabled()) {
    DeviceManagerDx* dm = DeviceManagerDx::Get();
    dm->ExportDeviceInfo(&aOut->d3d11());
  }
}

bool
gfxWindowsPlatform::SupportsPluginDirectDXGIDrawing()
{
  DeviceManagerDx* dm = DeviceManagerDx::Get();
  if (!dm->GetContentDevice() || !dm->TextureSharingWorks()) {
    return false;
  }
  return true;
}
