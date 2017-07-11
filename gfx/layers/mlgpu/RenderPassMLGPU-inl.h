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
                   const gfx::Polygon* aGeometry = nullptr)
{
  typedef typename Traits::TriangleVertices TriangleVertices;
  typedef typename Traits::FirstTriangle FirstTriangle;
  typedef typename Traits::SecondTriangle SecondTriangle;

  if (!aGeometry) {
    TriangleVertices base1 = aTraits.MakeVertex(FirstTriangle());
    TriangleVertices base2 = aTraits.MakeVertex(SecondTriangle());
    auto data1 = aTraits.MakeVertexData(FirstTriangle());
    auto data2 = aTraits.MakeVertexData(SecondTriangle());
    return aBuffer->PrependItem(base1, data1) && aBuffer->PrependItem(base2, data2);
  }

  auto triangles = aTraits.GenerateTriangles(*aGeometry);
  for (const auto& triangle : triangles) {
    TriangleVertices base = aTraits.MakeVertex(triangle);
    auto data = aTraits.MakeVertexData(triangle);
    if (!aBuffer->PrependItem(base, data)) {
      return false;
    }
  }
  return true;
}

template <typename Traits>
inline bool
BatchRenderPass<Traits>::Txn::AddImpl(const Traits& aTraits)
{
  VertexStagingBuffer* instances = mPass->GetInstances();

  if (mPass->mGeometry == GeometryMode::Polygon) {
    if (const Maybe<gfx::Polygon>& geometry = aTraits.geometry()) {
      gfx::Polygon polygon = geometry->ClipPolygon(aTraits.rect());
      if (polygon.IsEmpty()) {
        return true;
      }
      return AddShaderTriangles(instances, aTraits, &polygon);
    }
    return AddShaderTriangles(instances, aTraits);
  }

  typedef typename Traits::UnitQuadVertex UnitQuadVertex;
  typedef typename Traits::UnitQuad UnitQuad;

  UnitQuadVertex base = aTraits.MakeUnitQuadVertex();
  auto data = aTraits.MakeVertexData(UnitQuad());
  return instances->AddItem(base, data);
}

} // namespace layers
} // namespace mozilla

#endif // mozilla_gfx_layers_mlgpu_RenderPassMLGPU_inl_h
