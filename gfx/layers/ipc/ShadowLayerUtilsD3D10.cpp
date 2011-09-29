/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at:
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Code.
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation
 * Portions created by the Initial Developer are Copyrigght (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chris Jones <jones.chris.g@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <d3d10_1.h>
#include <dxgi.h>

#include "mozilla/layers/PLayers.h"
#include "ShadowLayers.h"

namespace mozilla {
namespace layers {

// Platform-specific shadow-layers interfaces.  See ShadowLayers.h.
// D3D10 doesn't need all these yet.
bool
ShadowLayerForwarder::PlatformAllocDoubleBuffer(const gfxIntSize&,
                                                gfxASurface::gfxContentType,
                                                SurfaceDescriptor*,
                                                SurfaceDescriptor*)
{
  return PR_FALSE;
}

bool
ShadowLayerForwarder::PlatformAllocBuffer(const gfxIntSize&,
                                          gfxASurface::gfxContentType,
                                          SurfaceDescriptor*)
{
  return PR_FALSE;
}

/*static*/ already_AddRefed<gfxASurface>
ShadowLayerForwarder::PlatformOpenDescriptor(const SurfaceDescriptor&)
{
  return nsnull;
}

bool
ShadowLayerForwarder::PlatformDestroySharedSurface(SurfaceDescriptor*)
{
  return PR_FALSE;
}

/*static*/ void
ShadowLayerForwarder::PlatformSyncBeforeUpdate()
{
}

bool
ShadowLayerManager::PlatformDestroySharedSurface(SurfaceDescriptor*)
{
  return PR_FALSE;
}

/*static*/ void
ShadowLayerManager::PlatformSyncBeforeReplyUpdate()
{
}

bool
GetDescriptor(ID3D10Texture2D* aTexture, SurfaceDescriptorD3D10* aDescr)
{
  NS_ABORT_IF_FALSE(aTexture && aDescr, "Params must be nonnull");

  HRESULT hr;
  IDXGIResource* dr = nsnull;
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
  ID3D10Texture2D* tex = nsnull;
  hr = aDevice->OpenSharedResource(reinterpret_cast<HANDLE>(aDescr.handle()),
                                   __uuidof(ID3D10Texture2D),
                                   (void**)&tex);
  if (!SUCCEEDED(hr) || !tex)
    return nsnull;

  // XXX FIXME TODO do we need this???
  return nsRefPtr<ID3D10Texture2D>(tex).forget();
}

} // namespace layers
} // namespace mozilla
