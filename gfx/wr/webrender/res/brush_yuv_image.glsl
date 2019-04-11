/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define VECS_PER_SPECIFIC_BRUSH 1

#include shared,prim_shared,brush

// TODO(gw): Consider whether we should even have separate shader compilations
//           for the various YUV modes. To save on the number of shaders we
//           need to compile, it might be worth just doing this as an
//           uber-shader instead.

#define YUV_COLOR_SPACE_REC601 0
#define YUV_COLOR_SPACE_REC709 1
#define YUV_COLOR_SPACE_REC2020 2

#define YUV_FORMAT_NV12 0
#define YUV_FORMAT_PLANAR 1
#define YUV_FORMAT_INTERLEAVED 2

#ifdef WR_FEATURE_ALPHA_PASS
varying vec2 vLocalPos;
#endif

varying vec3 vUv_Y;
flat varying vec4 vUvBounds_Y;

varying vec3 vUv_U;
flat varying vec4 vUvBounds_U;

varying vec3 vUv_V;
flat varying vec4 vUvBounds_V;

#ifdef WR_FEATURE_TEXTURE_RECT
    #define TEX_SIZE(sampler) vec2(1.0)
#else
    #define TEX_SIZE(sampler) vec2(textureSize(sampler, 0).xy)
#endif

flat varying float vCoefficient;
flat varying mat3 vYuvColorMatrix;
flat varying int vFormat;

#ifdef WR_VERTEX_SHADER
// The constants added to the Y, U and V components are applied in the fragment shader.

// From Rec601:
// [R]   [1.1643835616438356,  0.0,                 1.5960267857142858   ]   [Y -  16]
// [G] = [1.1643835616438358, -0.3917622900949137, -0.8129676472377708   ] x [U - 128]
// [B]   [1.1643835616438356,  2.017232142857143,   8.862867620416422e-17]   [V - 128]
//
// For the range [0,1] instead of [0,255].
//
// The matrix is stored in column-major.
const mat3 YuvColorMatrixRec601 = mat3(
    1.16438,  1.16438, 1.16438,
    0.0,     -0.39176, 2.01723,
    1.59603, -0.81297, 0.0
);

// From Rec709:
// [R]   [1.1643835616438356,  0.0,                    1.7927410714285714]   [Y -  16]
// [G] = [1.1643835616438358, -0.21324861427372963,   -0.532909328559444 ] x [U - 128]
// [B]   [1.1643835616438356,  2.1124017857142854,     0.0               ]   [V - 128]
//
// For the range [0,1] instead of [0,255]:
//
// The matrix is stored in column-major.
const mat3 YuvColorMatrixRec709 = mat3(
    1.16438,  1.16438,  1.16438,
    0.0    , -0.21325,  2.11240,
    1.79274, -0.53291,  0.0
);

// From Re2020:
// [R]   [1.16438356164384,  0.0,                    1.678674107142860 ]   [Y -  16]
// [G] = [1.16438356164384, -0.187326104219343,     -0.650424318505057 ] x [U - 128]
// [B]   [1.16438356164384,  2.14177232142857,       0.0               ]   [V - 128]
//
// For the range [0,1] instead of [0,255]:
//
// The matrix is stored in column-major.
const mat3 YuvColorMatrixRec2020 = mat3(
    1.16438356164384 ,  1.164383561643840,  1.16438356164384,
    0.0              , -0.187326104219343,  2.14177232142857,
    1.67867410714286 , -0.650424318505057,  0.0
);

void write_uv_rect(
    int resource_id,
    vec2 f,
    vec2 texture_size,
    out vec3 uv,
    out vec4 uv_bounds
) {
    ImageResource res = fetch_image_resource(resource_id);
    vec2 uv0 = res.uv_rect.p0;
    vec2 uv1 = res.uv_rect.p1;

    uv.xy = mix(uv0, uv1, f);
    uv.z = res.layer;

    uv_bounds = vec4(uv0 + vec2(0.5), uv1 - vec2(0.5));

    #ifndef WR_FEATURE_TEXTURE_RECT
        uv.xy /= texture_size;
        uv_bounds /= texture_size.xyxy;
    #endif
}

struct YuvPrimitive {
    float coefficient;
    int color_space;
    int yuv_format;
};

YuvPrimitive fetch_yuv_primitive(int address) {
    vec4 data = fetch_from_gpu_cache_1(address);
    return YuvPrimitive(data.x, int(data.y), int(data.z));
}

