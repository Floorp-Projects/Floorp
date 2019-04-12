/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_SHADERDEFINITIONSMLGPU_H
#define MOZILLA_GFX_SHADERDEFINITIONSMLGPU_H

#include "mozilla/gfx/Point.h"
#include "mozilla/gfx/Triangle.h"
#include "mozilla/gfx/Types.h"
#include "mozilla/layers/LayersHelpers.h"
#include "nsTArray.h"

namespace mozilla {
namespace layers {

struct ItemInfo;
class ShaderRenderPass;

namespace mlg {

// These may need to move into run-time values determined by MLGDevice.
static const size_t kConstantBufferElementSize = 16;
static const size_t kMaxConstantBufferSize = 4096 * kConstantBufferElementSize;

// Vertex shader slots. We reverse the first two slots across all shaders,
// and the first three slots free across all RenderPass shaders, for
// uniformity.
static const uint32_t kWorldConstantBufferSlot = 0;
static const uint32_t kLayerBufferSlot = 1;
static const uint32_t kMaskBufferSlot = 3;
static const uint32_t kBlendConstantBufferSlot = 4;
static const uint32_t kClearConstantBufferSlot = 2;

// This is specified in common-ps.hlsl.
static const uint32_t kMaskLayerTextureSlot = 4;
static const uint32_t kDefaultSamplerSlot = 0;
static const uint32_t kMaskSamplerSlot = 1;

// These are the maximum slot numbers we bind. We assert that no binding
// happens above the max slot, since we try to clear buffer bindings at
// the end of each frame.
static const uint32_t kMaxVertexShaderConstantBuffers = 5;
static const uint32_t kMaxPixelShaderConstantBuffers = 3;

// Maximum depth in the depth buffer. This must match common-vs.hlsl.
static const int32_t kDepthLimit = 1000000;

struct WorldConstants {
  float projection[4][4];
  gfx::Point targetOffset;
  int sortIndexOffset;
  unsigned debugFrameNumber;
};

struct ClearConstants {
  explicit ClearConstants(int aDepth) : depth(aDepth) {}
  int depth;
  int padding[3];
};

struct LayerConstants {
  float transform[4][4];
  gfx::Rect clipRect;
  uint32_t maskIndex;
  uint32_t padding[3];
};

struct MaskCombineInput {
  float texCoords[4];
};

struct MaskInformation {
  MaskInformation(float aOpacity, bool aHasMask)
      : opacity(aOpacity), hasMask(aHasMask ? 1 : 0) {}
  float opacity;
  uint32_t hasMask;
  uint32_t padding[2];
};

struct YCbCrShaderConstants {
  float yuvColorMatrix[3][4];
};

struct YCbCrColorDepthConstants {
  float coefficient;
  uint32_t padding[3];
};

struct BlendVertexShaderConstants {
  float backdropTransform[4][4];
};

template <typename T>
static inline nsTArray<gfx::IntRect> ToRectArray(const T& aRegion) {
  nsTArray<gfx::IntRect> rects;
  for (auto iter = aRegion.RectIter(); !iter.Done(); iter.Next()) {
    rects.AppendElement(iter.Get().ToUnknownRect());
  }
  return rects;
}

struct SimpleTraits {
  SimpleTraits(const ItemInfo& aItem, const gfx::Rect& aRect)
      : mItem(aItem), mRect(aRect) {}

  // Helper nonce structs so functions can break vertex data up by each
  // triangle in a quad, or return vertex info for a unit quad.
  struct AnyTriangle {};
  struct FirstTriangle : AnyTriangle {};
  struct SecondTriangle : AnyTriangle {};
  struct UnitQuad {};

  // This is the base vertex layout used by all unit quad shaders.
  struct UnitQuadVertex {
    gfx::Rect rect;
    uint32_t layerIndex;
    int depth;
  };

  // This is the base vertex layout used by all unit triangle shaders.
  struct TriangleVertices {
    gfx::Point p1, p2, p3;
    uint32_t layerIndex;
    int depth;
  };

  // Helper functions for populating a TriangleVertices. The first two use mRect
  // to generate triangles, the third function uses coordinates from an already
  // computed triangle.
  TriangleVertices MakeVertex(const FirstTriangle& aIgnore) const;
  TriangleVertices MakeVertex(const SecondTriangle& aIgnore) const;
  TriangleVertices MakeVertex(const gfx::Triangle& aTriangle) const;

  UnitQuadVertex MakeUnitQuadVertex() const;

  // This default GenerateTriangles only computes the 3 points of each triangle
  // in the polygon. If needed, shaders can override this and return a more
  // complex triangle, to encode dependent information in extended vertex data.
  //
  // AddShaderVertices will deduce this return type. It should be an nsTArray<T>
  // where T inherits from Triangle.
  nsTArray<gfx::Triangle> GenerateTriangles(const gfx::Polygon& aPolygon) const;

  // Accessors.
  const Maybe<gfx::Polygon>& geometry() const;
  const gfx::Rect& rect() const { return mRect; }

  const ItemInfo& mItem;
  gfx::Rect mRect;
};

struct ColorTraits : public SimpleTraits {
  ColorTraits(const ItemInfo& aItem, const gfx::Rect& aRect,
              const gfx::Color& aColor)
      : SimpleTraits(aItem, aRect), mColor(aColor) {}

  // Color data is the same across all vertex types.
  template <typename VertexType>
  const gfx::Color& MakeVertexData(const VertexType& aIgnore) const {
    return mColor;
  }

  gfx::Color mColor;
};

struct TexturedTraits : public SimpleTraits {
  TexturedTraits(const ItemInfo& aItem, const gfx::Rect& aRect,
                 const gfx::Rect& aTexCoords)
      : SimpleTraits(aItem, aRect), mTexCoords(aTexCoords) {}

  // Textured triangles need to compute a texture coordinate for each vertex.
  nsTArray<gfx::TexturedTriangle> GenerateTriangles(
      const gfx::Polygon& aPolygon) const;

  struct VertexData {
    gfx::Point p1, p2, p3;
  };
  VertexData MakeVertexData(const FirstTriangle& aIgnore) const;
  VertexData MakeVertexData(const SecondTriangle& aIgnore) const;
  VertexData MakeVertexData(const gfx::TexturedTriangle& aTriangle) const;
  const gfx::Rect& MakeVertexData(const UnitQuad& aIgnore) const {
    return mTexCoords;
  }

  gfx::Rect mTexCoords;
};

}  // namespace mlg
}  // namespace layers
}  // namespace mozilla

#endif
