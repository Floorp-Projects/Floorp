/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DeviceManagerDx.h"
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
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/layers/DeviceAttachmentsD3D11.h"
#include "nsIGfxInfo.h"
#include <d3d11.h>
#include <ddraw.h>

#ifdef MOZ_CRASHREPORTER
#include "nsExceptionHandler.h"
#include "nsPrintfCString.h"
#endif

namespace mozilla {
namespace gfx {

using namespace mozilla::widget;

StaticAutoPtr<DeviceManagerDx> DeviceManagerDx::sInstance;

// We don't have access to the D3D11CreateDevice type in gfxWindowsPlatform.h,
// since it doesn't include d3d11.h, so we use a static here. It should only
// be used within InitializeD3D11.
decltype(D3D11CreateDevice)* sD3D11CreateDeviceFn = nullptr;

// We don't have access to the DirectDrawCreateEx type in gfxWindowsPlatform.h,
// since it doesn't include ddraw.h, so we use a static here. It should only
// be used within InitializeDirectDrawConfig.
decltype(DirectDrawCreateEx)* sDirectDrawCreateExFn = nullptr;

/* static */ void
DeviceManagerDx::Init()
{
  sInstance = new DeviceManagerDx();
}

/* static */ void
DeviceManagerDx::Shutdown()
{
  sInstance = nullptr;
}

DeviceManagerDx::DeviceManagerDx()
 : mDeviceLock("gfxWindowsPlatform.mDeviceLock"),
   mCompositorDeviceSupportsVideo(false)
{
  // Set up the D3D11 feature levels we can ask for.
  if (IsWin8OrLater()) {
    mFeatureLevels.AppendElement(D3D_FEATURE_LEVEL_11_1);
  }
  mFeatureLevels.AppendElement(D3D_FEATURE_LEVEL_11_0);
  mFeatureLevels.AppendElement(D3D_FEATURE_LEVEL_10_1);
  mFeatureLevels.AppendElement(D3D_FEATURE_LEVEL_10_0);
}

bool
DeviceManagerDx::LoadD3D11()
{
  FeatureState& d3d11 = gfxConfig::GetFeature(Feature::D3D11_COMPOSITING);
  MOZ_ASSERT(d3d11.IsEnabled());

  if (sD3D11CreateDeviceFn) {
    return true;
  }

  nsModuleHandle module(LoadLibrarySystem32(L"d3d11.dll"));
  if (!module) {
    d3d11.SetFailed(FeatureStatus::Unavailable, "Direct3D11 not available on this computer",
                    NS_LITERAL_CSTRING("FEATURE_FAILURE_D3D11_LIB"));
    return false;
  }

  sD3D11CreateDeviceFn =
    (decltype(D3D11CreateDevice)*)GetProcAddress(module, "D3D11CreateDevice");
  if (!sD3D11CreateDeviceFn) {
    // We should just be on Windows Vista or XP in this case.
    d3d11.SetFailed(FeatureStatus::Unavailable, "Direct3D11 not available on this computer",
                    NS_LITERAL_CSTRING("FEATURE_FAILURE_D3D11_FUNCPTR"));
    return false;
  }

  mD3D11Module.steal(module);
  return true;
}

void
DeviceManagerDx::ReleaseD3D11()
{
  MOZ_ASSERT(!mCompositorDevice);
  MOZ_ASSERT(!mContentDevice);
  MOZ_ASSERT(!mDecoderDevice);

  mD3D11Module.reset();
  sD3D11CreateDeviceFn = nullptr;
}

static inline bool
ProcessOwnsCompositor()
{
  return XRE_GetProcessType() == GeckoProcessType_GPU ||
         (XRE_IsParentProcess() && !gfxConfig::IsEnabled(Feature::GPU_PROCESS));
}

bool
DeviceManagerDx::CreateCompositorDevices()
{
  MOZ_ASSERT(ProcessOwnsCompositor());

  FeatureState& d3d11 = gfxConfig::GetFeature(Feature::D3D11_COMPOSITING);
  MOZ_ASSERT(d3d11.IsEnabled());

  if (!LoadD3D11()) {
    return false;
  }

  CreateCompositorDevice(d3d11);

  if (!d3d11.IsEnabled()) {
    MOZ_ASSERT(!mCompositorDevice);
    ReleaseD3D11();
    return false;
  }

  // We leak these everywhere and we need them our entire runtime anyway, let's
  // leak it here as well. We keep the pointer to sD3D11CreateDeviceFn around
  // as well for D2D1 and device resets.
  mD3D11Module.disown();

  MOZ_ASSERT(mCompositorDevice);
  if (!d3d11.IsEnabled()) {
    return false;
  }

  PreloadAttachmentsOnCompositorThread();
  return true;
}

void
DeviceManagerDx::ImportDeviceInfo(const D3D11DeviceStatus& aDeviceStatus)
{
  MOZ_ASSERT(!ProcessOwnsCompositor());

  mDeviceStatus = Some(aDeviceStatus);
}

void
DeviceManagerDx::ExportDeviceInfo(D3D11DeviceStatus* aOut)
{
  // Even though the parent process might not own the compositor, we still
  // populate DeviceManagerDx with device statistics (for simplicity).
  // That means it still gets queried for compositor information.
  MOZ_ASSERT(XRE_IsParentProcess() || XRE_GetProcessType() == GeckoProcessType_GPU);

  if (mDeviceStatus) {
    *aOut = mDeviceStatus.value();
  }
}

void
DeviceManagerDx::CreateContentDevices()
{
  MOZ_ASSERT(gfxConfig::IsEnabled(Feature::D3D11_COMPOSITING));

  if (!LoadD3D11()) {
    return;
  }

  // We should have been assigned a DeviceStatus from the parent process,
  // GPU process, or the same process if using in-process compositing.
  MOZ_ASSERT(mDeviceStatus);

  if (CreateContentDevice() == FeatureStatus::CrashedInHandler) {
    DisableD3D11AfterCrash();
  }
}

IDXGIAdapter1*
DeviceManagerDx::GetDXGIAdapter()
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

