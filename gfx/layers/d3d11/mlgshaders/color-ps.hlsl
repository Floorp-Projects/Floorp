/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "common-ps.hlsl"
#include "color-common.hlsl"

float4 ColoredQuadPS(const VS_COLOROUTPUT_CLIPPED aVS) : SV_Target
{
  // Opacity is always 1.0, we premultiply it on the CPU.
  return aVS.vColor;
}

float4 ColoredVertexPS(const VS_COLOROUTPUT aVS) : SV_Target
{
  if (!RectContainsPoint(aVS.vClipRect, aVS.vPosition.xy)) {
    return float4(0, 0, 0, 0);
  }
  return aVS.vColor * ReadMaskWithOpacity(aVS.vMaskCoords, 1.0);
}
