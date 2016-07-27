/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_gfx_thebes_D3D11Checks_h
#define mozilla_gfx_thebes_D3D11Checks_h

struct ID3D11Device;
struct DXGI_ADAPTER_DESC;

namespace mozilla {
namespace gfx {

struct D3D11Checks
{
  static bool DoesRenderTargetViewNeedRecreating(ID3D11Device* aDevice);
  static bool DoesDeviceWork();
  static bool DoesTextureSharingWork(ID3D11Device *device);
  static bool DoesAlphaTextureSharingWork(ID3D11Device *device);
  static void WarnOnAdapterMismatch(ID3D11Device* device);
  static bool GetDxgiDesc(ID3D11Device* device, DXGI_ADAPTER_DESC* out);
};

} // namespace gfx
} // namespace mozilla

#endif // mozilla_gfx_thebes_D3D11Checks_h
