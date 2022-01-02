/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_WINDOWS_PLATFORM_H
#define GFX_WINDOWS_PLATFORM_H

#include "gfxCrashReporterUtils.h"
#include "gfxFontUtils.h"
#include "gfxWindowsSurface.h"
#include "gfxFont.h"
#include "gfxDWriteFonts.h"
#include "gfxPlatform.h"
#include "gfxTelemetry.h"
#include "gfxTypes.h"
#include "mozilla/Attributes.h"
#include "mozilla/Atomics.h"
#include "nsTArray.h"

#include "mozilla/Mutex.h"
#include "mozilla/RefPtr.h"

#include <windows.h>
#include <objbase.h>

#include <dxgi.h>

// This header is available in the June 2010 SDK and in the Win8 SDK
#include <d3dcommon.h>
// Win 8.0 SDK types we'll need when building using older sdks.
#if !defined(D3D_FEATURE_LEVEL_11_1)  // defined in the 8.0 SDK only
#  define D3D_FEATURE_LEVEL_11_1 static_cast<D3D_FEATURE_LEVEL>(0xb100)
#  define D3D_FL9_1_REQ_TEXTURE2D_U_OR_V_DIMENSION 2048
#  define D3D_FL9_3_REQ_TEXTURE2D_U_OR_V_DIMENSION 4096
#endif

namespace mozilla {
namespace gfx {
class DrawTarget;
class FeatureState;
class DeviceManagerDx;
}  // namespace gfx
}  // namespace mozilla
struct IDirect3DDevice9;
struct ID3D11Device;
struct IDXGIAdapter1;

/**
 * Utility to get a Windows HDC from a Moz2D DrawTarget.  If the DrawTarget is
 * not backed by a HDC this will get the HDC for the screen device context
 * instead.
 */
class MOZ_STACK_CLASS DCForMetrics final {
 public:
  explicit DCForMetrics();

  ~DCForMetrics() { ReleaseDC(nullptr, mDC); }

  operator HDC() { return mDC; }

 private:
  HDC mDC;
};

// ClearType parameters set by running ClearType tuner
struct ClearTypeParameterInfo {
  ClearTypeParameterInfo()
      : gamma(-1),
        pixelStructure(-1),
        clearTypeLevel(-1),
        enhancedContrast(-1) {}

  nsString displayName;  // typically just 'DISPLAY1'
  int32_t gamma;
  int32_t pixelStructure;
  int32_t clearTypeLevel;
  int32_t enhancedContrast;
};

class gfxWindowsPlatform final : public gfxPlatform {
  friend class mozilla::gfx::DeviceManagerDx;

 public:
  enum TextRenderingMode {
    TEXT_RENDERING_NO_CLEARTYPE,
    TEXT_RENDERING_NORMAL,
    TEXT_RENDERING_GDI_CLASSIC,
    TEXT_RENDERING_COUNT
  };

  gfxWindowsPlatform();
  virtual ~gfxWindowsPlatform();
  static gfxWindowsPlatform* GetPlatform() {
    return (gfxWindowsPlatform*)gfxPlatform::GetPlatform();
  }

  void EnsureDevicesInitialized() override;
  bool DevicesInitialized() override;

  bool CreatePlatformFontList() override;

  virtual already_AddRefed<gfxASurface> CreateOffscreenSurface(
      const IntSize& aSize, gfxImageFormat aFormat) override;

  enum RenderMode {
    /* Use GDI and windows surfaces */
    RENDER_GDI = 0,

    /* Use 32bpp image surfaces and call StretchDIBits */
    RENDER_IMAGE_STRETCH32,

    /* Use 32bpp image surfaces, and do 32->24 conversion before calling
       StretchDIBits */
    RENDER_IMAGE_STRETCH24,

    /* Use Direct2D rendering */
    RENDER_DIRECT2D,

    /* max */
    RENDER_MODE_MAX
  };

  bool IsDirect2DBackend();

  /**
   * Updates render mode with relation to the current preferences and
   * available devices.
   */
  void UpdateRenderMode();