  if (!mDeviceStatus) {
    // If we haven't created a device yet, and have no existing device status,
    // then this must be the compositor device. Pick the first adapter we can.
    if (FAILED(factory1->EnumAdapters1(0, getter_AddRefs(mAdapter)))) {
      return nullptr;
    }
  } else {
    // In the UI and GPU process, we clear mDeviceStatus on device reset, so we
    // should never reach here. Furthermore, the UI process does not create
    // devices when using a GPU process.
    //
    // So, this should only ever get called on the content process.
    MOZ_ASSERT(XRE_IsContentProcess());

    // In the child process, we search for the adapter that matches the parent
    // process. The first adapter can be mismatched on dual-GPU systems.
    for (UINT index = 0; ; index++) {
      RefPtr<IDXGIAdapter1> adapter;
      if (FAILED(factory1->EnumAdapters1(index, getter_AddRefs(adapter)))) {
        break;
      }

      const DxgiAdapterDesc& preferred = mDeviceStatus->adapter();

      DXGI_ADAPTER_DESC desc;
      if (SUCCEEDED(adapter->GetDesc(&desc)) &&
          desc.AdapterLuid.HighPart == preferred.AdapterLuid.HighPart &&
          desc.AdapterLuid.LowPart == preferred.AdapterLuid.LowPart &&
          desc.VendorId == preferred.VendorId &&
          desc.DeviceId == preferred.DeviceId)
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
DeviceManagerDx::CreateCompositorDeviceHelper(
  FeatureState& aD3d11, IDXGIAdapter1* aAdapter, bool aAttemptVideoSupport, RefPtr<ID3D11Device>& aOutDevice)
{
  // Check if a failure was injected for testing.
  if (gfxPrefs::DeviceFailForTesting()) {
    aD3d11.SetFailed(FeatureStatus::Failed, "Direct3D11 device failure simulated by preference",
                     NS_LITERAL_CSTRING("FEATURE_FAILURE_D3D11_SIM"));
    return false;
  }

  // Use D3D11_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS
  // to prevent bug 1092260. IE 11 also uses this flag.
  UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS;
  if (aAttemptVideoSupport) {
    flags |= D3D11_CREATE_DEVICE_VIDEO_SUPPORT;
  }

  HRESULT hr;
  RefPtr<ID3D11Device> device;
  if (!CreateDevice(aAdapter, D3D_DRIVER_TYPE_UNKNOWN, flags, hr, device)) {
    if (!aAttemptVideoSupport) {
      gfxCriticalError() << "Crash during D3D11 device creation";
      aD3d11.SetFailed(FeatureStatus::CrashedInHandler, "Crashed trying to acquire a D3D11 device",
                       NS_LITERAL_CSTRING("FEATURE_FAILURE_D3D11_DEVICE1"));
    }
    return false;
  }

  if (FAILED(hr) || !device) {
    if (!aAttemptVideoSupport) {
      aD3d11.SetFailed(FeatureStatus::Failed, "Failed to acquire a D3D11 device",
                       NS_LITERAL_CSTRING("FEATURE_FAILURE_D3D11_DEVICE2"));
    }
    return false;
  }
  if (!D3D11Checks::DoesDeviceWork()) {
    if (!aAttemptVideoSupport) {
      aD3d11.SetFailed(FeatureStatus::Broken, "Direct3D11 device was determined to be broken",
                       NS_LITERAL_CSTRING("FEATURE_FAILURE_D3D11_BROKEN"));
    }
    return false;
  }

  aOutDevice = device;
  return true;
}

void
DeviceManagerDx::CreateCompositorDevice(FeatureState& d3d11)
{
  if (gfxPrefs::LayersD3D11ForceWARP()) {
    CreateWARPCompositorDevice();
    return;
  }

  RefPtr<IDXGIAdapter1> adapter = GetDXGIAdapter();
  if (!adapter) {
    d3d11.SetFailed(FeatureStatus::Unavailable, "Failed to acquire a DXGI adapter",
                    NS_LITERAL_CSTRING("FEATURE_FAILURE_D3D11_DXGI"));
    return;
  }

  if (XRE_IsGPUProcess() && !D3D11Checks::DoesRemotePresentWork(adapter)) {
    d3d11.SetFailed(
      FeatureStatus::Unavailable,
      "DXGI does not support out-of-process presentation",
      NS_LITERAL_CSTRING("FEATURE_FAILURE_D3D11_REMOTE_PRESENT"));
    return;
  }

  RefPtr<ID3D11Device> device;
  if (!CreateCompositorDeviceHelper(d3d11, adapter, true, device)) {
    // Try again without video support and record that it failed.
    mCompositorDeviceSupportsVideo = false;
    if (!CreateCompositorDeviceHelper(d3d11, adapter, false, device)) {
      return;
    }
  } else {
    mCompositorDeviceSupportsVideo = true;
  }

  // Only test this when not using WARP since it can fail and cause
  // GetDeviceRemovedReason to return weird values.
  bool textureSharingWorks = D3D11Checks::DoesTextureSharingWork(device);

  DXGI_ADAPTER_DESC desc;
  PodZero(&desc);
  adapter->GetDesc(&desc);

  if (!textureSharingWorks) {
    gfxConfig::SetFailed(Feature::D3D11_HW_ANGLE,
                         FeatureStatus::Broken,
                         "Texture sharing doesn't work");
  }
  if (D3D11Checks::DoesRenderTargetViewNeedRecreating(device)) {
    gfxConfig::SetFailed(Feature::D3D11_HW_ANGLE,
                         FeatureStatus::Broken,
                         "RenderTargetViews need recreating");
  }
  if (XRE_IsParentProcess()) {
    // It seems like this may only happen when we're using the NVIDIA gpu
    D3D11Checks::WarnOnAdapterMismatch(device);
  }

  int featureLevel = device->GetFeatureLevel();
  {
    MutexAutoLock lock(mDeviceLock);
    mCompositorDevice = device;
    mDeviceStatus = Some(D3D11DeviceStatus(
      false,
      textureSharingWorks,
      featureLevel,
      DxgiAdapterDesc::From(desc)));
  }
  mCompositorDevice->SetExceptionMode(0);
}

//#define BREAK_ON_D3D_ERROR

bool
DeviceManagerDx::CreateDevice(IDXGIAdapter* aAdapter,
                                 D3D_DRIVER_TYPE aDriverType,
                                 UINT aFlags,
                                 HRESULT& aResOut,
                                 RefPtr<ID3D11Device>& aOutDevice)
{
#ifdef BREAK_ON_D3D_ERROR
  aFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

  MOZ_SEH_TRY {
    aResOut = sD3D11CreateDeviceFn(
      aAdapter, aDriverType, nullptr,
      aFlags,
      mFeatureLevels.Elements(), mFeatureLevels.Length(),
      D3D11_SDK_VERSION, getter_AddRefs(aOutDevice), nullptr, nullptr);
  } MOZ_SEH_EXCEPT (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }

#ifdef BREAK_ON_D3D_ERROR
  do {
    if (!aOutDevice)
      break;

    RefPtr<ID3D11Debug> debug;
    if(!SUCCEEDED( aOutDevice->QueryInterface(__uuidof(ID3D11Debug), getter_AddRefs(debug)) ))
      break;

    RefPtr<ID3D11InfoQueue> infoQueue;
    if(!SUCCEEDED( debug->QueryInterface(__uuidof(ID3D11InfoQueue), getter_AddRefs(infoQueue)) ))
      break;

    infoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, true);
    infoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, true);
    infoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_WARNING, true);
  } while (false);
#endif

