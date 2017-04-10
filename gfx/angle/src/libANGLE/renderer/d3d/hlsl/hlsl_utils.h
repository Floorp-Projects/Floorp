//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// hlsl_utils: Helper functions for HLSL.

#ifndef LIBANGLE_RENDERER_D3D_HLSL_HLSL_UTILS_H_
#define LIBANGLE_RENDERER_D3D_HLSL_HLSL_UTILS_H_

#include <string>

namespace rx
{

enum ShaderType
{
    SHADER_VERTEX,
    SHADER_PIXEL,
    SHADER_GEOMETRY,
    SHADER_TYPE_MAX
};

inline std::string GetVaryingSemantic(int majorShaderModel, bool programUsesPointSize)
{
    // SM3 reserves the TEXCOORD semantic for point sprite texcoords (gl_PointCoord)
    // In D3D11 we manually compute gl_PointCoord in the GS.
    return ((programUsesPointSize && majorShaderModel < 4) ? "COLOR" : "TEXCOORD");
}

}  // namespace rx

#endif  // LIBANGLE_RENDERER_D3D_HLSL_HLSL_UTILS_H_
