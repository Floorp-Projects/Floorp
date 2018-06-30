//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// GLES1Shaders.inc: Defines GLES1 emulation shader.

constexpr char kGLES1DrawVShader[] = R"(#version 300 es
precision highp float;

#define kMaxTexUnits                         4

in vec4 pos;
in vec3 normal;
in vec4 color;
in float pointsize;
in vec4 texcoord0;
in vec4 texcoord1;
in vec4 texcoord2;
in vec4 texcoord3;

uniform mat4 projection;
uniform mat4 modelview;
uniform mat4 modelview_invtr;
uniform mat4 texture_matrix[4];

out vec4 pos_varying;
out vec3 normal_varying;
out vec4 color_varying;
flat out vec4 color_varying_flat;
out float pointsize_varying;
out vec4 texcoord0_varying;
out vec4 texcoord1_varying;
out vec4 texcoord2_varying;
out vec4 texcoord3_varying;

void main()
{
    pos_varying = modelview * pos;
    mat3 mvInvTr3 = mat3(modelview_invtr);
    normal_varying = mvInvTr3 * normal;

    color_varying = color;
    color_varying_flat = color;
    pointsize_varying = pointsize;
    texcoord0_varying = texture_matrix[0] * texcoord0;
    texcoord1_varying = texture_matrix[1] * texcoord1;
    texcoord2_varying = texture_matrix[2] * texcoord2;
    texcoord3_varying = texture_matrix[3] * texcoord3;

    vec4 vertexPos = projection * modelview * pos;
    gl_Position = vertexPos;
}
)";

// version, flat,
constexpr char kGLES1DrawFShaderHeader[] = R"(#version 300 es
precision highp float;

// Defines for GL constants
#define kMaxLights                           8
#define kMaxTexUnits                         4

#define kModulate                       0x2100
#define kDecal                          0x2101
#define kCombine                        0x8570
#define kReplace                        0x1E01
#define kBlend                          0x0BE2
#define kAdd                            0x0104

#define kAddSigned                      0x8574
#define kInterpolate                    0x8575
#define kSubtract                       0x84E7
#define kDot3Rgb                        0x86AE
#define kDot3Rgba                       0x86AF

#define kAlpha                          0x1906
#define kRGB                            0x1907
#define kRGBA                           0x1908
#define kLuminance                      0x1909
#define kLuminanceAlpha                 0x190A

#define kTexture                        0x1702
#define kConstant                       0x8576
#define kPrimaryColor                   0x8577
#define kPrevious                       0x8578

#define kSrcColor                       0x0300
#define kOneMinusSrcColor               0x0301
#define kSrcAlpha                       0x0302
#define kOneMinusSrcAlpha               0x0303

#define kLinear                         0x2601
#define kExp                            0x0800
#define kExp2                           0x0801

#define kNever                          0x0200
#define kLess                           0x0201
#define kEqual                          0x0202
#define kLequal                         0x0203
#define kGreater                        0x0204
#define kNotequal                       0x0205
#define kGequal                         0x0206
#define kAlways                         0x0207
#define kZero                              0x0
#define kOne                               0x1

#define kClear                          0x1500
#define kAnd                            0x1501
#define kAnd_reverse                    0x1502
#define kCopy                           0x1503
#define kAnd_inverted                   0x1504
#define kNoop                           0x1505
#define kXor                            0x1506
#define kOr                             0x1507
#define kNor                            0x1508
#define kEquiv                          0x1509
#define kInvert                         0x150A
#define kOr_reverse                     0x150B
#define kCopy_inverted                  0x150C
#define kOr_inverted                    0x150D
#define kNand                           0x150E
#define kSet                            0x150F)";

constexpr char kGLES1DrawFShaderUniformDefs[] = R"(

// Texture units ///////////////////////////////////////////////////////////////

uniform bool enable_texture_2d[kMaxTexUnits];
uniform bool enable_texture_cube_map[kMaxTexUnits];

// These are not arrays because hw support for arrays
// of samplers is rather lacking.

uniform sampler2D tex_sampler0;
uniform samplerCube tex_cube_sampler0;

uniform sampler2D tex_sampler1;
uniform samplerCube tex_cube_sampler1;

uniform sampler2D tex_sampler2;
uniform samplerCube tex_cube_sampler2;

uniform sampler2D tex_sampler3;
uniform samplerCube tex_cube_sampler3;

// Global enables, alpha func, logic op ////////////////////////////////////////

uniform bool shade_model_flat;

// Vertex attributes////////////////////////////////////////////////////////////

in vec4 pos_varying;
in vec3 normal_varying;
in vec4 color_varying;
flat in vec4 color_varying_flat;
in float pointsize_varying;
in vec4 texcoord0_varying;
in vec4 texcoord1_varying;
in vec4 texcoord2_varying;
in vec4 texcoord3_varying;

// Outgoing fragment////////////////////////////////////////////////////////////

out vec4 frag_color;
)";

constexpr char kGLES1DrawFShaderFunctions[] = R"(

bool isTextureUnitEnabled(int unit)
{
    return enable_texture_2d[unit] || enable_texture_cube_map[unit];
}

vec4 getTextureColor(int unit)
{
    vec4 res;

    switch (unit)
    {
    case 0:
        if (enable_texture_2d[0])
        {
            res = texture(tex_sampler0, texcoord0_varying.xy);
        }
        else if (enable_texture_cube_map[0])
        {
            res = texture(tex_cube_sampler0, texcoord0_varying.xyz);
        }
        break;
    case 1:
        if (enable_texture_2d[1])
        {
            res = texture(tex_sampler1, texcoord1_varying.xy);
        }
        else if (enable_texture_cube_map[1])
        {
            res = texture(tex_cube_sampler1, texcoord1_varying.xyz);
        }
        break;
    case 2:
        if (enable_texture_2d[2])
        {
            res = texture(tex_sampler2, texcoord2_varying.xy);
        }
        else if (enable_texture_cube_map[2])
        {
            res = texture(tex_cube_sampler2, texcoord2_varying.xyz);
        }
        break;
    case 3:
        if (enable_texture_2d[3])
        {
            res = texture(tex_sampler3, texcoord3_varying.xy);
        }
        else if (enable_texture_cube_map[3])
        {
            // TODO: Weird stuff happens
            // res = texture(tex_cube_sampler3, texcoord3_varying.xyz);
        }
        break;
    default:
        break;
    }

    return res;
}

vec4 getColor(int unit, vec4 vertexColor)
{
    if (!isTextureUnitEnabled(unit))
    {
        return vertexColor;
    }

    return getTextureColor(unit);
}

)";

constexpr char kGLES1DrawFShaderMain[] = R"(
void main()
{
    vec4 currentFragment;
    vec4 vertex_color;

    if (shade_model_flat)
    {
        vertex_color = color_varying_flat;
    }
    else
    {
        vertex_color = color_varying;
    }

    // Unit 0 only for now
    currentFragment = getColor(0, vertex_color);

    frag_color = currentFragment;
}
)";