  return true;
}

void
DeviceManagerDx::CreateWARPCompositorDevice()
{
  ScopedGfxFeatureReporter reporterWARP("D3D11-WARP", gfxPrefs::LayersD3D11ForceWARP());
  FeatureState& d3d11 = gfxConfig::GetFeature(Feature::D3D11_COMPOSITING);

  HRESULT hr;
  RefPtr<ID3D11Device> device;

  // Use D3D11_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS
  // to prevent bug 1092260. IE 11 also uses this flag.
  UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
  if (!CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, flags, hr, device)) {
    gfxCriticalError() << "Exception occurred initializing WARP D3D11 device!";
    d3d11.SetFailed(FeatureStatus::CrashedInHandler, "Crashed creating a D3D11 WARP device",
                     NS_LITERAL_CSTRING("FEATURE_FAILURE_D3D11_WARP_DEVICE"));
  }

  if (FAILED(hr) || !device) {
    // This should always succeed... in theory.
    gfxCriticalError() << "Failed to initialize WARP D3D11 device! " << hexa(hr);
    d3d11.SetFailed(FeatureStatus::Failed, "Failed to create a D3D11 WARP device",
                    NS_LITERAL_CSTRING("FEATURE_FAILURE_D3D11_WARP_DEVICE2"));
    return;
  }

