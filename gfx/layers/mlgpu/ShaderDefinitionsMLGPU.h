/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
* This Source Code Form is subject to the terms of the Mozilla Public
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
static const uint32_t kItemBufferSlot = 2;
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
static const uint32_t kMaxPixelShaderConstantBuffers = 2;

// Maximum depth in the depth buffer. This must match common-vs.hlsl.
static const int32_t kDepthLimit = 1000000;

struct WorldConstants
{
  float projection[4][4];
  gfx::Point targetOffset;
  int sortIndexOffset;
  float padding;
};

struct ClearConstants
{
  explicit ClearConstants(int aDepth) : depth(aDepth)
  {}
  int depth;
  int padding[3];
};

struct LayerConstants
{
  float transform[4][4];
  gfx::Rect clipRect;
  uint32_t maskIndex;
  uint32_t padding[3];
};

struct MaskCombineInput
{
  float texCoords[4];
};

struct MaskInformation
{
  MaskInformation(float aOpacity, bool aHasMask)
   : opacity(aOpacity),
     hasMask(aHasMask ? 1 : 0)
  {}
  float opacity;
  uint32_t hasMask;
  uint32_t padding[2];
};

struct YCbCrShaderConstants {
  float yuvColorMatrix[3][4];
};

struct BlendVertexShaderConstants {
  float backdropTransform[4][4];
};

struct SimpleTraits
{
  explicit SimpleTraits(const gfx::Rect& aRect)
   : mRect(aRect)
  {}

  bool AddInstanceTo(ShaderRenderPass* aPass, const ItemInfo& aItem) const;
  bool AddVerticesTo(ShaderRenderPass* aPass,
                     const ItemInfo& aItem,
                     uint32_t aItemIndex,
                     const gfx::Polygon* aGeometry = nullptr) const;

  gfx::Rect mRect;
};

struct ColorTraits : public SimpleTraits
{
  ColorTraits(const gfx::Rect& aRect, const gfx::Color& aColor)
   : SimpleTraits(aRect), mColor(aColor)
  {}

  bool AddItemTo(ShaderRenderPass* aPass) const;

  gfx::Color mColor;
};

struct TexturedTraits : public SimpleTraits
{
  TexturedTraits(const gfx::Rect& aRect, const gfx::Rect& aTexCoords)
   : SimpleTraits(aRect), mTexCoords(aTexCoords)
  {}

  bool AddVerticesTo(ShaderRenderPass* aPass,
                     const ItemInfo& aItem,
                     uint32_t aItemIndex,
                     const gfx::Polygon* aGeometry = nullptr) const;
  bool AddItemTo(ShaderRenderPass* aPass) const;

  gfx::Rect mTexCoords;
};

} // namespace mlg
} // namespace layers
} // namespace mozilla

#endif
