/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "common-vs.hlsl"
#include "textured-common.hlsl"

VS_SAMPLEOUTPUT TexturedQuadImpl(const VertexInfo aInfo, const float2 aTexCoord)
{
  VS_SAMPLEOUTPUT output;
  output.vPosition = aInfo.worldPos;
  output.vTexCoords = aTexCoord;
  output.vLocalPos = aInfo.screenPos;
  output.vClipRect = aInfo.clipRect;
  output.vMaskCoords = aInfo.maskCoords;
  return output;
}

VS_SAMPLEOUTPUT_CLIPPED TexturedQuadVS(const VS_TEXTUREDINPUT aVertex)
{
  TexturedItem item = GetItem(aVertex.vIndex);

  float4 worldPos = ComputeClippedPosition(
    aVertex.vPos,
    aVertex.vRect,
    aVertex.vLayerId,
    aVertex.vDepth);

  VS_SAMPLEOUTPUT_CLIPPED output;
  output.vPosition = worldPos;
  output.vTexCoords = UnitQuadToRect(aVertex.vPos, item.texCoords);
  return output;
}

VS_SAMPLEOUTPUT TexturedVertexVS(const VS_TEXTUREDVERTEX aVertex)
{
  float2 layerPos = UnitTriangleToPos(
    aVertex.vUnitPos,
    aVertex.vPos1,
    aVertex.vPos2,
    aVertex.vPos3);

  float2 texCoord = UnitTriangleToPos(
    aVertex.vUnitPos,
    aVertex.vTexCoord1,
    aVertex.vTexCoord2,
    aVertex.vTexCoord3);

  VertexInfo info = ComputePosition(layerPos, aVertex.vLayerId, aVertex.vDepth);
  return TexturedQuadImpl(info, texCoord);
}
