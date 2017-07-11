/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_gfx_layers_d3d11_mlgshaders_common_vs_hlsl
#define mozilla_gfx_layers_d3d11_mlgshaders_common_vs_hlsl

#include "common.hlsl"

cbuffer VSBufSimple : register(b0)
{
  float4x4 WorldTransform;
  float2 RenderTargetOffset;
  int SortIndexOffset;
  float padding;
};

struct Layer {
  float4x4 transform;
  float4 clipRect;
  uint4 info;
};

cbuffer Layers : register(b1)
{
  Layer sLayers[682];
};

cbuffer MaskRects : register(b3)
{
  float4 sMaskRects[4096];
};

struct VertexInfo {
  float4 worldPos;
  float2 screenPos;
  float3 maskCoords;
  float4 clipRect;
};

float3 ComputeMaskCoords(float4 aPosition, Layer aLayer)
{
  if (aLayer.info.x == 0) {
    return float3(0.0, 0.0, 0.0);
  }

  float4 maskRect = sMaskRects[aLayer.info.x - 1];

  // See the perspective comment in CompositorD3D11.hlsl.
  float4x4 transform = float4x4(
    1.0/maskRect.z, 0.0,            0.0, -maskRect.x/maskRect.z,
    0.0,            1.0/maskRect.w, 0.0, -maskRect.y/maskRect.w,
    0.0,            0.0,            1.0, 0.0,
    0.0,            0.0,            0.0, 1.0);

  return float3(mul(transform, aPosition / aPosition.w).xy, 1.0) * aPosition.w;
}

float2 UnitTriangleToPos(const float3 aVertex,
                         const float2 aPos1,
                         const float2 aPos2,
                         const float2 aPos3)
{
  return aVertex.x * aPos1 +
         aVertex.y * aPos2 +
         aVertex.z * aPos3;
}

float2 UnitQuadToRect(const float2 aVertex, const float4 aRect)
{
  return float2(aRect.x + aVertex.x * aRect.z, aRect.y + aVertex.y * aRect.w);
}

float ComputeDepth(float4 aPosition, float aSortIndex)
{
  // Note: this value should match ShaderDefinitionsMLGPU.h.
  return ((aSortIndex + SortIndexOffset) / 1000000.0f) * aPosition.w;
}

// Compute the world-space, screen-space, layer-space clip, and mask
// uv-coordinates given a layer-space vertex, id, and z-index.
VertexInfo ComputePosition(float2 aVertex, uint aLayerId, float aSortIndex)
{
  Layer layer = sLayers[aLayerId];

  // Translate from unit vertex to layer quad vertex.
  float4 position = float4(aVertex, 0, 1);
  float4 clipRect = layer.clipRect;

  // Transform to screen coordinates.
  float4x4 transform = layer.transform;
  position = mul(transform, position);
  position.xyz /= position.w;
  position.xy -= RenderTargetOffset.xy;
  position.xyz *= position.w;

  float4 worldPos = mul(WorldTransform, position);

  // Depth must be computed after the world transform, since we don't want
  // 3d transforms clobbering the z-value. We assume a viewport culling
  // everything outside of [0, 1). Note that when depth-testing, we do not
  // use sorting indices < 1.
  //
  // Note that we have to normalize this value to w=1, since the GPU will
  // divide all values by w internally.
  worldPos.z = ComputeDepth(worldPos, aSortIndex);

  VertexInfo info;
  info.screenPos = position.xy;
  info.worldPos = worldPos;
  info.maskCoords = ComputeMaskCoords(position, layer);
  info.clipRect = clipRect;
  return info;
}

// This function takes a unit quad position and a layer rectangle, and computes
// a clipped draw rect. It is only valid to use this function for layers with
// rectilinear transforms that do not have masks.
float4 ComputeClippedPosition(const float2 aVertex,
                              const float4 aRect,
                              uint aLayerId,
                              float aDepth)
{
  Layer layer = sLayers[aLayerId];

  float4 position = float4(UnitQuadToRect(aVertex, aRect), 0, 1);

  float4x4 transform = layer.transform;
  float4 clipRect = layer.clipRect;

  // Transform to screen coordinates.
  //
  // We clamp the draw rect to the clip. This lets us use faster shaders.
  // For opaque shapes, it is necessary to do this anyway since we might
  // otherwrite write transparent pixels in the pixel which would also be
  // written to the depth buffer. We cannot use discard in the pixel shader
  // as this would break early-z tests.
  //
  // Note that for some shaders, like textured shaders, it is not valid to
  // change the draw rect like this without also clamping the texture
  // coordinates. We take care to adjust for this in our batching code.
  //
  // We do not need to do this for 3D transforms since we always treat those
  // as transparent (they are not written to the depth buffer). 3D items
  // will always use the full clip+masking shader.
  position = mul(transform, position);
  position.xyz /= position.w;
  position.xy -= RenderTargetOffset.xy;
  position.xy = clamp(position.xy, clipRect.xy, clipRect.xy + clipRect.zw);
  position.xyz *= position.w;

  float4 worldPos = mul(WorldTransform, position);

  // Depth must be computed after the world transform, since we don't want
  // 3d transforms clobbering the z-value. We assume a viewport culling
  // everything outside of [0, 1). Note that when depth-testing, we do not
  // use sorting indices < 1.
  //
  // Note that we have to normalize this value to w=1, since the GPU will
  // divide all values by w internally.
  worldPos.z = ComputeDepth(worldPos, aDepth);

  return worldPos;
}

#endif // mozilla_gfx_layers_d3d11_mlgshaders_common_vs_hlsl
