/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define VECS_PER_SPECIFIC_BRUSH 1

#include shared,prim_shared,brush,yuv

varying highp vec2 vUv_Y;
flat varying highp vec4 vUvBounds_Y;

varying highp vec2 vUv_U;
flat varying highp vec4 vUvBounds_U;

varying highp vec2 vUv_V;
flat varying highp vec4 vUvBounds_V;

flat varying YUV_PRECISION vec3 vYcbcrBias;
flat varying YUV_PRECISION mat3 vRgbFromDebiasedYcbcr;

// YUV format. Packed in to vector to work around bug 1630356.
flat varying mediump ivec2 vFormat;

#ifdef SWGL_DRAW_SPAN
flat varying mediump int vRescaleFactor;
#endif

#ifdef WR_VERTEX_SHADER

YuvPrimitive fetch_yuv_primitive(int address) {
    vec4 data = fetch_from_gpu_cache_1(address);
    // From YuvImageData.write_prim_gpu_blocks:
    int channel_bit_depth = int(data.x);
    int color_space = int(data.y);
    int yuv_format = int(data.z);
    return YuvPrimitive(channel_bit_depth, color_space, yuv_format);
}

void brush_vs(
    VertexInfo vi,
    int prim_address,
    RectWithEndpoint local_rect,
    RectWithEndpoint segment_rect,
    ivec4 prim_user_data,
    int specific_resource_address,
    mat4 transform,
    PictureTask pic_task,
    int brush_flags,
    vec4 unused
) {
    vec2 f = (vi.local_pos - local_rect.p0) / rect_size(local_rect);

    YuvPrimitive prim = fetch_yuv_primitive(prim_address);

#ifdef SWGL_DRAW_SPAN
    // swgl_commitTextureLinearYUV needs to know the color space specifier and
    // also needs to know how many bits of scaling are required to normalize
    // HDR textures. Note that MSB HDR formats don't need renormalization.
    vRescaleFactor = 0;
    if (prim.channel_bit_depth > 8 && prim.yuv_format != YUV_FORMAT_P010) {
        vRescaleFactor = 16 - prim.channel_bit_depth;
    }
#endif

    YuvColorMatrixInfo mat_info = get_rgb_from_ycbcr_info(prim);
    vYcbcrBias = mat_info.ycbcr_bias;
    vRgbFromDebiasedYcbcr = mat_info.rgb_from_debiased_ycbrc;

    vFormat.x = prim.yuv_format;

    // The additional test for 99 works around a gen6 shader compiler bug: 1708937
    if (vFormat.x == YUV_FORMAT_PLANAR || vFormat.x == 99) {
        ImageSource res_y = fetch_image_source(prim_user_data.x);
        ImageSource res_u = fetch_image_source(prim_user_data.y);
        ImageSource res_v = fetch_image_source(prim_user_data.z);
        write_uv_rect(res_y.uv_rect.p0, res_y.uv_rect.p1, f, TEX_SIZE_YUV(sColor0), vUv_Y, vUvBounds_Y);
        write_uv_rect(res_u.uv_rect.p0, res_u.uv_rect.p1, f, TEX_SIZE_YUV(sColor1), vUv_U, vUvBounds_U);
        write_uv_rect(res_v.uv_rect.p0, res_v.uv_rect.p1, f, TEX_SIZE_YUV(sColor2), vUv_V, vUvBounds_V);
    } else if (vFormat.x == YUV_FORMAT_NV12 || vFormat.x == YUV_FORMAT_P010) {
        ImageSource res_y = fetch_image_source(prim_user_data.x);
        ImageSource res_u = fetch_image_source(prim_user_data.y);
        write_uv_rect(res_y.uv_rect.p0, res_y.uv_rect.p1, f, TEX_SIZE_YUV(sColor0), vUv_Y, vUvBounds_Y);
        write_uv_rect(res_u.uv_rect.p0, res_u.uv_rect.p1, f, TEX_SIZE_YUV(sColor1), vUv_U, vUvBounds_U);
    } else if (vFormat.x == YUV_FORMAT_INTERLEAVED) {
        ImageSource res_y = fetch_image_source(prim_user_data.x);
        write_uv_rect(res_y.uv_rect.p0, res_y.uv_rect.p1, f, TEX_SIZE_YUV(sColor0), vUv_Y, vUvBounds_Y);
    }
}
#endif

#ifdef WR_FRAGMENT_SHADER

Fragment brush_fs() {
    vec4 color = sample_yuv(
        vFormat.x,
        vYcbcrBias,
        vRgbFromDebiasedYcbcr,
        vUv_Y,
        vUv_U,
        vUv_V,
        vUvBounds_Y,
        vUvBounds_U,
        vUvBounds_V
    );

#ifdef WR_FEATURE_ALPHA_PASS
    color *= antialias_brush();
#endif

    //color.r = float(100+vFormat) / 255.0;
    //color.g = vYcbcrBias.x;
    //color.b = vYcbcrBias.y;
    return Fragment(color);
}

#ifdef SWGL_DRAW_SPAN
void swgl_drawSpanRGBA8() {
    if (vFormat.x == YUV_FORMAT_PLANAR) {
        swgl_commitTextureLinearYUV(sColor0, vUv_Y, vUvBounds_Y,
                                    sColor1, vUv_U, vUvBounds_U,
                                    sColor2, vUv_V, vUvBounds_V,
                                    vYcbcrBias,
                                    vRgbFromDebiasedYcbcr,
                                    vRescaleFactor);
    } else if (vFormat.x == YUV_FORMAT_NV12 || vFormat.x == YUV_FORMAT_P010) {
        swgl_commitTextureLinearYUV(sColor0, vUv_Y, vUvBounds_Y,
                                    sColor1, vUv_U, vUvBounds_U,
                                    vYcbcrBias,
                                    vRgbFromDebiasedYcbcr,
                                    vRescaleFactor);
    } else if (vFormat.x == YUV_FORMAT_INTERLEAVED) {
        swgl_commitTextureLinearYUV(sColor0, vUv_Y, vUvBounds_Y,
                                    vYcbcrBias,
                                    vRgbFromDebiasedYcbcr,
                                    vRescaleFactor);
    }
}
#endif

#endif
