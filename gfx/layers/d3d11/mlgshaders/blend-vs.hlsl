/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "common-vs.hlsl"
#include "blend-common.hlsl"
#include "textured-common.hlsl"

cbuffer BlendConstants : register(b4)
{
  float4x4 mBackdropTransform;
}

float2 BackdropPosition(float4 aPosition)
{
  // Move the position from clip space (-1,1) into 0..1 space.
  float2 pos;
  pos.x = (aPosition.x + 1.0) / 2.0;
  pos.y = 1.0 - (aPosition.y + 1.0) / 2.0;

  return mul(mBackdropTransform, float4(pos.xy, 0, 1.0)).xy;
}

VS_BLEND_OUTPUT BlendImpl(const VertexInfo aInfo, float2 aTexCoord)
{
  VS_BLEND_OUTPUT output;
  output.vPosition = aInfo.worldPos;
  output.vTexCoords = aTexCoord;
  output.vBackdropCoords = BackdropPosition(output.vPosition);
  output.vLocalPos = aInfo.screenPos;
  output.vClipRect = aInfo.clipRect;
  output.vMaskCoords = aInfo.maskCoords;
  return output;
}

VS_BLEND_OUTPUT BlendVertexVS(const VS_TEXTUREDVERTEX aVertex)
{
  VertexInfo info = ComputePosition(aVertex.vLayerPos, aVertex.vLayerId, aVertex.vDepth);

  return BlendImpl(info, aVertex.vTexCoord);
}