  // Only test for texture sharing on Windows 8 since it puts the device into
  // an unusable state if used on Windows 7
  bool textureSharingWorks = false;
  if (IsWin8OrLater()) {
    textureSharingWorks = D3D11Checks::DoesTextureSharingWork(device);
  }

  DxgiAdapterDesc nullAdapter;
  PodZero(&nullAdapter);

  int featureLevel = device->GetFeatureLevel();
  {
    MutexAutoLock lock(mDeviceLock);
    mCompositorDevice = device;
    mDeviceStatus = Some(D3D11DeviceStatus(
      true,
      textureSharingWorks,
      featureLevel,
      nullAdapter));
  }
  mCompositorDevice->SetExceptionMode(0);

  reporterWARP.SetSuccessful();
}

FeatureStatus
DeviceManagerDx::CreateContentDevice()
{
  RefPtr<IDXGIAdapter1> adapter;
  if (!mDeviceStatus->isWARP()) {
    adapter = GetDXGIAdapter();
    if (!adapter) {
      gfxCriticalNote << "Could not get a DXGI adapter";
      return FeatureStatus::Unavailable;
    }
  }

  HRESULT hr;
  RefPtr<ID3D11Device> device;

  UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
  D3D_DRIVER_TYPE type = mDeviceStatus->isWARP()
                         ? D3D_DRIVER_TYPE_WARP
                         : D3D_DRIVER_TYPE_UNKNOWN;
  if (!CreateDevice(adapter, type, flags, hr, device)) {
    gfxCriticalNote << "Recovered from crash while creating a D3D11 content device";
    gfxWindowsPlatform::RecordContentDeviceFailure(TelemetryDeviceCode::Content);
    return FeatureStatus::CrashedInHandler;
  }

  if (FAILED(hr) || !device) {
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
    if (!D3D11Checks::DoesTextureSharingWork(device)) {
      return FeatureStatus::Failed;
    }

    DebugOnly<bool> ok = ContentAdapterIsParentAdapter(device);
    MOZ_ASSERT(ok);
  }

  {
    MutexAutoLock lock(mDeviceLock);
    mContentDevice = device;
  }
  mContentDevice->SetExceptionMode(0);

  RefPtr<ID3D10Multithread> multi;
  hr = mContentDevice->QueryInterface(__uuidof(ID3D10Multithread), getter_AddRefs(multi));
  if (SUCCEEDED(hr) && multi) {
    multi->SetMultithreadProtected(TRUE);
  }
  return FeatureStatus::Available;
}

