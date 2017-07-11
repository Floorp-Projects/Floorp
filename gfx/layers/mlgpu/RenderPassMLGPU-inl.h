/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_gfx_layers_mlgpu_RenderPassMLGPU_inl_h
#define mozilla_gfx_layers_mlgpu_RenderPassMLGPU_inl_h

namespace mozilla {
namespace layers {

template <typename Traits>
static inline bool
AddShaderTriangles(VertexStagingBuffer* aBuffer,
                   const Traits& aTraits,
                   uint32_t aItemIndex,
                   const gfx::Polygon* aGeometry = nullptr)
{
  typedef typename Traits::TriangleVertices TriangleVertices;
  typedef typename Traits::FirstTriangle FirstTriangle;
  typedef typename Traits::SecondTriangle SecondTriangle;

  if (!aGeometry) {
    TriangleVertices base1 = aTraits.MakeVertex(FirstTriangle());
    TriangleVertices base2 = aTraits.MakeVertex(SecondTriangle());
    auto data1 = aTraits.MakeVertexData(FirstTriangle(), aItemIndex);
    auto data2 = aTraits.MakeVertexData(SecondTriangle(), aItemIndex);
    return aBuffer->PrependItem(base1, data1) && aBuffer->PrependItem(base2, data2);
  }

  auto triangles = aTraits.GenerateTriangles(*aGeometry);
  for (const auto& triangle : triangles) {
    TriangleVertices base = aTraits.MakeVertex(triangle);
    auto data = aTraits.MakeVertexData(triangle, aItemIndex);
    if (!aBuffer->PrependItem(base, data)) {
      return false;
    }
  }
  return true;
}

template <typename Traits>
inline bool
BatchRenderPass<Traits>::Txn::Add(const Traits& aTraits)
{
  // If this succeeds, but we clip the polygon below, that's okay.
  // Polygons do not use instanced rendering so this won't break
  // ordering.
  if (!aTraits.AddItemTo(mPass)) {
    return false;
  }

  if (mPass->mGeometry == GeometryMode::Polygon) {
    size_t itemIndex = mPass->GetItems()->NumItems() - 1;
    if (aTraits.mItem.geometry) {
      gfx::Polygon polygon = aTraits.mItem.geometry->ClipPolygon(aTraits.mRect);
      if (polygon.IsEmpty()) {
        return true;
      }
      return AddShaderTriangles(mPass->GetInstances(), aTraits, itemIndex, &polygon);
    }
    return AddShaderTriangles(mPass->GetInstances(), aTraits, itemIndex);
  }
  return aTraits.AddInstanceTo(mPass);
}

} // namespace layers
} // namespace mozilla

#endif // mozilla_gfx_layers_mlgpu_RenderPassMLGPU_inl_h
