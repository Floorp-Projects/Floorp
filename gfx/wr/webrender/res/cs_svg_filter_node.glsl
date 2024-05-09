/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
Notes about how this shader works and the requirements it faces:
* Each filter has a _CONVERTSRGB variant that converts to linear before
  performing the operation and converts back to sRGB for output.  Since the
  inputs and output of this shader are premultiplied alpha, we have to undo the
  premultiply and then convert the sRGB color to linearRGB color, perform the
  desired operations, and then convert back to sRGB and then premultiply again.
* For some operations the _CONVERTSRGB variant is never invoked by WebRender, an
  example is OPACITY where the two modes have identical results, as scaling RGBA
  by a single scalar value only changes the opacity, without changing color
  relative to alpha, the sRGB vs linearRGB gamut mapping is relative to alpha.
* SVG filters are usually in linear space so the _CONVERTSRGB variant is used
  heavily in SVG filter graphs, whereas CSS filters use the regular variant.
* Handling of color-interpolation for feFlood and feDropShadow is out of scope
  for this shader, the values can be converted in the WebRender or Gecko code if
  necessary.
* All SVG filters have a subregion rect to clip the operation to, in many cases
  this can just be an alteration of the task uvrect in WebRender, but in some
  cases we might need to enforce it in the shader.
* All filters have an offset for each input, this is an optimization for folding
  feOffset into the downstream nodes of the graph, because it is inefficient to
  be copying an image just to scroll it, and feOffset is not rare.

Notes about specific filter kinds:
* FILTER_BLEND_* kinds follow spec
  https://drafts.fxtf.org/compositing-1/#blending which says to mix from
  Rs to B() based on Rb.a, then mix from Rb to that color based on Rs.a.
* FILTER_COMPOSITE_* kinds use math from Skia as it is elegant.
* FILTER_COMPONENT_TRANSFER_* kinds assume a [4][256] table in gpucache.
* FILTER_DROP_SHADOW_* composites Rs source over the dropshadow in Rb.a,
  it's not actually a composite per se, and needs to be composited onto
  the destination using a separate blend.
*/

#define WR_FEATURE_TEXTURE_2D

#include shared,prim_shared

varying highp vec2 vInput1Uv;
varying highp vec2 vInput2Uv;
flat varying highp vec4 vInput1UvRect;
flat varying highp vec4 vInput2UvRect;
flat varying mediump ivec4 vData;
flat varying mediump vec4 vFilterData0;
flat varying mediump vec4 vFilterData1;

// x: Filter input count, y: Filter kind.
// Packed in to a vector to work around bug 1630356.
flat varying mediump ivec2 vFilterInputCountFilterKindVec;
#define vFilterInputCount vFilterInputCountFilterKindVec.x
#define vFilterKind vFilterInputCountFilterKindVec.y
// Packed in to a vector to work around bug 1630356.
flat varying mediump vec2 vFloat0;

flat varying mediump mat4 vColorMat;
flat varying mediump ivec4 vFuncs;

