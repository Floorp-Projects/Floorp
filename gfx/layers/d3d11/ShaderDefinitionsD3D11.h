/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_gfx_layers_d3d11_ShaderDefinitionsD3D11_h
#define mozilla_gfx_layers_d3d11_ShaderDefinitionsD3D11_h

#include "mozilla/gfx/Rect.h"

namespace mozilla {
namespace layers {

struct VertexShaderConstants
{
  float layerTransform[4][4];
  float projection[4][4];
  float renderTargetOffset[4];
  gfx::Rect textureCoords;
  gfx::Rect layerQuad;
  float maskTransform[4][4];
  float backdropTransform[4][4];
};

struct PixelShaderConstants
{
  float layerColor[4];
  float layerOpacity[4];
  int blendConfig[4];
  float yuvColorMatrix[3][4];
};

struct Vertex
{
    float position[2];
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_gfx_layers_d3d11_ShaderDefinitionsD3D11_h
