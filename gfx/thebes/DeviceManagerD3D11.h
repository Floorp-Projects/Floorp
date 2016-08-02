/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_gfx_thebes_DeviceManagerD3D11_h
#define mozilla_gfx_thebes_DeviceManagerD3D11_h

#include "gfxPlatform.h"
#include "gfxTelemetry.h"
#include "mozilla/Mutex.h"
#include "mozilla/RefPtr.h"
#include "mozilla/StaticPtr.h"
#include "nsTArray.h"

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

namespace mozilla {
class ScopedGfxFeatureReporter;

namespace gfx {
class FeatureState;

class DeviceManagerD3D11 final
{
public:
  static void Init();
  static void Shutdown();

  DeviceManagerD3D11();

  static DeviceManagerD3D11* Get() {
    return sInstance;
  }

  RefPtr<ID3D11Device> GetCompositorDevice();
  RefPtr<ID3D11Device> GetImageBridgeDevice();
  RefPtr<ID3D11Device> GetContentDevice();
  RefPtr<ID3D11Device> GetDeviceForCurrentThread();
  RefPtr<ID3D11Device> CreateDecoderDevice();

  unsigned GetD3D11Version() const;
  bool TextureSharingWorks() const;
  bool IsWARP() const;

  void CreateDevices();
  void ResetDevices();

  // Call GetDeviceRemovedReason on each device until one returns
  // a failure.
  bool GetAnyDeviceRemovedReason(DeviceResetReason* aOutReason);

private:
  IDXGIAdapter1 *GetDXGIAdapter();

  bool CanUseD3D11ImageBridge();

  void DisableD3D11AfterCrash();

  void AttemptD3D11DeviceCreation(mozilla::gfx::FeatureState& d3d11);
  bool AttemptD3D11DeviceCreationHelper(
      IDXGIAdapter1* aAdapter,
      RefPtr<ID3D11Device>& aOutDevice,
      HRESULT& aResOut);

  void AttemptWARPDeviceCreation();
  bool AttemptWARPDeviceCreationHelper(
      mozilla::ScopedGfxFeatureReporter& aReporterWARP,
      RefPtr<ID3D11Device>& aOutDevice,
      HRESULT& aResOut);

  bool AttemptD3D11ImageBridgeDeviceCreationHelper(
      IDXGIAdapter1* aAdapter, HRESULT& aResOut);
  mozilla::gfx::FeatureStatus AttemptD3D11ImageBridgeDeviceCreation();

  mozilla::gfx::FeatureStatus AttemptD3D11ContentDeviceCreation();
  bool AttemptD3D11ContentDeviceCreationHelper(
      IDXGIAdapter1* aAdapter, HRESULT& aResOut);

  // Create a D3D11 device to be used for DXVA decoding.
  bool CreateD3D11DecoderDeviceHelper(
      IDXGIAdapter1* aAdapter, RefPtr<ID3D11Device>& aDevice,
      HRESULT& aResOut);

  bool ContentAdapterIsParentAdapter(ID3D11Device* device);

private:
  static StaticAutoPtr<DeviceManagerD3D11> sInstance;

  mozilla::Mutex mDeviceLock;
  nsTArray<D3D_FEATURE_LEVEL> mFeatureLevels;
  RefPtr<IDXGIAdapter1> mAdapter;
  RefPtr<ID3D11Device> mCompositorDevice;
  RefPtr<ID3D11Device> mContentDevice;
  RefPtr<ID3D11Device> mImageBridgeDevice;
  mozilla::Atomic<bool> mIsWARP;
  mozilla::Atomic<bool> mTextureSharingWorks;
};

} // namespace gfx
} // namespace mozilla

#endif // mozilla_gfx_thebes_DeviceManagerD3D11_h
