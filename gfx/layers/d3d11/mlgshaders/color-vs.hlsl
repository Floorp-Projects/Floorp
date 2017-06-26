/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "common-vs.hlsl"
#include "color-common.hlsl"

struct ColorItem
{
  float4 color;
};
#define SIZEOF_COLORITEM 1

ColorItem GetItem(uint aIndex)
{
  uint offset = aIndex * SIZEOF_COLORITEM;
  ColorItem item;
  item.color = sItems[offset + 0];
  return item;
}

struct VS_COLORQUAD
{
  float2 vPos : POSITION;
  float4 vRect : TEXCOORD0;
  uint vLayerId : TEXCOORD1;
  int vDepth : TEXCOORD2;
  uint vIndex : SV_InstanceID;
};

struct VS_COLORVERTEX
{
  float2 vLayerPos : POSITION;
  uint vLayerId : TEXCOORD0;
  int vDepth : TEXCOORD1;
  uint vIndex : TEXCOORD2;
};

VS_COLOROUTPUT ColorImpl(float4 aColor, const VertexInfo aInfo)
{
  VS_COLOROUTPUT output;
  output.vPosition = aInfo.worldPos;
  output.vLocalPos = aInfo.screenPos;
  output.vColor = aColor;
  output.vClipRect = aInfo.clipRect;
  output.vMaskCoords = aInfo.maskCoords;
  return output;
}

VS_COLOROUTPUT_CLIPPED ColoredQuadVS(const VS_COLORQUAD aInput)
{
  ColorItem item = GetItem(aInput.vIndex);
  float4 worldPos = ComputeClippedPosition(
    aInput.vPos,
    aInput.vRect,
    aInput.vLayerId,
    aInput.vDepth);

  VS_COLOROUTPUT_CLIPPED output;
  output.vPosition = worldPos;
  output.vColor = item.color;
  return output;
}

VS_COLOROUTPUT ColoredVertexVS(const VS_COLORVERTEX aInput)
{
  ColorItem item = GetItem(aInput.vIndex);
  VertexInfo info = ComputePosition(aInput.vLayerPos, aInput.vLayerId, aInput.vDepth);
  return ColorImpl(item.color, info);
}
