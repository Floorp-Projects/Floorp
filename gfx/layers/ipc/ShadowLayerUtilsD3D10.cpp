/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <d3d10_1.h>
#include <dxgi.h>

#include "mozilla/gfx/Point.h"
#include "mozilla/layers/LayerManagerComposite.h"
#include "mozilla/layers/PLayerTransaction.h"
#include "ShadowLayers.h"

using namespace mozilla::gl;

namespace mozilla {
namespace layers {


/*static*/ void
ShadowLayerForwarder::PlatformSyncBeforeUpdate()
{
}

/*static*/ bool
LayerManagerComposite::SupportsDirectTexturing()
{
  return true;
}

/*static*/ void
LayerManagerComposite::PlatformSyncBeforeReplyUpdate()
{
}

bool
GetDescriptor(ID3D10Texture2D* aTexture, SurfaceDescriptorD3D10* aDescr)
{
  NS_ABORT_IF_FALSE(aTexture && aDescr, "Params must be nonnull");

  HRESULT hr;
  IDXGIResource* dr = nullptr;
  hr = aTexture->QueryInterface(__uuidof(IDXGIResource), (void**)&dr);
  if (!SUCCEEDED(hr) || !dr)
    return false;

  hr = dr->GetSharedHandle(reinterpret_cast<HANDLE*>(&aDescr->handle()));
  return !!SUCCEEDED(hr);
}

already_AddRefed<ID3D10Texture2D>
OpenForeign(ID3D10Device* aDevice, const SurfaceDescriptorD3D10& aDescr)
{
  HRESULT hr;
  ID3D10Texture2D* tex = nullptr;
  hr = aDevice->OpenSharedResource(reinterpret_cast<HANDLE>(aDescr.handle()),
                                   __uuidof(ID3D10Texture2D),
                                   (void**)&tex);
  if (!SUCCEEDED(hr) || !tex)
    return nullptr;

  return nsRefPtr<ID3D10Texture2D>(tex).forget();
}

} // namespace layers
} // namespace mozilla
