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

inline nsTArray<gfx::Triangle>
SimpleTraits::GenerateTriangles(const gfx::Polygon& aPolygon) const
{
  return aPolygon.ToTriangles();
}

inline bool
SimpleTraits::AddInstanceTo(ShaderRenderPass* aPass) const
{
  return aPass->GetInstances()->AddItem(SimpleVertex(
    mRect, mItem.layerIndex, mItem.sortOrder));
}

inline SimpleTraits::TriangleVertices
SimpleTraits::MakeVertex(const FirstTriangle& aIgnore) const
{
  TriangleVertices v = {
    mRect.BottomLeft(), mRect.TopLeft(), mRect.TopRight(),
    mItem.layerIndex, mItem.sortOrder
  };
  return v;
}

inline SimpleTraits::TriangleVertices
SimpleTraits::MakeVertex(const SecondTriangle& aIgnore) const
{
  TriangleVertices v = {
    mRect.TopRight(), mRect.BottomRight(), mRect.BottomLeft(),
    mItem.layerIndex, mItem.sortOrder
  };
  return v;
}

inline SimpleTraits::TriangleVertices
SimpleTraits::MakeVertex(const gfx::Triangle& aTriangle) const
{
  TriangleVertices v = {
    aTriangle.p1, aTriangle.p2, aTriangle.p3,
    mItem.layerIndex, mItem.sortOrder
  };
  return v;
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

inline nsTArray<gfx::TexturedTriangle>
TexturedTraits::GenerateTriangles(const gfx::Polygon& aPolygon) const
{
  return GenerateTexturedTriangles(aPolygon, mRect, mTexCoords);
}

inline TexturedTraits::VertexData
TexturedTraits::MakeVertexData(const FirstTriangle& aIgnore, uint32_t aItemIndex) const
{
  VertexData v = { mTexCoords.BottomLeft(), mTexCoords.TopLeft(), mTexCoords.TopRight() };
  return v;
}

inline TexturedTraits::VertexData
TexturedTraits::MakeVertexData(const SecondTriangle& aIgnore, uint32_t aItemIndex) const
{
  VertexData v = { mTexCoords.TopRight(), mTexCoords.BottomRight(), mTexCoords.BottomLeft() };
  return v;
}

inline TexturedTraits::VertexData
TexturedTraits::MakeVertexData(const gfx::TexturedTriangle& aTriangle, uint32_t aItemIndex) const
{
  VertexData v = { aTriangle.textureCoords.p1, aTriangle.textureCoords.p2, aTriangle.textureCoords.p3 };
  return v;
}

} // namespace mlg
} // namespace layers
} // namespace mozilla

#endif // _include_gfx_layers_mlgpu_ShaderDefinitions_inl_h
