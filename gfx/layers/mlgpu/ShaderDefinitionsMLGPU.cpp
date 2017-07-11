/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ShaderDefinitionsMLGPU.h"
#include "RenderPassMLGPU.h"

namespace mozilla {
namespace layers {
namespace mlg {

using namespace gfx;

// Helper function for adding triangle vertices to shader buffers.
struct TriangleVertex
{
  gfx::Point point[3];
  uint32_t layerIndex;
  int depth;
  uint32_t itemIndex;
};

bool
SimpleTraits::AddVerticesTo(ShaderRenderPass* aPass,
                            const ItemInfo& aItem,
                            uint32_t aItemIndex,
                            const gfx::Polygon* aGeometry) const
{
  VertexStagingBuffer* vertices = aPass->GetInstances();

  // If we don't have geometry, take a fast path where we can hardcode
  // the set of triangles.
  if (!aGeometry) {
    TriangleVertex first = {
      { mRect.BottomLeft(), mRect.TopLeft(), mRect.TopRight() },
      aItem.layerIndex, aItem.sortOrder, aItemIndex
    };
    TriangleVertex second = {
      { mRect.TopRight(), mRect.BottomRight(), mRect.BottomLeft() },
      aItem.layerIndex, aItem.sortOrder, aItemIndex
    };
    if (!vertices->PrependItem(first) || !vertices->PrependItem(second)) {
      return false;
    }
    return true;
  }

  // Slow path: full-fledged geometry.
  nsTArray<Triangle> triangles = aGeometry->ToTriangles();
  for (const Triangle& t : triangles) {
    TriangleVertex v = {
      { t.p1, t.p2, t.p3 },
      aItem.layerIndex, aItem.sortOrder, aItemIndex
    };
    if (!vertices->PrependItem(v)) {
      return false;
    }
  }
  return true;
}

struct TexturedTriangleVertex
{
  gfx::Point layerPos[3];
  uint32_t layerIndex;
  int depth;
  gfx::Point texCoords[3];
};

bool
TexturedTraits::AddVerticesTo(ShaderRenderPass* aPass,
                              const ItemInfo& aItem,
                              uint32_t aItemIndex,
                              const gfx::Polygon* aGeometry) const
{
  VertexStagingBuffer* vertices = aPass->GetInstances();

  using Vertex = TexturedTriangleVertex;

  // If we don't have geometry, take a fast path where we can hardcode
  // the set of triangles.
  if (!aGeometry) {
    TexturedTriangleVertex first = {
      { mRect.BottomLeft(), mRect.TopLeft(), mRect.TopRight() },
      aItem.layerIndex, aItem.sortOrder,
      { mTexCoords.BottomLeft(), mTexCoords.TopLeft(), mTexCoords.TopRight() }
    };
    TexturedTriangleVertex second = {
      { mRect.TopRight(), mRect.BottomRight(), mRect.BottomLeft() },
      aItem.layerIndex, aItem.sortOrder,
      { mTexCoords.TopRight(), mTexCoords.BottomRight(), mTexCoords.BottomLeft() }
    };
    if (!vertices->PrependItem(first) || !vertices->PrependItem(second)) {
      return false;
    }
    return true;
  }

  // Slow path: full-fledged geometry.
  nsTArray<TexturedTriangle> triangles =
    GenerateTexturedTriangles(*aGeometry, mRect, mTexCoords);
  for (const TexturedTriangle& t: triangles) {
    TexturedTriangleVertex v = {
      { t.p1, t.p2, t.p3 },
      aItem.layerIndex, aItem.sortOrder,
      { t.textureCoords.p1, t.textureCoords.p2, t.textureCoords.p3 }
    };
    if (!vertices->PrependItem(v)) {
      return false;
    }
  }
  return true;
}

} // namespace mlg
} // namespace layers
} // namespace mozilla
