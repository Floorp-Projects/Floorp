/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef WR_FEATURE_TEXTURE_EXTERNAL
// Please check https://www.khronos.org/registry/OpenGL/extensions/OES/OES_EGL_image_external_essl3.txt
// for this extension.
#extension GL_OES_EGL_image_external_essl3 : require
#endif

#ifdef WR_FEATURE_DUAL_SOURCE_BLENDING
#extension GL_ARB_explicit_attrib_location : require
#endif

#include base

// The textureLod() doesn't support samplerExternalOES for WR_FEATURE_TEXTURE_EXTERNAL.
// https://www.khronos.org/registry/OpenGL/extensions/OES/OES_EGL_image_external_essl3.txt
//
// The textureLod() doesn't support sampler2DRect for WR_FEATURE_TEXTURE_RECT, too.
//
// Use texture() instead.
#if defined(WR_FEATURE_TEXTURE_EXTERNAL) || defined(WR_FEATURE_TEXTURE_RECT) || defined(WR_FEATURE_TEXTURE_2D)
#define TEX_SAMPLE(sampler, tex_coord) texture(sampler, tex_coord.xy)
#else
// In normal case, we use textureLod(). We haven't used the lod yet. So, we always pass 0.0 now.
#define TEX_SAMPLE(sampler, tex_coord) textureLod(sampler, tex_coord, 0.0)
#endif

//======================================================================================
// Vertex shader attributes and uniforms
//======================================================================================
#ifdef WR_VERTEX_SHADER
    // A generic uniform that shaders can optionally use to configure
    // an operation mode for this batch.
    uniform int uMode;

    // Uniform inputs
    uniform mat4 uTransform;       // Orthographic projection
    uniform float uDevicePixelRatio;

    // Attribute inputs
    in vec3 aPosition;
#endif

//======================================================================================
// Fragment shader attributes and uniforms
//======================================================================================
#ifdef WR_FRAGMENT_SHADER
    // Uniform inputs

    // Fragment shader outputs
    #ifdef WR_FEATURE_DUAL_SOURCE_BLENDING
        layout(location = 0, index = 0) out vec4 oFragColor;
        layout(location = 0, index = 1) out vec4 oFragBlend;
    #else
        out vec4 oFragColor;
    #endif
#endif

//======================================================================================
// Shared shader uniforms
//======================================================================================
#ifdef WR_FEATURE_TEXTURE_2D
uniform sampler2D sColor0;
uniform sampler2D sColor1;
uniform sampler2D sColor2;
#elif defined WR_FEATURE_TEXTURE_RECT
uniform sampler2DRect sColor0;
uniform sampler2DRect sColor1;
uniform sampler2DRect sColor2;
#elif defined WR_FEATURE_TEXTURE_EXTERNAL
uniform samplerExternalOES sColor0;
uniform samplerExternalOES sColor1;
uniform samplerExternalOES sColor2;
#else
uniform sampler2DArray sColor0;
uniform sampler2DArray sColor1;
uniform sampler2DArray sColor2;
#endif

#ifdef WR_FEATURE_DITHERING
uniform sampler2D sDither;
#endif

//======================================================================================
// Interpolator definitions
//======================================================================================

//======================================================================================
// VS only types and UBOs
//======================================================================================

//======================================================================================
// VS only functions
//======================================================================================
