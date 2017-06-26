/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _include_gfx_layers_mlgpu_ShaderDefinitions_inl_h
#define _include_gfx_layers_mlgpu_ShaderDefinitions_inl_h

namespace mozilla {
namespace layers {
namespace mlg {

// This is a helper class for writing vertices for unit-quad based
// shaders, since they all share the same input layout.
struct SimpleVertex
{
  SimpleVertex(const gfx::Rect& aRect,
               uint32_t aLayerIndex,
               int aDepth)
   : rect(aRect),
     layerIndex(aLayerIndex),
     depth(aDepth)
  {}

  gfx::Rect rect;
  uint32_t layerIndex;
  int depth;
};

bool
SimpleTraits::AddInstanceTo(ShaderRenderPass* aPass, const ItemInfo& aItem) const
{
  return aPass->GetInstances()->AddItem(SimpleVertex(
    mRect, aItem.layerIndex, aItem.sortOrder));
}


inline bool
ColorTraits::AddItemTo(ShaderRenderPass* aPass) const
{
  return aPass->GetItems()->AddItem(mColor);
}

inline bool
TexturedTraits::AddItemTo(ShaderRenderPass* aPass) const
{
  return aPass->GetItems()->AddItem(mTexCoords);
}

} // namespace mlg
} // namespace layers
} // namespace mozilla

#endif // _include_gfx_layers_mlgpu_ShaderDefinitions_inl_h
