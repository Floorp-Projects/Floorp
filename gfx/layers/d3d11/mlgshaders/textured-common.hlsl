/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef VERTEX_SHADER
struct TexturedItem
{
  float4 texCoords;
};
#define SIZEOF_TEXTUREDITEM 1

TexturedItem GetItem(uint aIndex)
{
  uint offset = aIndex * SIZEOF_TEXTUREDITEM;
  TexturedItem item;
  item.texCoords = sItems[offset + 0];
  return item;
}
#endif

// Instanced version.
struct VS_TEXTUREDINPUT
{
  float2 vPos : POSITION;
  float4 vRect : TEXCOORD0;
  uint vLayerId : TEXCOORD1;
  int vDepth : TEXCOORD2;
  uint vIndex : SV_InstanceID;
};

// Non-instanced version.
struct VS_TEXTUREDVERTEX
{
  float3 vUnitPos : POSITION0;
  float2 vPos1: POSITION1;
  float2 vPos2: POSITION2;
  float2 vPos3: POSITION3;
  uint vLayerId : TEXCOORD0;
  int vDepth : TEXCOORD1;
  float2 vTexCoord1 : TEXCOORD2;
  float2 vTexCoord2 : TEXCOORD3;
  float2 vTexCoord3 : TEXCOORD4;
};

struct VS_SAMPLEOUTPUT
{
  float4 vPosition : SV_Position;
  float2 vTexCoords : TEXCOORD0;
  float2 vLocalPos : TEXCOORD1;
  float3 vMaskCoords : TEXCOORD2;
  nointerpolation float4 vClipRect : TEXCOORD3;
};

struct VS_SAMPLEOUTPUT_CLIPPED
{
  float4 vPosition : SV_Position;
  float2 vTexCoords : TEXCOORD0;
};
