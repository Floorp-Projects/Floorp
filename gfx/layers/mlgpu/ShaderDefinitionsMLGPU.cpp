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
  TriangleVertex(const gfx::Point& aPoint,
                 const ItemInfo& aItem,
                 uint32_t aItemIndex)
   : point(aPoint),
     layerIndex(aItem.layerIndex),
     depth(aItem.sortOrder),
     itemIndex(aItemIndex)
  {}

  gfx::Point point;
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
  VertexStagingBuffer* vertices = aPass->GetVertices();

  // If we don't have geometry, take a fast path where we can hardcode
  // the set of triangles.
  if (!aGeometry) {
    if (!vertices->PrependItem(TriangleVertex(mRect.BottomLeft(), aItem, aItemIndex)) ||
        !vertices->PrependItem(TriangleVertex(mRect.TopLeft(), aItem, aItemIndex)) ||
        !vertices->PrependItem(TriangleVertex(mRect.TopRight(), aItem, aItemIndex)) ||
        !vertices->PrependItem(TriangleVertex(mRect.TopRight(), aItem, aItemIndex)) ||
        !vertices->PrependItem(TriangleVertex(mRect.BottomRight(), aItem, aItemIndex)) ||
        !vertices->PrependItem(TriangleVertex(mRect.BottomLeft(), aItem, aItemIndex)))
    {
      return false;
    }
    return true;
  }

  // Slow path: full-fledged geometry.
  nsTArray<Triangle> triangles = aGeometry->ToTriangles();
  for (const Triangle& t : triangles) {
    if (!vertices->PrependItem(TriangleVertex(t.p1, aItem, aItemIndex)) ||
        !vertices->PrependItem(TriangleVertex(t.p2, aItem, aItemIndex)) ||
        !vertices->PrependItem(TriangleVertex(t.p3, aItem, aItemIndex)))
    {
      return false;
    }
  }
  return true;
}

struct TexturedTriangleVertex
{
  TexturedTriangleVertex(const gfx::Point& aPoint,
                         const gfx::Point& aTexCoord,
                         const ItemInfo& aItem)
   : point(aPoint),
     texCoord(aTexCoord),
     layerIndex(aItem.layerIndex),
     depth(aItem.sortOrder)
  {}

  gfx::Point point;
  gfx::Point texCoord;
  uint32_t layerIndex;
  int depth;
};

bool
TexturedTraits::AddVerticesTo(ShaderRenderPass* aPass,
                              const ItemInfo& aItem,
                              uint32_t aItemIndex,
                              const gfx::Polygon* aGeometry) const
{
  VertexStagingBuffer* vertices = aPass->GetVertices();

  using Vertex = TexturedTriangleVertex;

  // If we don't have geometry, take a fast path where we can hardcode
  // the set of triangles.
  if (!aGeometry) {
    if (!vertices->PrependItem(Vertex(mRect.BottomLeft(), mTexCoords.BottomLeft(), aItem)) ||
        !vertices->PrependItem(Vertex(mRect.TopLeft(), mTexCoords.TopLeft(), aItem)) ||
        !vertices->PrependItem(Vertex(mRect.TopRight(), mTexCoords.TopRight(), aItem)) ||
        !vertices->PrependItem(Vertex(mRect.TopRight(), mTexCoords.TopRight(), aItem)) ||
        !vertices->PrependItem(Vertex(mRect.BottomRight(), mTexCoords.BottomRight(), aItem)) ||
        !vertices->PrependItem(Vertex(mRect.BottomLeft(), mTexCoords.BottomLeft(), aItem)))
    {
      return false;
    }
    return true;
  }

  // Slow path: full-fledged geometry.
  nsTArray<TexturedTriangle> triangles =
    GenerateTexturedTriangles(*aGeometry, mRect, mTexCoords);
  for (const TexturedTriangle& t: triangles) {
    if (!vertices->PrependItem(Vertex(t.p1, t.textureCoords.p1, aItem)) ||
        !vertices->PrependItem(Vertex(t.p2, t.textureCoords.p2, aItem)) ||
        !vertices->PrependItem(Vertex(t.p3, t.textureCoords.p3, aItem)))
    {
      return false;
    }
  }
  return true;
}

} // namespace mlg
} // namespace layers
} // namespace mozilla
