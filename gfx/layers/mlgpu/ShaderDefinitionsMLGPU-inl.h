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

inline const Maybe<gfx::Polygon>&
SimpleTraits::geometry() const
{
  return mItem.geometry;
}

inline nsTArray<gfx::Triangle>
SimpleTraits::GenerateTriangles(const gfx::Polygon& aPolygon) const
{
  return aPolygon.ToTriangles();
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

inline SimpleTraits::UnitQuadVertex
SimpleTraits::MakeUnitQuadVertex() const
{
  UnitQuadVertex v = { mRect, mItem.layerIndex, mItem.sortOrder };
  return v;
}

inline nsTArray<gfx::TexturedTriangle>
TexturedTraits::GenerateTriangles(const gfx::Polygon& aPolygon) const
{
  return GenerateTexturedTriangles(aPolygon, mRect, mTexCoords);
}

inline TexturedTraits::VertexData
TexturedTraits::MakeVertexData(const FirstTriangle& aIgnore) const
{
  VertexData v = { mTexCoords.BottomLeft(), mTexCoords.TopLeft(), mTexCoords.TopRight() };
  return v;
}

inline TexturedTraits::VertexData
TexturedTraits::MakeVertexData(const SecondTriangle& aIgnore) const
{
  VertexData v = { mTexCoords.TopRight(), mTexCoords.BottomRight(), mTexCoords.BottomLeft() };
  return v;
}

inline TexturedTraits::VertexData
TexturedTraits::MakeVertexData(const gfx::TexturedTriangle& aTriangle) const
{
  VertexData v = { aTriangle.textureCoords.p1, aTriangle.textureCoords.p2, aTriangle.textureCoords.p3 };
  return v;
}

} // namespace mlg
} // namespace layers
} // namespace mozilla

#endif // _include_gfx_layers_mlgpu_ShaderDefinitions_inl_h
