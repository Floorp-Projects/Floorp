/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define VECS_PER_YUV_BRUSH 1
#define VECS_PER_SPECIFIC_BRUSH VECS_PER_YUV_BRUSH

#define WR_BRUSH_VS_FUNCTION yuv_brush_vs
#define WR_BRUSH_FS_FUNCTION yuv_brush_fs

#include shared,prim_shared,brush,yuv

#ifdef WR_FEATURE_ALPHA_PASS
varying vec2 vLocalPos;
#endif

varying vec3 vUv_Y;
flat varying vec4 vUvBounds_Y;

varying vec3 vUv_U;
flat varying vec4 vUvBounds_U;

varying vec3 vUv_V;
flat varying vec4 vUvBounds_V;

flat varying float vCoefficient;
flat varying mat3 vYuvColorMatrix;
flat varying int vFormat;

#ifdef WR_VERTEX_SHADER

#ifdef WR_FEATURE_TEXTURE_RECT
    #define TEX_SIZE(sampler) vec2(1.0)
#else
    #define TEX_SIZE(sampler) vec2(textureSize(sampler, 0).xy)
#endif

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

void yuv_brush_vs(
    VertexInfo vi,
    int prim_address,
    RectWithSize local_rect,
    RectWithSize segment_rect,
    ivec4 prim_user_data,
    int specific_resource_address,
    mat4 transform,
    PictureTask pic_task,
    int brush_flags,
    vec4 unused
) {
    vec2 f = (vi.local_pos - local_rect.p0) / local_rect.size;

    YuvPrimitive prim = fetch_yuv_primitive(prim_address);
    vCoefficient = prim.coefficient;

    vYuvColorMatrix = get_yuv_color_matrix(prim.color_space);
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

Fragment yuv_brush_fs() {
    vec4 color = sample_yuv(
        vFormat,
        vYuvColorMatrix,
        vCoefficient,
        vUv_Y,
        vUv_U,
        vUv_V,
        vUvBounds_Y,
        vUvBounds_U,
        vUvBounds_V
    );

#ifdef WR_FEATURE_ALPHA_PASS
    color *= init_transform_fs(vLocalPos);
#endif

    return Fragment(color);
}
#endif