// must match add_svg_filter_node_instances in render_target.rs
#define FILTER_IDENTITY 0
#define FILTER_IDENTITY_CONVERTSRGB 1
#define FILTER_OPACITY 2
#define FILTER_OPACITY_CONVERTSRGB 3
#define FILTER_TO_ALPHA 4
#define FILTER_TO_ALPHA_CONVERTSRGB 5
#define FILTER_BLEND_COLOR 6
#define FILTER_BLEND_COLOR_CONVERTSRGB 7
#define FILTER_BLEND_COLOR_BURN 8
#define FILTER_BLEND_COLOR_BURN_CONVERTSRGB 9
#define FILTER_BLEND_COLOR_DODGE 10
#define FILTER_BLEND_COLOR_DODGE_CONVERTSRGB 11
#define FILTER_BLEND_DARKEN 12
#define FILTER_BLEND_DARKEN_CONVERTSRGB 13
#define FILTER_BLEND_DIFFERENCE 14
#define FILTER_BLEND_DIFFERENCE_CONVERTSRGB 15
#define FILTER_BLEND_EXCLUSION 16
#define FILTER_BLEND_EXCLUSION_CONVERTSRGB 17
#define FILTER_BLEND_HARD_LIGHT 18
#define FILTER_BLEND_HARD_LIGHT_CONVERTSRGB 19
#define FILTER_BLEND_HUE 20
#define FILTER_BLEND_HUE_CONVERTSRGB 21
#define FILTER_BLEND_LIGHTEN 22
#define FILTER_BLEND_LIGHTEN_CONVERTSRGB 23
#define FILTER_BLEND_LUMINOSITY 24
#define FILTER_BLEND_LUMINOSITY_CONVERTSRGB 25
#define FILTER_BLEND_MULTIPLY 26
#define FILTER_BLEND_MULTIPLY_CONVERTSRGB 27
#define FILTER_BLEND_NORMAL 28
#define FILTER_BLEND_NORMAL_CONVERTSRGB 29
#define FILTER_BLEND_OVERLAY 30
#define FILTER_BLEND_OVERLAY_CONVERTSRGB 31
#define FILTER_BLEND_SATURATION 32
#define FILTER_BLEND_SATURATION_CONVERTSRGB 33
#define FILTER_BLEND_SCREEN 34
#define FILTER_BLEND_SCREEN_CONVERTSRGB 35
#define FILTER_BLEND_SOFT_LIGHT 36
#define FILTER_BLEND_SOFT_LIGHT_CONVERTSRGB 37
#define FILTER_COLOR_MATRIX 38
#define FILTER_COLOR_MATRIX_CONVERTSRGB 39
#define FILTER_COMPONENT_TRANSFER 40
#define FILTER_COMPONENT_TRANSFER_CONVERTSRGB 41
#define FILTER_COMPOSITE_ARITHMETIC 42
#define FILTER_COMPOSITE_ARITHMETIC_CONVERTSRGB 43
#define FILTER_COMPOSITE_ATOP 44
#define FILTER_COMPOSITE_ATOP_CONVERTSRGB 45
#define FILTER_COMPOSITE_IN 46
#define FILTER_COMPOSITE_IN_CONVERTSRGB 47
#define FILTER_COMPOSITE_LIGHTER 48
#define FILTER_COMPOSITE_LIGHTER_CONVERTSRGB 49
#define FILTER_COMPOSITE_OUT 50
#define FILTER_COMPOSITE_OUT_CONVERTSRGB 51
#define FILTER_COMPOSITE_OVER 52
#define FILTER_COMPOSITE_OVER_CONVERTSRGB 53
#define FILTER_COMPOSITE_XOR 54
#define FILTER_COMPOSITE_XOR_CONVERTSRGB 55
#define FILTER_CONVOLVE_MATRIX_EDGE_MODE_DUPLICATE 56
#define FILTER_CONVOLVE_MATRIX_EDGE_MODE_DUPLICATE_CONVERTSRGB 57
#define FILTER_CONVOLVE_MATRIX_EDGE_MODE_NONE 58
#define FILTER_CONVOLVE_MATRIX_EDGE_MODE_NONE_CONVERTSRGB 59
#define FILTER_CONVOLVE_MATRIX_EDGE_MODE_WRAP 60
#define FILTER_CONVOLVE_MATRIX_EDGE_MODE_WRAP_CONVERTSRGB 61
#define FILTER_DIFFUSE_LIGHTING_DISTANT 62
#define FILTER_DIFFUSE_LIGHTING_DISTANT_CONVERTSRGB 63
#define FILTER_DIFFUSE_LIGHTING_POINT 64
#define FILTER_DIFFUSE_LIGHTING_POINT_CONVERTSRGB 65
#define FILTER_DIFFUSE_LIGHTING_SPOT 66
#define FILTER_DIFFUSE_LIGHTING_SPOT_CONVERTSRGB 67
#define FILTER_DISPLACEMENT_MAP 68
#define FILTER_DISPLACEMENT_MAP_CONVERTSRGB 69
#define FILTER_DROP_SHADOW 70
#define FILTER_DROP_SHADOW_CONVERTSRGB 71
#define FILTER_FLOOD 72
#define FILTER_FLOOD_CONVERTSRGB 73
#define FILTER_GAUSSIAN_BLUR 74
#define FILTER_GAUSSIAN_BLUR_CONVERTSRGB 75
#define FILTER_IMAGE 76
#define FILTER_IMAGE_CONVERTSRGB 77
#define FILTER_MORPHOLOGY_DILATE 80
#define FILTER_MORPHOLOGY_DILATE_CONVERTSRGB 81
#define FILTER_MORPHOLOGY_ERODE 82
#define FILTER_MORPHOLOGY_ERODE_CONVERTSRGB 83
#define FILTER_SPECULAR_LIGHTING_DISTANT 86
#define FILTER_SPECULAR_LIGHTING_DISTANT_CONVERTSRGB 87
#define FILTER_SPECULAR_LIGHTING_POINT 88
#define FILTER_SPECULAR_LIGHTING_POINT_CONVERTSRGB 89
#define FILTER_SPECULAR_LIGHTING_SPOT 90
#define FILTER_SPECULAR_LIGHTING_SPOT_CONVERTSRGB 91
#define FILTER_TILE 92
#define FILTER_TILE_CONVERTSRGB 93
#define FILTER_TURBULENCE_WITH_FRACTAL_NOISE_WITH_NO_STITCHING 94
#define FILTER_TURBULENCE_WITH_FRACTAL_NOISE_WITH_NO_STITCHING_CONVERTSRGB 95
#define FILTER_TURBULENCE_WITH_FRACTAL_NOISE_WITH_STITCHING 96
#define FILTER_TURBULENCE_WITH_FRACTAL_NOISE_WITH_STITCHING_CONVERTSRGB 97
#define FILTER_TURBULENCE_WITH_TURBULENCE_NOISE_WITH_NO_STITCHING 98
#define FILTER_TURBULENCE_WITH_TURBULENCE_NOISE_WITH_NO_STITCHING_CONVERTSRGB 99
#define FILTER_TURBULENCE_WITH_TURBULENCE_NOISE_WITH_STITCHING 100
#define FILTER_TURBULENCE_WITH_TURBULENCE_NOISE_WITH_STITCHING_CONVERTSRGB 101

// All of the _CONVERTSRGB variants match this mask
#define FILTER_BITFLAGS_CONVERTSRGB 1

#ifdef WR_VERTEX_SHADER

// due to padding around the target rect, we need to know both target and render task rect
PER_INSTANCE in vec4 aFilterTargetRect;
PER_INSTANCE in vec4 aFilterInput1ContentScaleAndOffset;
PER_INSTANCE in vec4 aFilterInput2ContentScaleAndOffset;
PER_INSTANCE in int aFilterInput1TaskAddress;
PER_INSTANCE in int aFilterInput2TaskAddress;
PER_INSTANCE in int aFilterKind;
PER_INSTANCE in int aFilterInputCount;
PER_INSTANCE in ivec2 aFilterExtraDataAddress;

// used for feFlood and feDropShadow colors
// this is based on SrgbToLinear below, but that version hits SWGL compile
// errors when used in vertex shaders for some reason
vec3 vertexSrgbToLinear(vec3 color) {
    vec3 c1 = color * vec3(1.0 / 12.92);
    vec3 c2;
    c2.r = pow(color.r * (1.0 / 1.055) + (0.055 / 1.055), 2.4);
    c2.g = pow(color.g * (1.0 / 1.055) + (0.055 / 1.055), 2.4);
    c2.b = pow(color.b * (1.0 / 1.055) + (0.055 / 1.055), 2.4);
    return mix(c1, c2, step(vec3(0.04045), color));
}

