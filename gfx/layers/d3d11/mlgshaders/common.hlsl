/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_gfx_layers_d3d11_mlgshaders_common_hlsl
#define mozilla_gfx_layers_d3d11_mlgshaders_common_hlsl

bool RectContainsPoint(float4 aRect, float2 aPoint)
{
  return aPoint.x >= aRect.x &&
         aPoint.y >= aRect.y &&
         aPoint.x <= (aRect.x + aRect.z) &&
         aPoint.y <= (aRect.y + aRect.w);
}

#endif // mozilla_gfx_layers_d3d11_mlgshaders_common_hlsl
