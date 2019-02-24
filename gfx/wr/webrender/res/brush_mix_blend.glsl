/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define VECS_PER_SPECIFIC_BRUSH 3

#include shared,prim_shared,brush

varying vec4 vSourceAndBackdropUv;
flat varying ivec4 vSourceUvBounds;
flat varying ivec4 vBackdropUvBounds;
flat varying ivec3 vOpAndLayers;

#ifdef WR_VERTEX_SHADER

//Note: this function is unsafe for `vi.world_pos.w <= 0.0`
vec2 snap_device_pos(VertexInfo vi, float device_pixel_scale) {
    return vi.world_pos.xy * device_pixel_scale / max(0.0, vi.world_pos.w) + vi.snap_offset;
}

ivec4 rect_to_ivec(RectWithSize rect) {
    return ivec4(rect.p0, rect.p0 + rect.size - 1.0);
}

void brush_vs(
    VertexInfo vi,
    int prim_address,
    RectWithSize local_rect,
    RectWithSize segment_rect,
    ivec4 user_data,
    mat4 transform,
    PictureTask pic_task,
    int brush_flags,
    vec4 unused
) {
    vec2 snapped_device_pos = snap_device_pos(vi, pic_task.device_pixel_scale);

    PictureTask src_task = fetch_picture_task(user_data.z);
    PictureTask backdrop_task = fetch_picture_task(user_data.y);

    vec2 src_uv = snapped_device_pos +
                  src_task.common_data.task_rect.p0 -
                  src_task.content_origin;
    vec2 backdrop_uv = snapped_device_pos +
                       backdrop_task.common_data.task_rect.p0 -
                       backdrop_task.content_origin;

    vSourceAndBackdropUv = vec4(src_uv, backdrop_uv);
    vSourceUvBounds = rect_to_ivec(src_task.common_data.task_rect);
    vBackdropUvBounds = rect_to_ivec(backdrop_task.common_data.task_rect);
    vOpAndLayers = ivec3(
        user_data.x,
        int(src_task.common_data.texture_layer_index),
        int(backdrop_task.common_data.texture_layer_index)
    );
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

const int MixBlendMode_Multiply    = 1;
const int MixBlendMode_Screen      = 2;
const int MixBlendMode_Overlay     = 3;
const int MixBlendMode_Darken      = 4;
const int MixBlendMode_Lighten     = 5;
const int MixBlendMode_ColorDodge  = 6;
const int MixBlendMode_ColorBurn   = 7;
const int MixBlendMode_HardLight   = 8;
const int MixBlendMode_SoftLight   = 9;
const int MixBlendMode_Difference  = 10;
const int MixBlendMode_Exclusion   = 11;
const int MixBlendMode_Hue         = 12;
const int MixBlendMode_Saturation  = 13;
const int MixBlendMode_Color       = 14;
const int MixBlendMode_Luminosity  = 15;

Fragment brush_fs() {
    // Return yellow if none of the branches match (shouldn't happen).
    vec4 result = vec4(1.0, 1.0, 0.0, 1.0);

    ivec2 source_uv = ivec2(floor(vSourceAndBackdropUv.xy));
    vec4 Cs = source_uv == clamp(source_uv, vSourceUvBounds.xy, vSourceUvBounds.zw) ?
        texelFetch(sPrevPassColor, ivec3(source_uv, vOpAndLayers.y), 0) :
        vec4(0.0);
    ivec2 backdrop_uv = ivec2(floor(vSourceAndBackdropUv.zw));
    vec4 Cb = backdrop_uv == clamp(backdrop_uv, vBackdropUvBounds.xy, vBackdropUvBounds.zw) ?
        texelFetch(sPrevPassColor, ivec3(backdrop_uv, vOpAndLayers.z), 0) :
        vec4(0.0);

    if (Cs.a == 0.0) {
        result = Cb;
    } else if (Cb.a == 0.0) {
        result = Cs;
    } else {
        vec3 original_backdrop = Cb.rgb;
        // The mix-blend-mode functions assume no premultiplied alpha
        Cs.rgb /= Cs.a;
        Cb.rgb /= Cb.a;

        switch (vOpAndLayers.x) {
            case MixBlendMode_Multiply:
                result.rgb = Multiply(Cb.rgb, Cs.rgb);
                break;
            case MixBlendMode_Screen:
                result.rgb = Screen(Cb.rgb, Cs.rgb);
                break;
            case MixBlendMode_Overlay:
                // Overlay is inverse of Hardlight
                result.rgb = HardLight(Cs.rgb, Cb.rgb);
                break;
            case MixBlendMode_Darken:
                result.rgb = min(Cs.rgb, Cb.rgb);
                break;
            case MixBlendMode_Lighten:
                result.rgb = max(Cs.rgb, Cb.rgb);
                break;
            case MixBlendMode_ColorDodge:
                result.r = ColorDodge(Cb.r, Cs.r);
                result.g = ColorDodge(Cb.g, Cs.g);
                result.b = ColorDodge(Cb.b, Cs.b);
                break;
            case MixBlendMode_ColorBurn:
                result.r = ColorBurn(Cb.r, Cs.r);
                result.g = ColorBurn(Cb.g, Cs.g);
                result.b = ColorBurn(Cb.b, Cs.b);
                break;
            case MixBlendMode_HardLight:
                result.rgb = HardLight(Cb.rgb, Cs.rgb);
                break;
            case MixBlendMode_SoftLight:
                result.r = SoftLight(Cb.r, Cs.r);
                result.g = SoftLight(Cb.g, Cs.g);
                result.b = SoftLight(Cb.b, Cs.b);
                break;
            case MixBlendMode_Difference:
                result.rgb = Difference(Cb.rgb, Cs.rgb);
                break;
            case MixBlendMode_Exclusion:
                result.rgb = Exclusion(Cb.rgb, Cs.rgb);
                break;
            case MixBlendMode_Hue:
                result.rgb = Hue(Cb.rgb, Cs.rgb);
                break;
            case MixBlendMode_Saturation:
                result.rgb = Saturation(Cb.rgb, Cs.rgb);
                break;
            case MixBlendMode_Color:
                result.rgb = Color(Cb.rgb, Cs.rgb);
                break;
            case MixBlendMode_Luminosity:
                result.rgb = Luminosity(Cb.rgb, Cs.rgb);
                break;
            default: break;
        }

        vec3 rgb = (1.0 - Cb.a) * Cs.rgb + Cb.a * result.rgb;
        // simulate alpha-blending with the backdrop
        result = mix(vec4(original_backdrop, Cb.a), vec4(rgb, 1.0), Cs.a);
    }

    return Fragment(result);
}
#endif