vec4 compute_uv_rect(RectWithEndpoint task_rect, vec2 texture_size) {
    vec4 uvRect = vec4(task_rect.p0 + vec2(0.5),
                       task_rect.p1 - vec2(0.5));
    uvRect /= texture_size.xyxy;
    return uvRect;
}

vec2 compute_uv(RectWithEndpoint task_rect, vec4 scale_and_offset, vec2 target_size, vec2 texture_size) {
    // SVG spec dictates that we want to *not* scale coordinates between nodes,
    // must be able to sample at offsets, and must be able to fetch outside the
    // clamp rect as transparent black, so we have custom texcoords for the
    // fetch area, separate from the clamp rect
    return (task_rect.p0 + scale_and_offset.zw + scale_and_offset.xy * aPosition.xy) / texture_size.xy;
}

void main(void) {
    vec2 pos = mix(aFilterTargetRect.xy, aFilterTargetRect.zw, aPosition.xy);

    RectWithEndpoint input_1_task;
    if (aFilterInputCount > 0) {
        vec2 texture_size = vec2(TEX_SIZE(sColor0).xy);
        input_1_task = fetch_render_task_rect(aFilterInput1TaskAddress);
        vInput1UvRect = compute_uv_rect(input_1_task, texture_size);
        vInput1Uv = compute_uv(input_1_task, aFilterInput1ContentScaleAndOffset, aFilterTargetRect.zw - aFilterTargetRect.xy, texture_size);
    }

    RectWithEndpoint input_2_task;
    if (aFilterInputCount > 1) {
        vec2 texture_size = vec2(TEX_SIZE(sColor1).xy);
        input_2_task = fetch_render_task_rect(aFilterInput2TaskAddress);
        vInput2UvRect = compute_uv_rect(input_2_task, texture_size);
        vInput2Uv = compute_uv(input_2_task, aFilterInput2ContentScaleAndOffset, aFilterTargetRect.zw - aFilterTargetRect.xy, texture_size);
    }

    vFilterInputCount = aFilterInputCount;
    vFilterKind = aFilterKind;

    switch (aFilterKind) {
        case FILTER_IDENTITY:
        case FILTER_IDENTITY_CONVERTSRGB:
            break;
        case FILTER_OPACITY:
        case FILTER_OPACITY_CONVERTSRGB:
            // opacity takes one input and an alpha value, so we just stuffed
            // that in the unused input 2 content rect
            vFloat0.x = aFilterInput2ContentScaleAndOffset.x;
            break;
        case FILTER_TO_ALPHA:
        case FILTER_TO_ALPHA_CONVERTSRGB:
            break;
        case FILTER_BLEND_COLOR:
        case FILTER_BLEND_COLOR_CONVERTSRGB:
        case FILTER_BLEND_COLOR_BURN:
        case FILTER_BLEND_COLOR_BURN_CONVERTSRGB:
        case FILTER_BLEND_COLOR_DODGE:
        case FILTER_BLEND_COLOR_DODGE_CONVERTSRGB:
        case FILTER_BLEND_DARKEN:
        case FILTER_BLEND_DARKEN_CONVERTSRGB:
        case FILTER_BLEND_DIFFERENCE:
        case FILTER_BLEND_DIFFERENCE_CONVERTSRGB:
        case FILTER_BLEND_EXCLUSION:
        case FILTER_BLEND_EXCLUSION_CONVERTSRGB:
        case FILTER_BLEND_HARD_LIGHT:
        case FILTER_BLEND_HARD_LIGHT_CONVERTSRGB:
        case FILTER_BLEND_HUE:
        case FILTER_BLEND_HUE_CONVERTSRGB:
        case FILTER_BLEND_LIGHTEN:
        case FILTER_BLEND_LIGHTEN_CONVERTSRGB:
        case FILTER_BLEND_LUMINOSITY:
        case FILTER_BLEND_LUMINOSITY_CONVERTSRGB:
        case FILTER_BLEND_MULTIPLY:
        case FILTER_BLEND_MULTIPLY_CONVERTSRGB:
        case FILTER_BLEND_NORMAL:
        case FILTER_BLEND_NORMAL_CONVERTSRGB:
        case FILTER_BLEND_OVERLAY:
        case FILTER_BLEND_OVERLAY_CONVERTSRGB:
        case FILTER_BLEND_SATURATION:
        case FILTER_BLEND_SATURATION_CONVERTSRGB:
        case FILTER_BLEND_SCREEN:
        case FILTER_BLEND_SCREEN_CONVERTSRGB:
        case FILTER_BLEND_SOFT_LIGHT:
        case FILTER_BLEND_SOFT_LIGHT_CONVERTSRGB:
            break;
        case FILTER_COLOR_MATRIX:
        case FILTER_COLOR_MATRIX_CONVERTSRGB:
            vec4 mat_data[4] = fetch_from_gpu_cache_4_direct(aFilterExtraDataAddress);
            vColorMat = mat4(mat_data[0], mat_data[1], mat_data[2], mat_data[3]);
            vFilterData0 = fetch_from_gpu_cache_1_direct(aFilterExtraDataAddress + ivec2(4, 0));
            break;
        case FILTER_COMPONENT_TRANSFER:
        case FILTER_COMPONENT_TRANSFER_CONVERTSRGB:
            vData = ivec4(aFilterExtraDataAddress, 0, 0);
            break;
        case FILTER_COMPOSITE_ARITHMETIC:
        case FILTER_COMPOSITE_ARITHMETIC_CONVERTSRGB:
            // arithmetic parameters
            vFilterData0 = fetch_from_gpu_cache_1_direct(aFilterExtraDataAddress);
            break;
        case FILTER_COMPOSITE_ATOP:
        case FILTER_COMPOSITE_ATOP_CONVERTSRGB:
        case FILTER_COMPOSITE_IN:
        case FILTER_COMPOSITE_IN_CONVERTSRGB:
        case FILTER_COMPOSITE_LIGHTER:
        case FILTER_COMPOSITE_LIGHTER_CONVERTSRGB:
        case FILTER_COMPOSITE_OUT:
        case FILTER_COMPOSITE_OUT_CONVERTSRGB:
        case FILTER_COMPOSITE_OVER:
        case FILTER_COMPOSITE_OVER_CONVERTSRGB:
        case FILTER_COMPOSITE_XOR:
        case FILTER_COMPOSITE_XOR_CONVERTSRGB:
            break;
        case FILTER_CONVOLVE_MATRIX_EDGE_MODE_DUPLICATE:
        case FILTER_CONVOLVE_MATRIX_EDGE_MODE_DUPLICATE_CONVERTSRGB:
            // TODO
            break;
        case FILTER_CONVOLVE_MATRIX_EDGE_MODE_NONE:
        case FILTER_CONVOLVE_MATRIX_EDGE_MODE_NONE_CONVERTSRGB:
            // TODO
            break;
        case FILTER_CONVOLVE_MATRIX_EDGE_MODE_WRAP:
        case FILTER_CONVOLVE_MATRIX_EDGE_MODE_WRAP_CONVERTSRGB:
            // TODO
            break;
        case FILTER_DIFFUSE_LIGHTING_DISTANT:
        case FILTER_DIFFUSE_LIGHTING_DISTANT_CONVERTSRGB:
            // TODO
            break;
        case FILTER_DIFFUSE_LIGHTING_POINT:
        case FILTER_DIFFUSE_LIGHTING_POINT_CONVERTSRGB:
            // TODO
            break;
        case FILTER_DIFFUSE_LIGHTING_SPOT:
        case FILTER_DIFFUSE_LIGHTING_SPOT_CONVERTSRGB:
            // TODO
            break;
        case FILTER_DISPLACEMENT_MAP:
        case FILTER_DISPLACEMENT_MAP_CONVERTSRGB:
            // TODO
            break;
        case FILTER_DROP_SHADOW:
            vFilterData0 = fetch_from_gpu_cache_1_direct(aFilterExtraDataAddress);
            // premultiply the color
            vFilterData0.rgb = vFilterData0.rgb * vFilterData0.a;
            break;
        case FILTER_DROP_SHADOW_CONVERTSRGB:
            vFilterData0 = fetch_from_gpu_cache_1_direct(aFilterExtraDataAddress);
            // convert from sRGB to linearRGB and premultiply by alpha
            vFilterData0.rgb = vertexSrgbToLinear(vFilterData0.rgb);
            vFilterData0.rgb = vFilterData0.rgb * vFilterData0.a;
            break;
        case FILTER_FLOOD:
            // feFlood has no actual input textures, so input 2 rect is color
            vFilterData0 = aFilterInput2ContentScaleAndOffset;
            // premultiply the color
            vFilterData0.rgb = vFilterData0.rgb * vFilterData0.a;
            break;
        case FILTER_FLOOD_CONVERTSRGB:
            // feFlood has no actual input textures, so input 2 rect is color
            vFilterData0 = aFilterInput2ContentScaleAndOffset;
            // convert from sRGB to linearRGB and premultiply by alpha
            vFilterData0.rgb = vertexSrgbToLinear(vFilterData0.rgb);
            vFilterData0.rgb = vFilterData0.rgb * vFilterData0.a;
            break;
        case FILTER_GAUSSIAN_BLUR:
        case FILTER_GAUSSIAN_BLUR_CONVERTSRGB:
            break;
        case FILTER_IMAGE:
        case FILTER_IMAGE_CONVERTSRGB:
            // TODO
            break;
        case FILTER_MORPHOLOGY_DILATE:
        case FILTER_MORPHOLOGY_DILATE_CONVERTSRGB:
        case FILTER_MORPHOLOGY_ERODE:
        case FILTER_MORPHOLOGY_ERODE_CONVERTSRGB:
            // morphology filters have radius values in second input rect
            vFilterData0 = aFilterInput2ContentScaleAndOffset;
            break;
        case FILTER_SPECULAR_LIGHTING_DISTANT:
        case FILTER_SPECULAR_LIGHTING_DISTANT_CONVERTSRGB:
            // TODO
            break;
        case FILTER_SPECULAR_LIGHTING_POINT:
        case FILTER_SPECULAR_LIGHTING_POINT_CONVERTSRGB:
            // TODO
            break;
        case FILTER_SPECULAR_LIGHTING_SPOT:
        case FILTER_SPECULAR_LIGHTING_SPOT_CONVERTSRGB:
            // TODO
            break;
        case FILTER_TILE:
        case FILTER_TILE_CONVERTSRGB:
            // TODO
            break;
        case FILTER_TURBULENCE_WITH_FRACTAL_NOISE_WITH_NO_STITCHING:
        case FILTER_TURBULENCE_WITH_FRACTAL_NOISE_WITH_NO_STITCHING_CONVERTSRGB:
        case FILTER_TURBULENCE_WITH_FRACTAL_NOISE_WITH_STITCHING:
        case FILTER_TURBULENCE_WITH_FRACTAL_NOISE_WITH_STITCHING_CONVERTSRGB:
        case FILTER_TURBULENCE_WITH_TURBULENCE_NOISE_WITH_NO_STITCHING:
        case FILTER_TURBULENCE_WITH_TURBULENCE_NOISE_WITH_NO_STITCHING_CONVERTSRGB:
        case FILTER_TURBULENCE_WITH_TURBULENCE_NOISE_WITH_STITCHING:
        case FILTER_TURBULENCE_WITH_TURBULENCE_NOISE_WITH_STITCHING_CONVERTSRGB:
            // TODO
            break;
        default:
            break;
    }

    gl_Position = uTransform * vec4(pos, 0.0, 1.0);
}
#endif

