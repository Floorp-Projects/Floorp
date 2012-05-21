/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_ShadowLayerUtilsD3D10_h
#define mozilla_layers_ShadowLayerUtilsD3D10_h

#define MOZ_HAVE_PLATFORM_SPECIFIC_LAYER_BUFFERS

struct ID3D10Device;
struct ID3D10Texture2D;

namespace mozilla {
namespace layers {

class SurfaceDescriptorD3D10;

/**
 * Write into |aDescr| a cross-process descriptor of |aTexture|, if
 * possible.  Return true iff |aDescr| was successfully set.
 */
bool
GetDescriptor(ID3D10Texture2D* aTexture, SurfaceDescriptorD3D10* aDescr);

already_AddRefed<ID3D10Texture2D>
OpenForeign(ID3D10Device* aDevice, const SurfaceDescriptorD3D10& aDescr);

} // namespace layers
} // namespace mozilla

#endif  // mozilla_layers_ShadowLayerUtilsD3D10_h
