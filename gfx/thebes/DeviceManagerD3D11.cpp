/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DeviceManagerD3D11.h"
#include "D3D11Checks.h"
#include "gfxConfig.h"
#include "GfxDriverInfo.h"
#include "gfxPrefs.h"
#include "gfxWindowsPlatform.h"
#include "mozilla/D3DMessageUtils.h"
#include "mozilla/Telemetry.h"
#include "mozilla/WindowsVersion.h"
#include "mozilla/gfx/GraphicsMessages.h"
#include "mozilla/gfx/Logging.h"
#include "nsIGfxInfo.h"
#include "nsWindowsHelpers.h"
#include <d3d11.h>

namespace mozilla {
namespace gfx {

using namespace mozilla::widget;

StaticAutoPtr<DeviceManagerD3D11> DeviceManagerD3D11::sInstance;

// We don't have access to the D3D11CreateDevice type in gfxWindowsPlatform.h,
// since it doesn't include d3d11.h, so we use a static here. It should only
// be used within InitializeD3D11.
decltype(D3D11CreateDevice)* sD3D11CreateDeviceFn = nullptr;

/* static */ void
DeviceManagerD3D11::Init()
{
  sInstance = new DeviceManagerD3D11();
}

/* static */ void
DeviceManagerD3D11::Shutdown()
{
  sInstance = nullptr;
}

DeviceManagerD3D11::DeviceManagerD3D11()
 : mDeviceLock("gfxWindowsPlatform.mDeviceLock"),
   mIsWARP(false),
   mTextureSharingWorks(false)
{
  // Set up the D3D11 feature levels we can ask for.
  if (IsWin8OrLater()) {
    mFeatureLevels.AppendElement(D3D_FEATURE_LEVEL_11_1);
  }
  mFeatureLevels.AppendElement(D3D_FEATURE_LEVEL_11_0);
  mFeatureLevels.AppendElement(D3D_FEATURE_LEVEL_10_1);
  mFeatureLevels.AppendElement(D3D_FEATURE_LEVEL_10_0);
  mFeatureLevels.AppendElement(D3D_FEATURE_LEVEL_9_3);
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

bool
DeviceManagerD3D11::CanUseD3D11ImageBridge()
{
  if (XRE_IsContentProcess()) {
    if (!gfxPlatform::GetPlatform()->GetParentDevicePrefs().useD3D11ImageBridge()) {
      return false;
    }
  }
  return !mIsWARP;
}

void
DeviceManagerD3D11::CreateDevices()
{
  FeatureState& d3d11 = gfxConfig::GetFeature(Feature::D3D11_COMPOSITING);
  MOZ_ASSERT(d3d11.IsEnabled());

  nsModuleHandle d3d11Module(LoadLibrarySystem32(L"d3d11.dll"));
  sD3D11CreateDeviceFn =
    (decltype(D3D11CreateDevice)*)GetProcAddress(d3d11Module, "D3D11CreateDevice");

  if (!sD3D11CreateDeviceFn) {
    // We should just be on Windows Vista or XP in this case.
    d3d11.SetFailed(FeatureStatus::Unavailable, "Direct3D11 not available on this computer",
                    NS_LITERAL_CSTRING("FEATURE_FAILURE_D3D11_LIB"));
    return;
  }

  // Check if a failure was injected for testing.
  if (gfxPrefs::DeviceFailForTesting()) {
    d3d11.SetFailed(FeatureStatus::Failed, "Direct3D11 device failure simulated by preference",
                    NS_LITERAL_CSTRING("FEATURE_FAILURE_D3D11_SIM"));
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
      if (!mCompositorDevice && IsWARPStable() && !gfxPrefs::LayersD3D11DisableWARP()) {
        gfxConfig::Reenable(Feature::D3D11_COMPOSITING, Fallback::USE_D3D11_WARP_COMPOSITOR);
      }
    }

    // If that failed, see if we can use WARP.
    if (gfxConfig::UseFallback(Fallback::USE_D3D11_WARP_COMPOSITOR)) {
      MOZ_ASSERT(d3d11.IsEnabled());
      MOZ_ASSERT(!mCompositorDevice);
      MOZ_ASSERT(IsWARPStable() || gfxPrefs::LayersD3D11ForceWARP());

      AttemptWARPDeviceCreation();
      if (d3d11.GetValue() == FeatureStatus::CrashedInHandler) {
        return;
      }
    }

    // If we still have no device by now, exit.
    if (!mCompositorDevice) {
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
    mTextureSharingWorks =
      gfxPlatform::GetPlatform()->GetParentDevicePrefs().d3d11TextureSharingWorks();
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

IDXGIAdapter1*
DeviceManagerD3D11::GetDXGIAdapter()
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
    const DxgiAdapterDesc& parent =
      gfxPlatform::GetPlatform()->GetParentDevicePrefs().adapter();

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

bool
DeviceManagerD3D11::AttemptD3D11DeviceCreationHelper(
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
DeviceManagerD3D11::AttemptD3D11DeviceCreation(FeatureState& d3d11)
{
  RefPtr<IDXGIAdapter1> adapter = GetDXGIAdapter();
  if (!adapter) {
    d3d11.SetFailed(FeatureStatus::Unavailable, "Failed to acquire a DXGI adapter",
                    NS_LITERAL_CSTRING("FEATURE_FAILURE_D3D11_DXGI"));
    return;
  }

  HRESULT hr;
  RefPtr<ID3D11Device> device;
  if (!AttemptD3D11DeviceCreationHelper(adapter, device, hr)) {
    gfxCriticalError() << "Crash during D3D11 device creation";
    d3d11.SetFailed(FeatureStatus::CrashedInHandler, "Crashed trying to acquire a D3D11 device",
                    NS_LITERAL_CSTRING("FEATURE_FAILURE_D3D11_DEVICE1"));
    return;
  }

  if (FAILED(hr) || !device) {
    gfxCriticalError() << "D3D11 device creation failed: " << hexa(hr);
    d3d11.SetFailed(FeatureStatus::Failed, "Failed to acquire a D3D11 device",
                    NS_LITERAL_CSTRING("FEATURE_FAILURE_D3D11_DEVICE2"));
    return;
  }
  if (!D3D11Checks::DoesDeviceWork()) {
    d3d11.SetFailed(FeatureStatus::Broken, "Direct3D11 device was determined to be broken",
                    NS_LITERAL_CSTRING("FEATURE_FAILURE_D3D11_BROKEN"));
    return;
  }

  {
    MutexAutoLock lock(mDeviceLock);
    mCompositorDevice = device;
  }

  // Only test this when not using WARP since it can fail and cause
  // GetDeviceRemovedReason to return weird values.
  mTextureSharingWorks = D3D11Checks::DoesTextureSharingWork(mCompositorDevice);

  if (!mTextureSharingWorks) {
    gfxConfig::SetFailed(Feature::D3D11_HW_ANGLE,
                         FeatureStatus::Broken,
                         "Texture sharing doesn't work");
  }

  if (D3D11Checks::DoesRenderTargetViewNeedRecreating(mCompositorDevice)) {
    gfxConfig::SetFailed(Feature::D3D11_HW_ANGLE,
                         FeatureStatus::Broken,
                         "RenderTargetViews need recreating");
  }

  // It seems like this may only happen when we're using the NVIDIA gpu
  D3D11Checks::WarnOnAdapterMismatch(mCompositorDevice);

  mCompositorDevice->SetExceptionMode(0);
  mIsWARP = false;
}

bool
DeviceManagerD3D11::AttemptWARPDeviceCreationHelper(
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
DeviceManagerD3D11::AttemptWARPDeviceCreation()
{
  ScopedGfxFeatureReporter reporterWARP("D3D11-WARP", gfxPrefs::LayersD3D11ForceWARP());
  FeatureState& d3d11 = gfxConfig::GetFeature(Feature::D3D11_COMPOSITING);

  HRESULT hr;
  RefPtr<ID3D11Device> device;
  if (!AttemptWARPDeviceCreationHelper(reporterWARP, device, hr)) {
    gfxCriticalError() << "Exception occurred initializing WARP D3D11 device!";
    d3d11.SetFailed(FeatureStatus::CrashedInHandler, "Crashed creating a D3D11 WARP device",
                     NS_LITERAL_CSTRING("FEATURE_FAILURE_D3D11_WARP_DEVICE"));
    return;
  }

  if (FAILED(hr) || !device) {
    // This should always succeed... in theory.
    gfxCriticalError() << "Failed to initialize WARP D3D11 device! " << hexa(hr);
    d3d11.SetFailed(FeatureStatus::Failed, "Failed to create a D3D11 WARP device",
                    NS_LITERAL_CSTRING("FEATURE_FAILURE_D3D11_WARP_DEVICE2"));
    return;
  }

  {
    MutexAutoLock lock(mDeviceLock);
    mCompositorDevice = device;
  }

  // Only test for texture sharing on Windows 8 since it puts the device into
  // an unusable state if used on Windows 7
  if (IsWin8OrLater()) {
    mTextureSharingWorks = D3D11Checks::DoesTextureSharingWork(mCompositorDevice);
  }
  mCompositorDevice->SetExceptionMode(0);
  mIsWARP = true;
}

bool
DeviceManagerD3D11::AttemptD3D11ContentDeviceCreationHelper(
  IDXGIAdapter1* aAdapter, HRESULT& aResOut)
{
  MOZ_SEH_TRY {
    aResOut =
      sD3D11CreateDeviceFn(
        aAdapter, mIsWARP ? D3D_DRIVER_TYPE_WARP : D3D_DRIVER_TYPE_UNKNOWN,
        nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        mFeatureLevels.Elements(), mFeatureLevels.Length(),
        D3D11_SDK_VERSION, getter_AddRefs(mContentDevice), nullptr, nullptr);

  } MOZ_SEH_EXCEPT (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
  return true;
}

FeatureStatus
DeviceManagerD3D11::AttemptD3D11ContentDeviceCreation()
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
    gfxWindowsPlatform::RecordContentDeviceFailure(TelemetryDeviceCode::Content);
    return FeatureStatus::CrashedInHandler;
  }

  if (FAILED(hr) || !mContentDevice) {
    gfxCriticalNote << "Failed to create a D3D11 content device: " << hexa(hr);
    gfxWindowsPlatform::RecordContentDeviceFailure(TelemetryDeviceCode::Content);
    return FeatureStatus::Failed;
  }

  // InitializeD2D() will abort early if the compositor device did not support
  // texture sharing. If we're in the content process, we can't rely on the
  // parent device alone: some systems have dual GPUs that are capable of
  // binding the parent and child processes to different GPUs. As a safety net,
  // we re-check texture sharing against the newly created D3D11 content device.
  // If it fails, we won't use Direct2D.
  if (XRE_IsContentProcess()) {
    if (!D3D11Checks::DoesTextureSharingWork(mContentDevice)) {
      mContentDevice = nullptr;
      return FeatureStatus::Failed;
    }

    DebugOnly<bool> ok = ContentAdapterIsParentAdapter(mContentDevice);
    MOZ_ASSERT(ok);
  }

  mContentDevice->SetExceptionMode(0);

  RefPtr<ID3D10Multithread> multi;
  hr = mContentDevice->QueryInterface(__uuidof(ID3D10Multithread), getter_AddRefs(multi));
  if (SUCCEEDED(hr) && multi) {
    multi->SetMultithreadProtected(TRUE);
  }
  return FeatureStatus::Available;
}

bool
DeviceManagerD3D11::AttemptD3D11ImageBridgeDeviceCreationHelper(
  IDXGIAdapter1* aAdapter,
  HRESULT& aResOut)
{
  MOZ_SEH_TRY {
    aResOut =
      sD3D11CreateDeviceFn(GetDXGIAdapter(), D3D_DRIVER_TYPE_UNKNOWN, nullptr,
                           D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                           mFeatureLevels.Elements(), mFeatureLevels.Length(),
                           D3D11_SDK_VERSION, getter_AddRefs(mImageBridgeDevice), nullptr, nullptr);
  } MOZ_SEH_EXCEPT(EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
  return true;
}

FeatureStatus
DeviceManagerD3D11::AttemptD3D11ImageBridgeDeviceCreation()
{
  HRESULT hr;
  if (!AttemptD3D11ImageBridgeDeviceCreationHelper(GetDXGIAdapter(), hr)) {
    gfxCriticalNote << "Recovered from crash while creating a D3D11 image bridge device";
    gfxWindowsPlatform::RecordContentDeviceFailure(TelemetryDeviceCode::Image);
    return FeatureStatus::CrashedInHandler;
  }

  if (FAILED(hr) || !mImageBridgeDevice) {
    gfxCriticalNote << "Failed to create a content image bridge device: " << hexa(hr);
    gfxWindowsPlatform::RecordContentDeviceFailure(TelemetryDeviceCode::Image);
    return FeatureStatus::Failed;
  }

  mImageBridgeDevice->SetExceptionMode(0);
  if (!D3D11Checks::DoesAlphaTextureSharingWork(mImageBridgeDevice)) {
    mImageBridgeDevice = nullptr;
    return FeatureStatus::Failed;
  }

  if (XRE_IsContentProcess()) {
    ContentAdapterIsParentAdapter(mImageBridgeDevice);
  }
  return FeatureStatus::Available;
}

bool
DeviceManagerD3D11::CreateD3D11DecoderDeviceHelper(
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

RefPtr<ID3D11Device>
DeviceManagerD3D11::CreateDecoderDevice()
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
  if (FAILED(hr) || !device || !D3D11Checks::DoesDeviceWork()) {
    return nullptr;
  }

  RefPtr<ID3D10Multithread> multi;
  device->QueryInterface(__uuidof(ID3D10Multithread), getter_AddRefs(multi));

  multi->SetMultithreadProtected(TRUE);
  return device;
}

void
DeviceManagerD3D11::ResetDevices()
{
  MutexAutoLock lock(mDeviceLock);

  mCompositorDevice = nullptr;
  mContentDevice = nullptr;
  mImageBridgeDevice = nullptr;
  Factory::SetDirect3D11Device(nullptr);
}

bool
DeviceManagerD3D11::ContentAdapterIsParentAdapter(ID3D11Device* device)
{
  DXGI_ADAPTER_DESC desc;
  if (!D3D11Checks::GetDxgiDesc(device, &desc)) {
    gfxCriticalNote << "Could not query device DXGI adapter info";
    return false;
  }

  const DxgiAdapterDesc& parent =
    gfxPlatform::GetPlatform()->GetParentDevicePrefs().adapter();
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

static inline bool
DidDeviceReset(RefPtr<ID3D11Device> aDevice, DeviceResetReason* aOutReason)
{
  if (!aDevice) {
    return false;
  }
  HRESULT hr = aDevice->GetDeviceRemovedReason();
  if (hr == S_OK) {
    return false;
  }

  *aOutReason = HResultToResetReason(hr);
  return true;
}

bool
DeviceManagerD3D11::GetAnyDeviceRemovedReason(DeviceResetReason* aOutReason)
{
  // Note: this can be called off the main thread, so we need to use
  // our threadsafe getters.
  if (DidDeviceReset(GetCompositorDevice(), aOutReason) ||
      DidDeviceReset(GetImageBridgeDevice(), aOutReason) ||
      DidDeviceReset(GetContentDevice(), aOutReason))
  {
    return true;
  }
  return false;
}

void
DeviceManagerD3D11::DisableD3D11AfterCrash()
{
  gfxConfig::Disable(Feature::D3D11_COMPOSITING,
    FeatureStatus::CrashedInHandler,
    "Crashed while acquiring a Direct3D11 device",
    NS_LITERAL_CSTRING("FEATURE_FAILURE_D3D11_CRASH"));
  ResetDevices();
}

RefPtr<ID3D11Device>
DeviceManagerD3D11::GetCompositorDevice()
{
  MutexAutoLock lock(mDeviceLock);
  return mCompositorDevice;
}

RefPtr<ID3D11Device>
DeviceManagerD3D11::GetImageBridgeDevice()
{
  MutexAutoLock lock(mDeviceLock);
  return mImageBridgeDevice;
}

RefPtr<ID3D11Device>
DeviceManagerD3D11::GetContentDevice()
{
  MOZ_ASSERT(NS_IsMainThread());
  return mContentDevice;
}

RefPtr<ID3D11Device>
DeviceManagerD3D11::GetDeviceForCurrentThread()
{
  if (NS_IsMainThread()) {
    return GetContentDevice();
  }
  return GetCompositorDevice();
}

unsigned
DeviceManagerD3D11::GetD3D11Version() const
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!mCompositorDevice) {
    return 0;
  }
  return mCompositorDevice->GetFeatureLevel();
}

bool
DeviceManagerD3D11::TextureSharingWorks() const
{
  return mTextureSharingWorks;
}

bool
DeviceManagerD3D11::IsWARP() const
{
  return mIsWARP;
}

} // namespace gfx
} // namespace mozilla
