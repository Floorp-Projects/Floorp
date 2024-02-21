/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_gfx_thebes_DeviceManagerDx_h
#define mozilla_gfx_thebes_DeviceManagerDx_h

#include "gfxPlatform.h"
#include "gfxTelemetry.h"
#include "gfxTypes.h"
#include "mozilla/Maybe.h"
#include "mozilla/Mutex.h"
#include "mozilla/RefPtr.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/gfx/GraphicsMessages.h"
#include "nsTArray.h"
#include "nsWindowsHelpers.h"

#include <windows.h>
#include <objbase.h>

#include <d3d11.h>
#include <dxgi.h>
#include <dxgi1_6.h>

// This header is available in the June 2010 SDK and in the Win8 SDK
#include <d3dcommon.h>
// Win 8.0 SDK types we'll need when building using older sdks.
#if !defined(D3D_FEATURE_LEVEL_11_1)  // defined in the 8.0 SDK only
#  define D3D_FEATURE_LEVEL_11_1 static_cast<D3D_FEATURE_LEVEL>(0xb100)
#  define D3D_FL9_1_REQ_TEXTURE2D_U_OR_V_DIMENSION 2048
#  define D3D_FL9_3_REQ_TEXTURE2D_U_OR_V_DIMENSION 4096
#endif

struct ID3D11Device;
struct IDCompositionDevice2;
struct IDirectDraw7;

namespace mozilla {
class ScopedGfxFeatureReporter;
namespace layers {
class DeviceAttachmentsD3D11;
}  // namespace layers

namespace gfx {
class FeatureState;

class DeviceManagerDx final {
 public:
  static void Init();
  static void Shutdown();

  DeviceManagerDx();
  ~DeviceManagerDx();

  static DeviceManagerDx* Get() { return sInstance; }

  RefPtr<ID3D11Device> GetCompositorDevice();
  RefPtr<ID3D11Device> GetContentDevice();
  RefPtr<ID3D11Device> GetCanvasDevice();
  RefPtr<ID3D11Device> GetImageDevice();
  RefPtr<IDCompositionDevice2> GetDirectCompositionDevice();
  RefPtr<ID3D11Device> GetVRDevice();
  RefPtr<ID3D11Device> CreateDecoderDevice(bool aHardwareWebRender);
  RefPtr<ID3D11Device> CreateMediaEngineDevice();
  IDirectDraw7* GetDirectDraw();

  unsigned GetCompositorFeatureLevel() const;
  bool TextureSharingWorks();
  bool IsWARP();
  bool CanUseNV12();
  bool CanUseP010();
  bool CanUseP016();
  bool CanUseDComp();

  // Returns true if we can create a texture with
  // D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX and also
  // upload texture data during the CreateTexture2D
  // call. This crashes on some devices, so we might
  // need to avoid it.
  bool CanInitializeKeyedMutexTextures();

  // Enumerate and return all outputs on the current adapter.
  nsTArray<DXGI_OUTPUT_DESC1> EnumerateOutputs();

  // find the IDXGIOutput with a description.Monitor matching
  // 'monitor'; returns false if not found or some error occurred.
  bool GetOutputFromMonitor(HMONITOR monitor, RefPtr<IDXGIOutput>* aOutOutput);

  // Check if the current adapter supports hardware stretching
  void CheckHardwareStretchingSupport(HwStretchingSupport& aRv);

  bool CreateCompositorDevices();
  void CreateContentDevices();
  void CreateDirectCompositionDevice();
  bool CreateCanvasDevice();

  static HANDLE CreateDCompSurfaceHandle();

  void GetCompositorDevices(
      RefPtr<ID3D11Device>* aOutDevice,
      RefPtr<layers::DeviceAttachmentsD3D11>* aOutAttachments);

  void ImportDeviceInfo(const D3D11DeviceStatus& aDeviceStatus);

  // Returns whether the device info was exported.
  bool ExportDeviceInfo(D3D11DeviceStatus* aOut);

  void ResetDevices();
  void InitializeDirectDraw();

  // Reset and reacquire the devices if a reset has happened.
  // Returns whether a reset occurred not whether reacquiring
  // was successful.
  bool MaybeResetAndReacquireDevices();

  // Device reset helpers.
  bool HasDeviceReset(DeviceResetReason* aOutReason = nullptr);

  // Note: these set the cached device reset reason, which will be picked up
  // on the next frame.
  void ForceDeviceReset(ForcedDeviceResetReason aReason);

 private:
  void ResetDevicesLocked() MOZ_REQUIRES(mDeviceLock);

