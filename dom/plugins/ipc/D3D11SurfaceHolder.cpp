/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 8 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nsDebug.h"
#include "D3D11SurfaceHolder.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/DeviceManagerD3D11.h"
#include "mozilla/layers/TextureD3D11.h"
#include <d3d11.h>

namespace mozilla {
namespace plugins {

using namespace mozilla::gfx;
using namespace mozilla::layers;

D3D11SurfaceHolder::D3D11SurfaceHolder(ID3D11Texture2D* back,
                                       SurfaceFormat format,
                                       const IntSize& size)
 : mDevice11(DeviceManagerD3D11::Get()->GetContentDevice()),
   mBack(back),
   mFormat(format),
   mSize(size)
{
}

D3D11SurfaceHolder::~D3D11SurfaceHolder()
{
}

bool
D3D11SurfaceHolder::IsValid()
{
  // If a TDR occurred, platform devices will be recreated.
  if (DeviceManagerD3D11::Get()->GetContentDevice() != mDevice11) {
     return false;
  }
  return true;
}

bool
D3D11SurfaceHolder::CopyToTextureClient(TextureClient* aClient)
{
  MOZ_ASSERT(NS_IsMainThread());

  D3D11TextureData* data = aClient->GetInternalData()->AsD3D11TextureData();
  if (!data) {
    // We don't support this yet. We expect to have a D3D11 compositor, and
    // therefore D3D11 surfaces.
    NS_WARNING("Plugin DXGI surface has unsupported TextureClient");
    return false;
  }

  RefPtr<ID3D11DeviceContext> context;
  mDevice11->GetImmediateContext(getter_AddRefs(context));
  if (!context) {
    NS_WARNING("Could not get an immediate D3D11 context");
    return false;
  }

  TextureClientAutoLock autoLock(aClient, OpenMode::OPEN_WRITE_ONLY);
  if (!autoLock.Succeeded()) {
    return false;
  }

  RefPtr<IDXGIKeyedMutex> mutex;
  HRESULT hr = mBack->QueryInterface(__uuidof(IDXGIKeyedMutex), (void **)getter_AddRefs(mutex));
  if (FAILED(hr) || !mutex) {
    NS_WARNING("Could not acquire an IDXGIKeyedMutex");
    return false;
  }

  hr = mutex->AcquireSync(0, 0);
  if (hr == WAIT_ABANDONED || hr == WAIT_TIMEOUT || FAILED(hr)) {
    NS_WARNING("Could not acquire DXGI surface lock - plugin forgot to release?");
    return false;
  }

  context->CopyResource(data->GetD3D11Texture(), mBack);

  mutex->ReleaseSync(0);
  return true;
}

} // namespace plugins
} // namespace mozilla