#ifdef WR_FRAGMENT_SHADER

vec3 Multiply(vec3 Cb, vec3 Cs) {
    return Cb * Cs;
}

vec3 Screen(vec3 Cb, vec3 Cs) {
    return Cb + Cs - (Cb * Cs);
}

vec3 HardLight(vec3 Cb, vec3 Cs) {
    vec3 m = Multiply(Cb, 2.0 * Cs);
    vec3 s = Screen(Cb, 2.0 * Cs - 1.0);
    vec3 edge = vec3(0.5, 0.5, 0.5);
    return mix(m, s, step(edge, Cs));
}

// TODO: Worth doing with mix/step? Check GLSL output.
float ColorDodge(float Cb, float Cs) {
    if (Cb == 0.0)
        return 0.0;
    else if (Cs == 1.0)
        return 1.0;
    else
        return min(1.0, Cb / (1.0 - Cs));
}

// TODO: Worth doing with mix/step? Check GLSL output.
float ColorBurn(float Cb, float Cs) {
    if (Cb == 1.0)
        return 1.0;
    else if (Cs == 0.0)
        return 0.0;
    else
        return 1.0 - min(1.0, (1.0 - Cb) / Cs);
}

float SoftLight(float Cb, float Cs) {
    if (Cs <= 0.5) {
        return Cb - (1.0 - 2.0 * Cs) * Cb * (1.0 - Cb);
    } else {
        float D;

        if (Cb <= 0.25)
            D = ((16.0 * Cb - 12.0) * Cb + 4.0) * Cb;
        else
            D = sqrt(Cb);

        return Cb + (2.0 * Cs - 1.0) * (D - Cb);
    }
}