  /**
   * Verifies a D2D device is present and working, will attempt to create one
   * it is non-functional or non-existant.
   *
   * \param aAttemptForce Attempt to force D2D cairo device creation by using
   * cairo device creation routines.
   */
  void VerifyD2DDevice(bool aAttemptForce);

  void GetCommonFallbackFonts(uint32_t aCh, Script aRunScript,
                              eFontPresentation aPresentation,
                              nsTArray<const char*>& aFontList) override;

  bool CanUseHardwareVideoDecoding() override;

  void CompositorUpdated() override;

  bool DidRenderingDeviceReset(
      DeviceResetReason* aResetReason = nullptr) override;
  void SchedulePaintIfDeviceReset() override;
  void CheckForContentOnlyDeviceReset();

  mozilla::gfx::BackendType GetContentBackendFor(
      mozilla::layers::LayersBackend aLayers) override;

  mozilla::gfx::BackendType GetPreferredCanvasBackend() override;

  static void GetDLLVersion(char16ptr_t aDLLPath, nsAString& aVersion);

  // returns ClearType tuning information for each display
  static void GetCleartypeParams(nsTArray<ClearTypeParameterInfo>& aParams);

  void FontsPrefsChanged(const char* aPref) override;

  static inline bool DWriteEnabled() {
    return !!mozilla::gfx::Factory::GetDWriteFactory();
  }

 public:
  static nsresult GetGpuTimeSinceProcessStartInMs(uint64_t* aResult);

  bool DwmCompositionEnabled();

  static bool IsOptimus();

  bool SupportsApzWheelInput() const override { return true; }

  // Recreate devices as needed for a device reset. Returns true if a device
  // reset occurred.
  bool HandleDeviceReset();
  void UpdateBackendPrefs();

  already_AddRefed<mozilla::gfx::VsyncSource> CreateHardwareVsyncSource()
      override;
  static mozilla::Atomic<size_t> sD3D11SharedTextures;
  static mozilla::Atomic<size_t> sD3D9SharedTextures;

  bool SupportsPluginDirectBitmapDrawing() override { return true; }

  static void RecordContentDeviceFailure(
      mozilla::gfx::TelemetryDeviceCode aDevice);

  static void InitMemoryReportersForGPUProcess();

 protected:
  bool AccelerateLayersByDefault() override { return true; }
  nsTArray<uint8_t> GetPlatformCMSOutputProfileData() override;
  void GetPlatformDisplayInfo(mozilla::widget::InfoObject& aObj) override;

  void ImportGPUDeviceData(const mozilla::gfx::GPUDeviceData& aData) override;
  void ImportContentDeviceData(
      const mozilla::gfx::ContentDeviceData& aData) override;
  void BuildContentDeviceData(mozilla::gfx::ContentDeviceData* aOut) override;

  BackendPrefsData GetBackendPrefs() const override;

  bool CheckVariationFontSupport() override;

 protected:
  RenderMode mRenderMode;

 private:
  enum class DwmCompositionStatus : uint32_t {
    Unknown,
    Disabled,
    Enabled,
  };

  void Init();
  void InitAcceleration() override;
  void InitWebRenderConfig() override;

  void InitializeDevices();
  void InitializeD3D11();
  void InitializeD2D();
  bool InitDWriteSupport();
  void InitGPUProcessSupport();

  void DisableD2D(mozilla::gfx::FeatureStatus aStatus, const char* aMessage,
                  const nsACString& aFailureId);

  void InitializeConfig();
  void InitializeD3D9Config();
  void InitializeD3D11Config();
  void InitializeD2DConfig();
  void InitializeDirectDrawConfig();

  void RecordStartupTelemetry();

  bool mInitializedDevices = false;

  mozilla::Atomic<DwmCompositionStatus, mozilla::ReleaseAcquire>
      mDwmCompositionStatus;

  // Cached contents of the output color profile file
  nsTArray<uint8_t> mCachedOutputColorProfile;
};

#endif /* GFX_WINDOWS_PLATFORM_H */