RefPtr<ID3D11Device>
DeviceManagerDx::CreateDecoderDevice()
{
  bool isAMD = false;
  {
    MutexAutoLock lock(mDeviceLock);
    if (!mDeviceStatus) {
      return nullptr;
    }
    isAMD = mDeviceStatus->adapter().VendorId == 0x1002;
  }

  bool reuseDevice = false;
  if (gfxPrefs::Direct3D11ReuseDecoderDevice() < 0) {
    // Use the default logic, which is to allow reuse of devices on AMD, but create
    // separate devices everywhere else.
    if (isAMD) {
      reuseDevice = true;
    }
  } else if (gfxPrefs::Direct3D11ReuseDecoderDevice() > 0) {
    reuseDevice = true;
  }

  if (reuseDevice) {
    if (mCompositorDevice && mCompositorDeviceSupportsVideo && !mDecoderDevice) {
      mDecoderDevice = mCompositorDevice;

      RefPtr<ID3D10Multithread> multi;
      mDecoderDevice->QueryInterface(__uuidof(ID3D10Multithread), getter_AddRefs(multi));
      if (multi) {
        multi->SetMultithreadProtected(TRUE);
      }
    }

    if (mDecoderDevice) {
      RefPtr<ID3D11Device> dev = mDecoderDevice;
      return dev.forget();
    }
  }

   if (!sD3D11CreateDeviceFn) {
    // We should just be on Windows Vista or XP in this case.
    return nullptr;
  }

  RefPtr<IDXGIAdapter1> adapter = GetDXGIAdapter();
  if (!adapter) {
    return nullptr;
  }

  HRESULT hr;
  RefPtr<ID3D11Device> device;

  UINT flags = D3D11_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS |
               D3D11_CREATE_DEVICE_VIDEO_SUPPORT;
  if (!CreateDevice(adapter, D3D_DRIVER_TYPE_UNKNOWN, flags, hr, device)) {
    return nullptr;
  }
  if (FAILED(hr) || !device || !D3D11Checks::DoesDeviceWork()) {
    return nullptr;
  }

  RefPtr<ID3D10Multithread> multi;
  device->QueryInterface(__uuidof(ID3D10Multithread), getter_AddRefs(multi));
  if (multi) {
    multi->SetMultithreadProtected(TRUE);
  }
  if (reuseDevice) {
    mDecoderDevice = device;
  }
  return device;
}

void
DeviceManagerDx::ResetDevices()
{
  MutexAutoLock lock(mDeviceLock);

  mAdapter = nullptr;
  mCompositorAttachments = nullptr;
  mCompositorDevice = nullptr;
  mContentDevice = nullptr;
  mDeviceStatus = Nothing();
  mDeviceResetReason = Nothing();
  Factory::SetDirect3D11Device(nullptr);
}

bool
DeviceManagerDx::MaybeResetAndReacquireDevices()
{
  DeviceResetReason resetReason;
  if (!HasDeviceReset(&resetReason)) {
    return false;
  }

  if (resetReason != DeviceResetReason::FORCED_RESET) {
    Telemetry::Accumulate(Telemetry::DEVICE_RESET_REASON, uint32_t(resetReason));
  }

#ifdef MOZ_CRASHREPORTER
  nsPrintfCString reasonString("%d", int(resetReason));
  CrashReporter::AnnotateCrashReport(
    NS_LITERAL_CSTRING("DeviceResetReason"),
    reasonString);
#endif

  bool createCompositorDevice = !!mCompositorDevice;
  bool createContentDevice = !!mContentDevice;

  ResetDevices();

  if (createCompositorDevice && !CreateCompositorDevices()) {
    // Just stop, don't try anything more
    return true;
  }
  if (createContentDevice) {
    CreateContentDevices();
  }

  return true;
}