vec3 Difference(vec3 Cb, vec3 Cs) {
    return abs(Cb - Cs);
}

vec3 Exclusion(vec3 Cb, vec3 Cs) {
    return Cb + Cs - 2.0 * Cb * Cs;
}

// These functions below are taken from the spec.
// There's probably a much quicker way to implement
// them in GLSL...
float Sat(vec3 c) {
    return max(c.r, max(c.g, c.b)) - min(c.r, min(c.g, c.b));
}

float Lum(vec3 c) {
    vec3 f = vec3(0.3, 0.59, 0.11);
    return dot(c, f);
}

vec3 ClipColor(vec3 C) {
    float L = Lum(C);
    float n = min(C.r, min(C.g, C.b));
    float x = max(C.r, max(C.g, C.b));

    if (n < 0.0)
        C = L + (((C - L) * L) / (L - n));

    if (x > 1.0)
        C = L + (((C - L) * (1.0 - L)) / (x - L));

    return C;
}

vec3 SetLum(vec3 C, float l) {
    float d = l - Lum(C);
    return ClipColor(C + d);
}

void SetSatInner(inout float Cmin, inout float Cmid, inout float Cmax, float s) {
    if (Cmax > Cmin) {
        Cmid = (((Cmid - Cmin) * s) / (Cmax - Cmin));
        Cmax = s;
    } else {
        Cmid = 0.0;
        Cmax = 0.0;
    }
    Cmin = 0.0;
}

vec3 SetSat(vec3 C, float s) {
    if (C.r <= C.g) {
        if (C.g <= C.b) {
            SetSatInner(C.r, C.g, C.b, s);
        } else {
            if (C.r <= C.b) {
                SetSatInner(C.r, C.b, C.g, s);
            } else {
                SetSatInner(C.b, C.r, C.g, s);
            }
        }
    } else {
        if (C.r <= C.b) {
            SetSatInner(C.g, C.r, C.b, s);
        } else {
            if (C.g <= C.b) {
                SetSatInner(C.g, C.b, C.r, s);
            } else {
                SetSatInner(C.b, C.g, C.r, s);
            }
        }
    }
    return C;
}

vec3 Hue(vec3 Cb, vec3 Cs) {
    return SetLum(SetSat(Cs, Sat(Cb)), Lum(Cb));
}

vec3 Saturation(vec3 Cb, vec3 Cs) {
    return SetLum(SetSat(Cb, Sat(Cs)), Lum(Cb));
}

vec3 Color(vec3 Cb, vec3 Cs) {
    return SetLum(Cs, Lum(Cb));
}

vec3 Luminosity(vec3 Cb, vec3 Cs) {
    return SetLum(Cb, Lum(Cs));
}

// Based on the Gecko implementation in
// https://hg.mozilla.org/mozilla-central/file/91b4c3687d75/gfx/src/FilterSupport.cpp#l24
// These could be made faster by sampling a lookup table stored in a float texture
// with linear interpolation.

vec3 SrgbToLinear(vec3 color) {
    vec3 c1 = color / 12.92;
    vec3 c2 = pow(color / 1.055 + vec3(0.055 / 1.055), vec3(2.4));
    return if_then_else(lessThanEqual(color, vec3(0.04045)), c1, c2);
}

vec3 LinearToSrgb(vec3 color) {
    vec3 c1 = color * 12.92;
    vec3 c2 = vec3(1.055) * pow(color, vec3(1.0 / 2.4)) - vec3(0.055);
    return if_then_else(lessThanEqual(color, vec3(0.0031308)), c1, c2);
}

vec4 sampleInUvRect(sampler2D sampler, vec2 uv, vec4 uvRect) {
    vec2 clamped = clamp(uv.xy, uvRect.xy, uvRect.zw);
    return texture(sampler, clamped);
}

