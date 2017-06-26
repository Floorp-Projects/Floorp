/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "common-ps.hlsl"
#include "diagnostics-common.hlsl"

Texture2D sTexture: register(ps, t0);

float4 DiagnosticTextPS(const VS_DIAGOUTPUT aInput) : SV_Target
{
  return sTexture.Sample(sSampler, aInput.vTexCoord);
}
