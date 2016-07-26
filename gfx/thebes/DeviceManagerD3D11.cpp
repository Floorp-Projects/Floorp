/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DeviceManagerD3D11.h"
#include "gfxWindowsPlatform.h"
#include <d3d11.h>

namespace mozilla {
namespace gfx {

StaticAutoPtr<DeviceManagerD3D11> DeviceManagerD3D11::sInstance;

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

RefPtr<ID3D11Device>
DeviceManagerD3D11::GetCompositorDevice()
{
  RefPtr<ID3D11Device> device;
  if (!gfxWindowsPlatform::GetPlatform()->GetD3D11Device(&device))
    return nullptr;
  return device;
}

RefPtr<ID3D11Device>
DeviceManagerD3D11::GetImageBridgeDevice()
{
  RefPtr<ID3D11Device> device;
  if (!gfxWindowsPlatform::GetPlatform()->GetD3D11ImageBridgeDevice(&device)) {
    return nullptr;
  }
  return device;
}

RefPtr<ID3D11Device>
DeviceManagerD3D11::GetContentDevice()
{
  return gfxWindowsPlatform::GetPlatform()->GetD3D11ContentDevice();
}

RefPtr<ID3D11Device>
DeviceManagerD3D11::GetDeviceForCurrentThread()
{
  RefPtr<ID3D11Device> device;
  if (!gfxWindowsPlatform::GetPlatform()->GetD3D11DeviceForCurrentThread(&device)) {
    return nullptr;
  }
  return device;
}

RefPtr<ID3D11Device>
DeviceManagerD3D11::CreateDecoderDevice()
{
  return gfxWindowsPlatform::GetPlatform()->CreateD3D11DecoderDevice();
}

unsigned
DeviceManagerD3D11::GetD3D11Version() const
{
  return gfxWindowsPlatform::GetPlatform()->GetD3D11Version();
}

bool
DeviceManagerD3D11::TextureSharingWorks() const
{
  return gfxWindowsPlatform::GetPlatform()->CompositorD3D11TextureSharingWorks();
}

bool
DeviceManagerD3D11::IsWARP() const
{
  return gfxWindowsPlatform::GetPlatform()->IsWARP();
}

} // namespace gfx
} // namespace mozilla