vec4 sampleInUvRectRepeat(sampler2D sampler, vec2 uv, vec4 uvRect) {
    vec2 size = (uvRect.zw - uvRect.xy);
    vec2 tiled = uv.xy - floor((uv.xy - uvRect.xy) / size) * size;
    return texture(sampler, tiled);
}

void main(void) {
    // Raw premultiplied color of source texture
    vec4 Rs = vec4(0.0, 0.0, 0.0, 0.0);
    // Raw premultiplied color of destination texture
    vec4 Rb = vec4(0.0, 0.0, 0.0, 0.0);
    // Normalized (non-premultiplied) color of source texture
    vec4 Ns = vec4(0.0, 0.0, 0.0, 0.0);
    // Normalized (non-premultiplied) color of destination texture
    vec4 Nb = vec4(0.0, 0.0, 0.0, 0.0);
    // used in FILTER_COMPONENT_TRANSFER
    ivec4 k;
    if (vFilterInputCount > 0) {
        Rs = sampleInUvRect(sColor0, vInput1Uv, vInput1UvRect);
        Ns.rgb = Rs.rgb * (1.0 / max(0.000001, Rs.a));
        Ns.a = Rs.a;
        if ((vFilterKind & FILTER_BITFLAGS_CONVERTSRGB) != 0) {
            Ns.rgb = SrgbToLinear(Ns.rgb);
            Rs.rgb = Ns.rgb * Rs.a;
        }
    }
    if (vFilterInputCount > 1) {
        Rb = sampleInUvRect(sColor1, vInput2Uv, vInput2UvRect);
        Nb.rgb = Rb.rgb * (1.0 / max(0.000001, Rb.a));
        Nb.a = Rb.a;
        if ((vFilterKind & FILTER_BITFLAGS_CONVERTSRGB) != 0) {
            Nb.rgb = SrgbToLinear(Nb.rgb);
            Rb.rgb = Nb.rgb * Rb.a;
        }
    }

    vec4 result = vec4(1.0, 0.0, 0.0, 1.0);

    switch (vFilterKind) {
        case FILTER_IDENTITY:
        case FILTER_IDENTITY_CONVERTSRGB:
            result = Rs;
            break;
        case FILTER_OPACITY:
        case FILTER_OPACITY_CONVERTSRGB:
            result = Rs * vFloat0.x;
            break;
        case FILTER_TO_ALPHA:
        case FILTER_TO_ALPHA_CONVERTSRGB:
            // Just return the alpha, we have literally nothing to do on the RGB
            // values here, this also means CONVERTSRGB is irrelevant.
            oFragColor = vec4(0.0, 0.0, 0.0, Rs.a);
            return;
        case FILTER_BLEND_COLOR:
        case FILTER_BLEND_COLOR_CONVERTSRGB:
            result.rgb = Color(Nb.rgb, Ns.rgb);
            result.rgb = (1.0 - Rb.a) * Rs.rgb + (1.0 - Rs.a) * Rb.rgb + Rs.a * Rb.a * result.rgb;
            result.a = Rb.a * (1.0 - Rs.a) + Rs.a;
            break;
        case FILTER_BLEND_COLOR_BURN:
        case FILTER_BLEND_COLOR_BURN_CONVERTSRGB:
            result.rgb = vec3(ColorBurn(Nb.r, Ns.r), ColorBurn(Nb.g, Ns.g), ColorBurn(Nb.b, Ns.b));
            result.rgb = (1.0 - Rb.a) * Rs.rgb + (1.0 - Rs.a) * Rb.rgb + Rs.a * Rb.a * result.rgb;
            result.a = Rb.a * (1.0 - Rs.a) + Rs.a;
            break;
        case FILTER_BLEND_COLOR_DODGE:
        case FILTER_BLEND_COLOR_DODGE_CONVERTSRGB:
            result.rgb = vec3(ColorDodge(Nb.r, Ns.r), ColorDodge(Nb.g, Ns.g), ColorDodge(Nb.b, Ns.b));
            result.rgb = (1.0 - Rb.a) * Rs.rgb + (1.0 - Rs.a) * Rb.rgb + Rs.a * Rb.a * result.rgb;
            result.a = Rb.a * (1.0 - Rs.a) + Rs.a;
            break;
        case FILTER_BLEND_DARKEN:
        case FILTER_BLEND_DARKEN_CONVERTSRGB:
            result.rgb = Rs.rgb + Rb.rgb - max(Rs.rgb * Rb.a, Rb.rgb * Rs.a);
            result.a = Rb.a * (1.0 - Rs.a) + Rs.a;
            break;
        case FILTER_BLEND_DIFFERENCE:
        case FILTER_BLEND_DIFFERENCE_CONVERTSRGB:
            result.rgb = Rs.rgb + Rb.rgb - 2.0 * min(Rs.rgb * Rb.a, Rb.rgb * Rs.a);
            result.a = Rb.a * (1.0 - Rs.a) + Rs.a;
            break;
        case FILTER_BLEND_EXCLUSION:
        case FILTER_BLEND_EXCLUSION_CONVERTSRGB:
            result.rgb = Rs.rgb + Rb.rgb - 2.0 * (Rs.rgb * Rb.rgb);
            result.a = Rb.a * (1.0 - Rs.a) + Rs.a;
            break;
        case FILTER_BLEND_HARD_LIGHT:
        case FILTER_BLEND_HARD_LIGHT_CONVERTSRGB:
            result.rgb = HardLight(Nb.rgb, Ns.rgb);
            result.rgb = (1.0 - Rb.a) * Rs.rgb + (1.0 - Rs.a) * Rb.rgb + Rs.a * Rb.a * result.rgb;
            result.a = Rb.a * (1.0 - Rs.a) + Rs.a;
            break;
        case FILTER_BLEND_HUE:
        case FILTER_BLEND_HUE_CONVERTSRGB:
            result.rgb = Hue(Nb.rgb, Ns.rgb);
            result.rgb = (1.0 - Rb.a) * Rs.rgb + (1.0 - Rs.a) * Rb.rgb + Rs.a * Rb.a * result.rgb;
            result.a = Rb.a * (1.0 - Rs.a) + Rs.a;
            break;
        case FILTER_BLEND_LIGHTEN:
        case FILTER_BLEND_LIGHTEN_CONVERTSRGB:
            result.rgb = Rs.rgb + Rb.rgb - min(Rs.rgb * Rb.a, Rb.rgb * Rs.a);
            result.a = Rb.a * (1.0 - Rs.a) + Rs.a;
            break;
        case FILTER_BLEND_LUMINOSITY:
        case FILTER_BLEND_LUMINOSITY_CONVERTSRGB:
            result.rgb = Luminosity(Nb.rgb, Ns.rgb);
            result.rgb = (1.0 - Rb.a) * Rs.rgb + (1.0 - Rs.a) * Rb.rgb + Rs.a * Rb.a * result.rgb;
            result.a = Rb.a * (1.0 - Rs.a) + Rs.a;
            break;
        case FILTER_BLEND_MULTIPLY:
        case FILTER_BLEND_MULTIPLY_CONVERTSRGB:
            result.rgb = Rs.rgb * (1.0 - Rb.a) + Rb.rgb * (1.0 - Rs.a) + Rs.rgb * Rb.rgb;
            result.a = Rb.a * (1.0 - Rs.a) + Rs.a;
            break;
        case FILTER_BLEND_NORMAL:
        case FILTER_BLEND_NORMAL_CONVERTSRGB:
            result = Rb * (1.0 - Rs.a) + Rs;
            break;
        case FILTER_BLEND_OVERLAY:
        case FILTER_BLEND_OVERLAY_CONVERTSRGB:
            // Overlay is inverse of Hardlight
            result.rgb = HardLight(Ns.rgb, Nb.rgb);
            result.rgb = (1.0 - Rb.a) * Rs.rgb + (1.0 - Rs.a) * Rb.rgb + Rs.a * Rb.a * result.rgb;
            result.a = Rb.a * (1.0 - Rs.a) + Rs.a;
            break;
        case FILTER_BLEND_SATURATION:
        case FILTER_BLEND_SATURATION_CONVERTSRGB:
            result.rgb = Saturation(Nb.rgb, Ns.rgb);
            result.rgb = (1.0 - Rb.a) * Rs.rgb + (1.0 - Rs.a) * Rb.rgb + Rs.a * Rb.a * result.rgb;
            result.a = Rb.a * (1.0 - Rs.a) + Rs.a;
            break;
        case FILTER_BLEND_SCREEN:
        case FILTER_BLEND_SCREEN_CONVERTSRGB:
            result.rgb = Rs.rgb + Rb.rgb - (Rs.rgb * Rb.rgb);
            result.a = Rb.a * (1.0 - Rs.a) + Rs.a;
            break;
        case FILTER_BLEND_SOFT_LIGHT:
        case FILTER_BLEND_SOFT_LIGHT_CONVERTSRGB:
            result.rgb = vec3(SoftLight(Nb.r, Ns.r), SoftLight(Nb.g, Ns.g), SoftLight(Nb.b, Ns.b));
            result.rgb = (1.0 - Rb.a) * Rs.rgb + (1.0 - Rs.a) * Rb.rgb + Rs.a * Rb.a * result.rgb;
            result.a = Rb.a * (1.0 - Rs.a) + Rs.a;
            break;
        case FILTER_COLOR_MATRIX:
        case FILTER_COLOR_MATRIX_CONVERTSRGB:
            result = vColorMat * Ns + vFilterData0;
            result = clamp(result, vec4(0.0), vec4(1.0));
            result.rgb = result.rgb * result.a;
            break;
        case FILTER_COMPONENT_TRANSFER:
        case FILTER_COMPONENT_TRANSFER_CONVERTSRGB:
            // fetch new value for each channel from the RGBA lookup table.
            result = floor(clamp(Ns * 255.0, vec4(0.0), vec4(255.0)));
            // SWGL doesn't have an intrinsic for ivec4(vec4)
            k = ivec4(int(result.r), int(result.g), int(result.b), int(result.a));
            result.r = fetch_from_gpu_cache_1_direct(vData.xy + ivec2(k.r, 0)).r;
            result.g = fetch_from_gpu_cache_1_direct(vData.xy + ivec2(k.g, 0)).g;
            result.b = fetch_from_gpu_cache_1_direct(vData.xy + ivec2(k.b, 0)).b;
            result.a = fetch_from_gpu_cache_1_direct(vData.xy + ivec2(k.a, 0)).a;
            result.rgb = result.rgb * result.a;
            break;
        case FILTER_COMPOSITE_ARITHMETIC:
        case FILTER_COMPOSITE_ARITHMETIC_CONVERTSRGB:
            result = Rs * Rb * vFilterData0.x + Rs * vFilterData0.y + Rb * vFilterData0.z + vec4(vFilterData0.w);
            result = clamp(result, vec4(0.0), vec4(1.0));
            break;
        case FILTER_COMPOSITE_ATOP:
        case FILTER_COMPOSITE_ATOP_CONVERTSRGB:
            result = Rs * Rb.a + Rb * (1.0 - Rs.a);
            break;
        case FILTER_COMPOSITE_IN:
        case FILTER_COMPOSITE_IN_CONVERTSRGB:
            result = Rs * Rb.a;
            break;
        case FILTER_COMPOSITE_LIGHTER:
        case FILTER_COMPOSITE_LIGHTER_CONVERTSRGB:
            result = Rs + Rb;
            result = clamp(result, vec4(0.0), vec4(1.0));
            break;
        case FILTER_COMPOSITE_OUT:
        case FILTER_COMPOSITE_OUT_CONVERTSRGB:
            result = Rs * (1.0 - Rb.a);
            break;
        case FILTER_COMPOSITE_OVER:
        case FILTER_COMPOSITE_OVER_CONVERTSRGB:
            result = Rs + Rb * (1.0 - Rs.a);
            break;
        case FILTER_COMPOSITE_XOR:
        case FILTER_COMPOSITE_XOR_CONVERTSRGB:
            result = Rs * (1.0 - Rb.a) + Rb * (1.0 - Rs.a);
            break;
        case FILTER_CONVOLVE_MATRIX_EDGE_MODE_DUPLICATE:
        case FILTER_CONVOLVE_MATRIX_EDGE_MODE_DUPLICATE_CONVERTSRGB:
            // TODO
            break;
        case FILTER_CONVOLVE_MATRIX_EDGE_MODE_NONE:
        case FILTER_CONVOLVE_MATRIX_EDGE_MODE_NONE_CONVERTSRGB:
            // TODO
            break;
        case FILTER_CONVOLVE_MATRIX_EDGE_MODE_WRAP:
        case FILTER_CONVOLVE_MATRIX_EDGE_MODE_WRAP_CONVERTSRGB:
            // TODO
            break;
        case FILTER_DIFFUSE_LIGHTING_DISTANT:
        case FILTER_DIFFUSE_LIGHTING_DISTANT_CONVERTSRGB:
            // TODO
            break;
        case FILTER_DIFFUSE_LIGHTING_POINT:
        case FILTER_DIFFUSE_LIGHTING_POINT_CONVERTSRGB:
            // TODO
            break;
        case FILTER_DIFFUSE_LIGHTING_SPOT:
        case FILTER_DIFFUSE_LIGHTING_SPOT_CONVERTSRGB:
            // TODO
            break;
        case FILTER_DISPLACEMENT_MAP:
        case FILTER_DISPLACEMENT_MAP_CONVERTSRGB:
            // TODO
            break;
        case FILTER_DROP_SHADOW:
        case FILTER_DROP_SHADOW_CONVERTSRGB:
            // First input is original image, second input is offset and blurred
            // image, we replace color of second input with vFilterData.rgb and
            // composite with mode OVER.
            // This color is already premultiplied, so it's ready to use
            result = Rs + vFilterData0 * (Rb.a * (1.0 - Rs.a));
            break;
        case FILTER_FLOOD:
        case FILTER_FLOOD_CONVERTSRGB:
            result = vFilterData0;
            break;
        case FILTER_GAUSSIAN_BLUR:
        case FILTER_GAUSSIAN_BLUR_CONVERTSRGB:
            // unused - the IDENTITY filter is used for composing this
            break;
        case FILTER_IMAGE:
        case FILTER_IMAGE_CONVERTSRGB:
            // TODO - we need to get the uvrect set up in the code before
            // this shader case will matter, best to leave it at the fallback
            // color for now when it is known to be broken.
            break;
        case FILTER_MORPHOLOGY_DILATE:
        case FILTER_MORPHOLOGY_DILATE_CONVERTSRGB:
            // TODO
            break;
        case FILTER_MORPHOLOGY_ERODE:
        case FILTER_MORPHOLOGY_ERODE_CONVERTSRGB:
            // TODO
            break;
        case FILTER_SPECULAR_LIGHTING_DISTANT:
        case FILTER_SPECULAR_LIGHTING_DISTANT_CONVERTSRGB:
            // TODO
            break;
        case FILTER_SPECULAR_LIGHTING_POINT:
        case FILTER_SPECULAR_LIGHTING_POINT_CONVERTSRGB:
            // TODO
            break;
        case FILTER_SPECULAR_LIGHTING_SPOT:
        case FILTER_SPECULAR_LIGHTING_SPOT_CONVERTSRGB:
            // TODO
            break;
        case FILTER_TILE:
        case FILTER_TILE_CONVERTSRGB:
            // TODO
            // we can just return the texel without doing anything else
            vec2 tileUv = rect_repeat(vInput1Uv, vInput1UvRect.xy, vInput1UvRect.zw);
            oFragColor = sampleInUvRect(sColor0, tileUv, vInput1UvRect);
            return;
        case FILTER_TURBULENCE_WITH_FRACTAL_NOISE_WITH_NO_STITCHING:
        case FILTER_TURBULENCE_WITH_FRACTAL_NOISE_WITH_NO_STITCHING_CONVERTSRGB:
            // TODO
            break;
        case FILTER_TURBULENCE_WITH_FRACTAL_NOISE_WITH_STITCHING:
        case FILTER_TURBULENCE_WITH_FRACTAL_NOISE_WITH_STITCHING_CONVERTSRGB:
            // TODO
            break;
        case FILTER_TURBULENCE_WITH_TURBULENCE_NOISE_WITH_NO_STITCHING:
        case FILTER_TURBULENCE_WITH_TURBULENCE_NOISE_WITH_NO_STITCHING_CONVERTSRGB:
            // TODO
            break;
        case FILTER_TURBULENCE_WITH_TURBULENCE_NOISE_WITH_STITCHING:
        case FILTER_TURBULENCE_WITH_TURBULENCE_NOISE_WITH_STITCHING_CONVERTSRGB:
            // TODO
            break;
        default:
            break;
    }

    if ((vFilterKind & FILTER_BITFLAGS_CONVERTSRGB) != 0) {
        // convert back to sRGB in unmultiplied color space
        result.rgb = LinearToSrgb(result.rgb * (1.0 / max(0.000001, result.a))) * result.a;
    }

    oFragColor = result;
}
#endif
