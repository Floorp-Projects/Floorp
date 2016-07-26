/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_gfx_thebes_DeviceManagerD3D11_h
#define mozilla_gfx_thebes_DeviceManagerD3D11_h

#include "mozilla/RefPtr.h"
#include "mozilla/StaticPtr.h"

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
namespace gfx {

class DeviceManagerD3D11 final
{
public:
  static void Init();
  static void Shutdown();

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

private:
  static StaticAutoPtr<DeviceManagerD3D11> sInstance;
};

} // namespace gfx
} // namespace mozilla

#endif // mozilla_gfx_thebes_DeviceManagerD3D11_h