void brush_vs(
    VertexInfo vi,
    int prim_address,
    RectWithSize local_rect,
    RectWithSize segment_rect,
    ivec4 prim_user_data,
    int segment_user_data,
    mat4 transform,
    PictureTask pic_task,
    int brush_flags,
    vec4 unused
) {
    vec2 f = (vi.local_pos - local_rect.p0) / local_rect.size;

    YuvPrimitive prim = fetch_yuv_primitive(prim_address);
    vCoefficient = prim.coefficient;

    if (prim.color_space == YUV_COLOR_SPACE_REC601) {
      vYuvColorMatrix = YuvColorMatrixRec601;
    } else if (prim.color_space == YUV_COLOR_SPACE_REC709) {
      vYuvColorMatrix = YuvColorMatrixRec709;
    } else {
      vYuvColorMatrix = YuvColorMatrixRec2020;
    }
    vFormat = prim.yuv_format;

#ifdef WR_FEATURE_ALPHA_PASS
    vLocalPos = vi.local_pos;
#endif

    if (vFormat == YUV_FORMAT_PLANAR) {
        write_uv_rect(prim_user_data.x, f, TEX_SIZE(sColor0), vUv_Y, vUvBounds_Y);
        write_uv_rect(prim_user_data.y, f, TEX_SIZE(sColor1), vUv_U, vUvBounds_U);
        write_uv_rect(prim_user_data.z, f, TEX_SIZE(sColor2), vUv_V, vUvBounds_V);
    } else if (vFormat == YUV_FORMAT_NV12) {
        write_uv_rect(prim_user_data.x, f, TEX_SIZE(sColor0), vUv_Y, vUvBounds_Y);
        write_uv_rect(prim_user_data.y, f, TEX_SIZE(sColor1), vUv_U, vUvBounds_U);
    } else if (vFormat == YUV_FORMAT_INTERLEAVED) {
        write_uv_rect(prim_user_data.x, f, TEX_SIZE(sColor0), vUv_Y, vUvBounds_Y);
    }
}
#endif

#ifdef WR_FRAGMENT_SHADER

Fragment brush_fs() {
    vec3 yuv_value;

    if (vFormat == YUV_FORMAT_PLANAR) {
        // The yuv_planar format should have this third texture coordinate.
        vec2 uv_y = clamp(vUv_Y.xy, vUvBounds_Y.xy, vUvBounds_Y.zw);
        vec2 uv_u = clamp(vUv_U.xy, vUvBounds_U.xy, vUvBounds_U.zw);
        vec2 uv_v = clamp(vUv_V.xy, vUvBounds_V.xy, vUvBounds_V.zw);
        yuv_value.x = TEX_SAMPLE(sColor0, vec3(uv_y, vUv_Y.z)).r;
        yuv_value.y = TEX_SAMPLE(sColor1, vec3(uv_u, vUv_U.z)).r;
        yuv_value.z = TEX_SAMPLE(sColor2, vec3(uv_v, vUv_V.z)).r;
    } else if (vFormat == YUV_FORMAT_NV12) {
        vec2 uv_y = clamp(vUv_Y.xy, vUvBounds_Y.xy, vUvBounds_Y.zw);
        vec2 uv_uv = clamp(vUv_U.xy, vUvBounds_U.xy, vUvBounds_U.zw);
        yuv_value.x = TEX_SAMPLE(sColor0, vec3(uv_y, vUv_Y.z)).r;
        yuv_value.yz = TEX_SAMPLE(sColor1, vec3(uv_uv, vUv_U.z)).rg;
    } else if (vFormat == YUV_FORMAT_INTERLEAVED) {
        // "The Y, Cb and Cr color channels within the 422 data are mapped into
        // the existing green, blue and red color channels."
        // https://www.khronos.org/registry/OpenGL/extensions/APPLE/APPLE_rgb_422.txt
        vec2 uv_y = clamp(vUv_Y.xy, vUvBounds_Y.xy, vUvBounds_Y.zw);
        yuv_value = TEX_SAMPLE(sColor0, vec3(uv_y, vUv_Y.z)).gbr;
    } else {
        yuv_value = vec3(0.0);
    }

    // See the YuvColorMatrix definition for an explanation of where the constants come from.
    vec3 rgb = vYuvColorMatrix * (yuv_value * vCoefficient - vec3(0.06275, 0.50196, 0.50196));
    vec4 color = vec4(rgb, 1.0);

#ifdef WR_FEATURE_ALPHA_PASS
    color *= init_transform_fs(vLocalPos);
#endif

    return Fragment(color);
}
#endif