bool
DeviceManagerDx::ContentAdapterIsParentAdapter(ID3D11Device* device)
{
  DXGI_ADAPTER_DESC desc;
  if (!D3D11Checks::GetDxgiDesc(device, &desc)) {
    gfxCriticalNote << "Could not query device DXGI adapter info";
    return false;
  }

  const DxgiAdapterDesc& preferred = mDeviceStatus->adapter();

  if (desc.VendorId != preferred.VendorId ||
      desc.DeviceId != preferred.DeviceId ||
      desc.SubSysId != preferred.SubSysId ||
      desc.AdapterLuid.HighPart != preferred.AdapterLuid.HighPart ||
      desc.AdapterLuid.LowPart != preferred.AdapterLuid.LowPart)
  {
    gfxCriticalNote <<
      "VendorIDMismatch P " <<
      hexa(preferred.VendorId) << " " <<
      hexa(desc.VendorId);
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

bool
DeviceManagerDx::HasDeviceReset(DeviceResetReason* aOutReason)
{
  MutexAutoLock lock(mDeviceLock);

  if (mDeviceResetReason) {
    if (aOutReason) {
      *aOutReason = mDeviceResetReason.value();
    }
    return true;
  }

  DeviceResetReason reason;
  if (GetAnyDeviceRemovedReason(&reason)) {
    mDeviceResetReason = Some(reason);
    if (aOutReason) {
      *aOutReason = reason;
    }
    return true;
  }

  return false;
}

static inline bool
DidDeviceReset(const RefPtr<ID3D11Device>& aDevice, DeviceResetReason* aOutReason)
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
DeviceManagerDx::GetAnyDeviceRemovedReason(DeviceResetReason* aOutReason)
{
  // Caller must own the lock, since we access devices directly, and can be
  // called from any thread.
  mDeviceLock.AssertCurrentThreadOwns();

  if (DidDeviceReset(mCompositorDevice, aOutReason) ||
      DidDeviceReset(mContentDevice, aOutReason))
  {
    return true;
  }

  if (XRE_IsParentProcess() &&
      NS_IsMainThread() &&
      gfxPrefs::DeviceResetForTesting())
  {
    gfxPrefs::SetDeviceResetForTesting(0);
    *aOutReason = DeviceResetReason::FORCED_RESET;
    return true;
  }

  return false;
}

void
DeviceManagerDx::ForceDeviceReset(ForcedDeviceResetReason aReason)
{
  Telemetry::Accumulate(Telemetry::FORCED_DEVICE_RESET_REASON, uint32_t(aReason));
  {
    MutexAutoLock lock(mDeviceLock);
    mDeviceResetReason = Some(DeviceResetReason::FORCED_RESET);
  }
}

void
DeviceManagerDx::NotifyD3D9DeviceReset()
{
  MutexAutoLock lock(mDeviceLock);
  mDeviceResetReason = Some(DeviceResetReason::D3D9_RESET);
}

void
DeviceManagerDx::DisableD3D11AfterCrash()
{
  gfxConfig::Disable(Feature::D3D11_COMPOSITING,
    FeatureStatus::CrashedInHandler,
    "Crashed while acquiring a Direct3D11 device",
    NS_LITERAL_CSTRING("FEATURE_FAILURE_D3D11_CRASH"));
  ResetDevices();
}

RefPtr<ID3D11Device>
DeviceManagerDx::GetCompositorDevice()
{
  MutexAutoLock lock(mDeviceLock);
  return mCompositorDevice;
}

RefPtr<ID3D11Device>
DeviceManagerDx::GetContentDevice()
{
  MutexAutoLock lock(mDeviceLock);
  return mContentDevice;
}

unsigned
DeviceManagerDx::GetCompositorFeatureLevel() const
{
  if (!mDeviceStatus) {
    return 0;
  }
  return mDeviceStatus->featureLevel();
}

bool
DeviceManagerDx::TextureSharingWorks()
{
  MutexAutoLock lock(mDeviceLock);
  if (!mDeviceStatus) {
    return false;
  }
  return mDeviceStatus->textureSharingWorks();
}

bool
DeviceManagerDx::CanInitializeKeyedMutexTextures()
{
  MutexAutoLock lock(mDeviceLock);
  if (!mDeviceStatus) {
    return false;
  }
  // Disable this on all Intel devices because of crashes.
  // See bug 1292923.
  return (mDeviceStatus->adapter().VendorId != 0x8086 || gfxPrefs::Direct3D11AllowIntelMutex());
}

bool
DeviceManagerDx::HasCrashyInitData()
{
  MutexAutoLock lock(mDeviceLock);
  if (!mDeviceStatus) {
    return false;
  }

  return (mDeviceStatus->adapter().VendorId == 0x8086 && !IsWin10OrLater());
}

bool
DeviceManagerDx::CheckRemotePresentSupport()
{
  MOZ_ASSERT(XRE_IsParentProcess());

  RefPtr<IDXGIAdapter1> adapter = GetDXGIAdapter();
  if (!adapter) {
    return false;
  }
  if (!D3D11Checks::DoesRemotePresentWork(adapter)) {
    return false;
  }
  return true;
}

bool
DeviceManagerDx::IsWARP()
{
  MutexAutoLock lock(mDeviceLock);
  if (!mDeviceStatus) {
    return false;
  }
  return mDeviceStatus->isWARP();
}

void
DeviceManagerDx::InitializeDirectDraw()
{
  MOZ_ASSERT(layers::CompositorThreadHolder::IsInCompositorThread());

  if (mDirectDraw) {
    // Already initialized.
    return;
  }

  FeatureState& ddraw = gfxConfig::GetFeature(Feature::DIRECT_DRAW);
  if (!ddraw.IsEnabled()) {
    return;
  }

  // Check if DirectDraw is available on this system.
  mDirectDrawDLL.own(LoadLibrarySystem32(L"ddraw.dll"));
  if (!mDirectDrawDLL) {
    ddraw.SetFailed(FeatureStatus::Unavailable, "DirectDraw not available on this computer",
                    NS_LITERAL_CSTRING("FEATURE_FAILURE_DDRAW_LIB"));
    return;
  }

  sDirectDrawCreateExFn =
    (decltype(DirectDrawCreateEx)*)GetProcAddress(mDirectDrawDLL, "DirectDrawCreateEx");
  if (!sDirectDrawCreateExFn) {
    ddraw.SetFailed(FeatureStatus::Unavailable, "DirectDraw not available on this computer",
                    NS_LITERAL_CSTRING("FEATURE_FAILURE_DDRAW_LIB"));
    return;
  }

  HRESULT hr;
  MOZ_SEH_TRY {
    hr = sDirectDrawCreateExFn(nullptr, getter_AddRefs(mDirectDraw), IID_IDirectDraw7, nullptr);
  } MOZ_SEH_EXCEPT(EXCEPTION_EXECUTE_HANDLER) {
    ddraw.SetFailed(FeatureStatus::Failed, "Failed to create DirectDraw",
                    NS_LITERAL_CSTRING("FEATURE_FAILURE_DDRAW_LIB"));
    gfxCriticalNote << "DoesCreatingDirectDrawFailed";
    return;
  }
  if (FAILED(hr)) {
    ddraw.SetFailed(FeatureStatus::Failed, "Failed to create DirectDraw",
                    NS_LITERAL_CSTRING("FEATURE_FAILURE_DDRAW_LIB"));
    gfxCriticalNote << "DoesCreatingDirectDrawFailed " << hexa(hr);
    return;
  }
}

IDirectDraw7*
DeviceManagerDx::GetDirectDraw()
{
  return mDirectDraw;
}

void
DeviceManagerDx::GetCompositorDevices(RefPtr<ID3D11Device>* aOutDevice,
                                      RefPtr<layers::DeviceAttachmentsD3D11>* aOutAttachments)
{
  MOZ_ASSERT(layers::CompositorThreadHolder::IsInCompositorThread());

  RefPtr<ID3D11Device> device;
  {
    MutexAutoLock lock(mDeviceLock);
    if (!mCompositorDevice) {
      return;
    }
    if (mCompositorAttachments) {
      *aOutDevice = mCompositorDevice;
      *aOutAttachments = mCompositorAttachments;
      return;
    }

    // Otherwise, we'll try to create attachments outside the lock.
    device = mCompositorDevice;
  }

  // We save the attachments object even if it fails to initialize, so the
  // compositor can grab the failure ID.
  RefPtr<layers::DeviceAttachmentsD3D11> attachments =
    layers::DeviceAttachmentsD3D11::Create(device);
  {
    MutexAutoLock lock(mDeviceLock);
    if (device != mCompositorDevice) {
      return;
    }
    mCompositorAttachments = attachments;
  }

  *aOutDevice = device;
  *aOutAttachments = attachments;
}

/* static */ void
DeviceManagerDx::PreloadAttachmentsOnCompositorThread()
{
  MessageLoop* loop = layers::CompositorThreadHolder::Loop();
  if (!loop) {
    return;
  }

  RefPtr<Runnable> task = NS_NewRunnableFunction([]() -> void {
    if (DeviceManagerDx* dm = DeviceManagerDx::Get()) {
      RefPtr<ID3D11Device> device;
      RefPtr<layers::DeviceAttachmentsD3D11> attachments;
      dm->GetCompositorDevices(&device, &attachments);
    }
  });
  loop->PostTask(task.forget());
}

} // namespace gfx
} // namespace mozilla
