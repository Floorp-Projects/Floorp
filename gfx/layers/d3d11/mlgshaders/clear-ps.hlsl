/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "common-ps.hlsl"
#include "clear-common.hlsl"

float4 ClearPS(const VS_CLEAR_OUT aVS) : SV_Target
{
  return float4(0.0, 0.0, 0.0, 0.0);
}

