/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_gfx_thebes_DeviceManagerDx_h
#define mozilla_gfx_thebes_DeviceManagerDx_h

#include "gfxPlatform.h"
#include "gfxTelemetry.h"
#include "mozilla/Maybe.h"
#include "mozilla/Mutex.h"
#include "mozilla/RefPtr.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/gfx/GraphicsMessages.h"
#include "nsTArray.h"
#include "nsWindowsHelpers.h"

#include <windows.h>
#include <objbase.h>

#include <dxgi.h>

// This header is available in the June 2010 SDK and in the Win8 SDK
#include <d3dcommon.h>
// Win 8.0 SDK types we'll need when building using older sdks.
#if !defined(D3D_FEATURE_LEVEL_11_1) // defined in the 8.0 SDK only
#define D3D_FEATURE_LEVEL_11_1 static_cast<D3D_FEATURE_LEVEL>(0xb100)
#define D3D_FL9_1_REQ_TEXTURE2D_U_OR_V_DIMENSION 2048
#define D3D_FL9_3_REQ_TEXTURE2D_U_OR_V_DIMENSION 4096
#endif

struct ID3D11Device;
struct IDirectDraw7;

namespace mozilla {
class ScopedGfxFeatureReporter;
namespace layers {
class DeviceAttachmentsD3D11;
} // namespace layers

namespace gfx {
class FeatureState;

class DeviceManagerDx final
{
public:
  static void Init();
  static void Shutdown();

  DeviceManagerDx();

  static DeviceManagerDx* Get() {
    return sInstance;
  }

  RefPtr<ID3D11Device> GetCompositorDevice();
  RefPtr<ID3D11Device> GetContentDevice();
  RefPtr<ID3D11Device> CreateDecoderDevice();
  IDirectDraw7* GetDirectDraw();

  unsigned GetCompositorFeatureLevel() const;
  bool TextureSharingWorks();
  bool IsWARP();

  // Returns true if we can create a texture with
  // D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX and also
  // upload texture data during the CreateTexture2D
  // call. This crashes on some devices, so we might
  // need to avoid it.
  bool CanInitializeKeyedMutexTextures();

  // Intel devices on older windows versions seem to occasionally have
  // stability issues when supplying InitData to CreateTexture2D.
  bool HasCrashyInitData();

  bool CreateCompositorDevices();
  void CreateContentDevices();

  void GetCompositorDevices(RefPtr<ID3D11Device>* aOutDevice,
                            RefPtr<layers::DeviceAttachmentsD3D11>* aOutAttachments);

  void ImportDeviceInfo(const D3D11DeviceStatus& aDeviceStatus);
  void ExportDeviceInfo(D3D11DeviceStatus* aOut);

  void ResetDevices();
  void InitializeDirectDraw();

  // Reset and reacquire the devices if a reset has happened.
  // Returns whether a reset occurred not whether reacquiring
  // was successful.
  bool MaybeResetAndReacquireDevices();

  // Test whether we can acquire a DXGI 1.2-compatible adapter. This should
  // only be called on startup before devices are initialized.
  bool CheckRemotePresentSupport();

  // Device reset helpers.
  bool HasDeviceReset(DeviceResetReason* aOutReason = nullptr);

  // Note: these set the cached device reset reason, which will be picked up
  // on the next frame.
  void ForceDeviceReset(ForcedDeviceResetReason aReason);
  void NotifyD3D9DeviceReset();

  // Pre-load any compositor resources that are expensive, and are needed when we
  // attempt to create a compositor.
  static void PreloadAttachmentsOnCompositorThread();

private:
  IDXGIAdapter1 *GetDXGIAdapter();

  void DisableD3D11AfterCrash();

  void CreateCompositorDevice(mozilla::gfx::FeatureState& d3d11);
  bool CreateCompositorDeviceHelper(
      mozilla::gfx::FeatureState& aD3d11,
      IDXGIAdapter1* aAdapter,
      bool aAttemptVideoSupport,
      RefPtr<ID3D11Device>& aOutDevice);

  void CreateWARPCompositorDevice();

  mozilla::gfx::FeatureStatus CreateContentDevice();

  bool CreateDevice(IDXGIAdapter* aAdapter,
                    D3D_DRIVER_TYPE aDriverType,
                    UINT aFlags,
                    HRESULT& aResOut,
                    RefPtr<ID3D11Device>& aOutDevice);

  bool ContentAdapterIsParentAdapter(ID3D11Device* device);

  bool LoadD3D11();
  void ReleaseD3D11();

  // Call GetDeviceRemovedReason on each device until one returns
  // a failure.
  bool GetAnyDeviceRemovedReason(DeviceResetReason* aOutReason);

private:
  static StaticAutoPtr<DeviceManagerDx> sInstance;

  // This is assigned during device creation. Afterwards, it is released if
  // devices failed, and "forgotten" if devices succeeded (meaning, we leak
  // the ref and unassign the module).
  nsModuleHandle mD3D11Module;

  mozilla::Mutex mDeviceLock;
  nsTArray<D3D_FEATURE_LEVEL> mFeatureLevels;
  RefPtr<IDXGIAdapter1> mAdapter;
  RefPtr<ID3D11Device> mCompositorDevice;
  RefPtr<ID3D11Device> mContentDevice;
  RefPtr<ID3D11Device> mDecoderDevice;
  RefPtr<layers::DeviceAttachmentsD3D11> mCompositorAttachments;
  bool mCompositorDeviceSupportsVideo;

  Maybe<D3D11DeviceStatus> mDeviceStatus;

  nsModuleHandle mDirectDrawDLL;
  RefPtr<IDirectDraw7> mDirectDraw;

  Maybe<DeviceResetReason> mDeviceResetReason;
};

} // namespace gfx
} // namespace mozilla

#endif // mozilla_gfx_thebes_DeviceManagerDx_h