  // Device reset helpers.
  bool HasDeviceResetLocked(DeviceResetReason* aOutReason = nullptr)
      MOZ_REQUIRES(mDeviceLock);

  // Pre-load any compositor resources that are expensive, and are needed when
  // we attempt to create a compositor.
  static void PreloadAttachmentsOnCompositorThread();

  already_AddRefed<IDXGIAdapter1> GetDXGIAdapter();
  IDXGIAdapter1* GetDXGIAdapterLocked() MOZ_REQUIRES(mDeviceLock);

  void DisableD3D11AfterCrash();

  bool CreateCompositorDevicesLocked() MOZ_REQUIRES(mDeviceLock);
  void CreateContentDevicesLocked() MOZ_REQUIRES(mDeviceLock);
  void CreateDirectCompositionDeviceLocked() MOZ_REQUIRES(mDeviceLock);
  bool CreateCanvasDeviceLocked() MOZ_REQUIRES(mDeviceLock);

  void CreateCompositorDevice(mozilla::gfx::FeatureState& d3d11)
      MOZ_REQUIRES(mDeviceLock);
  bool CreateCompositorDeviceHelper(mozilla::gfx::FeatureState& aD3d11,
                                    IDXGIAdapter1* aAdapter,
                                    bool aAttemptVideoSupport,
                                    RefPtr<ID3D11Device>& aOutDevice)
      MOZ_REQUIRES(mDeviceLock);

  void CreateWARPCompositorDevice() MOZ_REQUIRES(mDeviceLock);
  bool CreateVRDevice() MOZ_REQUIRES(mDeviceLock);

  mozilla::gfx::FeatureStatus CreateContentDevice() MOZ_REQUIRES(mDeviceLock);

  bool CreateDevice(IDXGIAdapter* aAdapter, D3D_DRIVER_TYPE aDriverType,
                    UINT aFlags, HRESULT& aResOut,
                    RefPtr<ID3D11Device>& aOutDevice) MOZ_REQUIRES(mDeviceLock);

  bool ContentAdapterIsParentAdapter(ID3D11Device* device)
      MOZ_REQUIRES(mDeviceLock);

  bool LoadD3D11();
  bool LoadDcomp();
  void ReleaseD3D11() MOZ_REQUIRES(mDeviceLock);

  // Call GetDeviceRemovedReason on each device until one returns
  // a failure.
  bool GetAnyDeviceRemovedReason(DeviceResetReason* aOutReason)
      MOZ_REQUIRES(mDeviceLock);

 private:
  static StaticAutoPtr<DeviceManagerDx> sInstance;

  // This is assigned during device creation. Afterwards, it is released if
  // devices failed, and "forgotten" if devices succeeded (meaning, we leak
  // the ref and unassign the module).
  nsModuleHandle mD3D11Module;

  nsModuleHandle mDcompModule;

  mutable mozilla::Mutex mDeviceLock;
  nsTArray<D3D_FEATURE_LEVEL> mFeatureLevels MOZ_GUARDED_BY(mDeviceLock);
  RefPtr<IDXGIAdapter1> mAdapter MOZ_GUARDED_BY(mDeviceLock);
  RefPtr<ID3D11Device> mCompositorDevice MOZ_GUARDED_BY(mDeviceLock);
  RefPtr<ID3D11Device> mContentDevice MOZ_GUARDED_BY(mDeviceLock);
  RefPtr<ID3D11Device> mCanvasDevice MOZ_GUARDED_BY(mDeviceLock);
  RefPtr<ID3D11Device> mImageDevice MOZ_GUARDED_BY(mDeviceLock);
  RefPtr<ID3D11Device> mVRDevice MOZ_GUARDED_BY(mDeviceLock);
  RefPtr<ID3D11Device> mDecoderDevice MOZ_GUARDED_BY(mDeviceLock);
  RefPtr<IDCompositionDevice2> mDirectCompositionDevice
      MOZ_GUARDED_BY(mDeviceLock);
  RefPtr<layers::DeviceAttachmentsD3D11> mCompositorAttachments
      MOZ_GUARDED_BY(mDeviceLock);
  bool mCompositorDeviceSupportsVideo MOZ_GUARDED_BY(mDeviceLock);
  Maybe<D3D11DeviceStatus> mDeviceStatus MOZ_GUARDED_BY(mDeviceLock);
  Maybe<DeviceResetReason> mDeviceResetReason MOZ_GUARDED_BY(mDeviceLock);

  nsModuleHandle mDirectDrawDLL;
  RefPtr<IDirectDraw7> mDirectDraw;
};

}  // namespace gfx
}  // namespace mozilla

#endif  // mozilla_gfx_thebes_DeviceManagerDx_h
