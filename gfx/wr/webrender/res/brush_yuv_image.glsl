/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define VECS_PER_SPECIFIC_BRUSH 1

#include shared,prim_shared,brush,yuv

#ifdef WR_FEATURE_ALPHA_PASS
varying vec2 vLocalPos;
#endif

flat varying vec3 vYuvLayers;

varying vec2 vUv_Y;
flat varying vec4 vUvBounds_Y;

varying vec2 vUv_U;
flat varying vec4 vUvBounds_U;

varying vec2 vUv_V;
flat varying vec4 vUvBounds_V;

flat varying float vCoefficient;
flat varying mat3 vYuvColorMatrix;
flat varying vec3 vYuvOffsetVector;
flat varying int vFormat;

#ifdef SWGL
flat varying int vYuvColorSpace;
flat varying int vRescaleFactor;
#endif

#ifdef WR_VERTEX_SHADER

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
    vYuvOffsetVector = get_yuv_offset_vector(prim.color_space);
    vFormat = prim.yuv_format;

#ifdef SWGL
    // swgl_commitTextureLinearYUV needs to know the color space specifier and
    // also needs to know how many bits of scaling are required to normalize
    // HDR textures.
    vYuvColorSpace = prim.color_space;
    vRescaleFactor = int(log2(prim.coefficient));
#endif

#ifdef WR_FEATURE_ALPHA_PASS
    vLocalPos = vi.local_pos;
#endif

    if (vFormat == YUV_FORMAT_PLANAR) {
        ImageResource res_y = fetch_image_resource(prim_user_data.x);
        ImageResource res_u = fetch_image_resource(prim_user_data.y);
        ImageResource res_v = fetch_image_resource(prim_user_data.z);
        write_uv_rect(res_y.uv_rect.p0, res_y.uv_rect.p1, f, TEX_SIZE(sColor0), vUv_Y, vUvBounds_Y);
        write_uv_rect(res_u.uv_rect.p0, res_u.uv_rect.p1, f, TEX_SIZE(sColor1), vUv_U, vUvBounds_U);
        write_uv_rect(res_v.uv_rect.p0, res_v.uv_rect.p1, f, TEX_SIZE(sColor2), vUv_V, vUvBounds_V);
        vYuvLayers = vec3(res_y.layer, res_u.layer, res_v.layer);
    } else if (vFormat == YUV_FORMAT_NV12) {
        ImageResource res_y = fetch_image_resource(prim_user_data.x);
        ImageResource res_u = fetch_image_resource(prim_user_data.y);
        write_uv_rect(res_y.uv_rect.p0, res_y.uv_rect.p1,  f, TEX_SIZE(sColor0), vUv_Y, vUvBounds_Y);
        write_uv_rect(res_u.uv_rect.p0, res_u.uv_rect.p1, f, TEX_SIZE(sColor1), vUv_U, vUvBounds_U);
        vYuvLayers = vec3(res_y.layer, res_u.layer, 0.0);
    } else if (vFormat == YUV_FORMAT_INTERLEAVED) {
        ImageResource res_y = fetch_image_resource(prim_user_data.x);
        write_uv_rect(res_y.uv_rect.p0, res_y.uv_rect.p1, f, TEX_SIZE(sColor0), vUv_Y, vUvBounds_Y);
        vYuvLayers = vec3(res_y.layer, 0.0, 0.0);
    }
}
#endif

#ifdef WR_FRAGMENT_SHADER

Fragment brush_fs() {
    vec4 color = sample_yuv(
        vFormat,
        vYuvColorMatrix,
        vYuvOffsetVector,
        vCoefficient,
        vYuvLayers,
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

#ifdef SWGL
void swgl_drawSpanRGBA8() {
    if (vFormat == YUV_FORMAT_PLANAR) {
        if (!swgl_isTextureLinear(sColor0) || !swgl_isTextureLinear(sColor1) || !swgl_isTextureLinear(sColor2)) {
            return;
        }

        int layer0 = swgl_textureLayerOffset(sColor0, vYuvLayers.x);
        vec2 uv0 = swgl_linearQuantize(sColor0, vUv_Y);
        vec2 min_uv0 = swgl_linearQuantize(sColor0, vUvBounds_Y.xy);
        vec2 max_uv0 = swgl_linearQuantize(sColor0, vUvBounds_Y.zw);
        vec2 step_uv0 = swgl_linearQuantizeStep(sColor0, swgl_interpStep(vUv_Y));

        int layer1 = swgl_textureLayerOffset(sColor1, vYuvLayers.y);
        vec2 uv1 = swgl_linearQuantize(sColor1, vUv_U);
        vec2 min_uv1 = swgl_linearQuantize(sColor1, vUvBounds_U.xy);
        vec2 max_uv1 = swgl_linearQuantize(sColor1, vUvBounds_U.zw);
        vec2 step_uv1 = swgl_linearQuantizeStep(sColor1, swgl_interpStep(vUv_U));

        int layer2 = swgl_textureLayerOffset(sColor2, vYuvLayers.z);
        vec2 uv2 = swgl_linearQuantize(sColor2, vUv_V);
        vec2 min_uv2 = swgl_linearQuantize(sColor2, vUvBounds_V.xy);
        vec2 max_uv2 = swgl_linearQuantize(sColor2, vUvBounds_V.zw);
        vec2 step_uv2 = swgl_linearQuantizeStep(sColor2, swgl_interpStep(vUv_V));

        #ifdef WR_FEATURE_ALPHA_PASS
        if (has_valid_transform_bounds()) {
            while (swgl_SpanLength > 0) {
                float alpha = init_transform_fs(vLocalPos);
                vLocalPos += swgl_interpStep(vLocalPos);
                swgl_commitTextureLinearColorYUV(sColor0, clamp(uv0, min_uv0, max_uv0), layer0,
                                                 sColor1, clamp(uv1, min_uv1, max_uv1), layer1,
                                                 sColor2, clamp(uv2, min_uv2, max_uv2), layer2,
                                                 vYuvColorSpace, vRescaleFactor, alpha);
                uv0 += step_uv0;
                uv1 += step_uv1;
                uv2 += step_uv2;
            }
            return;
        }
        #endif

        while (swgl_SpanLength > 0) {
            swgl_commitTextureLinearYUV(sColor0, clamp(uv0, min_uv0, max_uv0), layer0,
                                        sColor1, clamp(uv1, min_uv1, max_uv1), layer1,
                                        sColor2, clamp(uv2, min_uv2, max_uv2), layer2,
                                        vYuvColorSpace, vRescaleFactor);
            uv0 += step_uv0;
            uv1 += step_uv1;
            uv2 += step_uv2;
        }
    } else if (vFormat == YUV_FORMAT_NV12) {
        if (!swgl_isTextureLinear(sColor0) || !swgl_isTextureLinear(sColor1)) {
            return;
        }

        int layer0 = swgl_textureLayerOffset(sColor0, vYuvLayers.x);
        vec2 uv0 = swgl_linearQuantize(sColor0, vUv_Y);
        vec2 min_uv0 = swgl_linearQuantize(sColor0, vUvBounds_Y.xy);
        vec2 max_uv0 = swgl_linearQuantize(sColor0, vUvBounds_Y.zw);
        vec2 step_uv0 = swgl_linearQuantizeStep(sColor0, swgl_interpStep(vUv_Y));

        int layer1 = swgl_textureLayerOffset(sColor1, vYuvLayers.y);
        vec2 uv1 = swgl_linearQuantize(sColor1, vUv_U);
        vec2 min_uv1 = swgl_linearQuantize(sColor1, vUvBounds_U.xy);
        vec2 max_uv1 = swgl_linearQuantize(sColor1, vUvBounds_U.zw);
        vec2 step_uv1 = swgl_linearQuantizeStep(sColor1, swgl_interpStep(vUv_U));

        #ifdef WR_FEATURE_ALPHA_PASS
        if (has_valid_transform_bounds()) {
            while (swgl_SpanLength > 0) {
                float alpha = init_transform_fs(vLocalPos);
                vLocalPos += swgl_interpStep(vLocalPos);
                swgl_commitTextureLinearColorYUV(sColor0, clamp(uv0, min_uv0, max_uv0), layer0,
                                                 sColor1, clamp(uv1, min_uv1, max_uv1), layer1,
                                                 vYuvColorSpace, vRescaleFactor, alpha);
                uv0 += step_uv0;
                uv1 += step_uv1;
            }
            return;
        }
        #endif

        while (swgl_SpanLength > 0) {
            swgl_commitTextureLinearYUV(sColor0, clamp(uv0, min_uv0, max_uv0), layer0,
                                        sColor1, clamp(uv1, min_uv1, max_uv1), layer1,
                                        vYuvColorSpace, vRescaleFactor);
            uv0 += step_uv0;
            uv1 += step_uv1;
        }
    } else if (vFormat == YUV_FORMAT_INTERLEAVED) {
        if (!swgl_isTextureLinear(sColor0)) {
            return;
        }

        int layer0 = swgl_textureLayerOffset(sColor0, vYuvLayers.x);
        vec2 uv0 = swgl_linearQuantize(sColor0, vUv_Y);
        vec2 min_uv0 = swgl_linearQuantize(sColor0, vUvBounds_Y.xy);
        vec2 max_uv0 = swgl_linearQuantize(sColor0, vUvBounds_Y.zw);
        vec2 step_uv0 = swgl_linearQuantizeStep(sColor0, swgl_interpStep(vUv_Y));

        #ifdef WR_FEATURE_ALPHA_PASS
        if (has_valid_transform_bounds()) {
            while (swgl_SpanLength > 0) {
                float alpha = init_transform_fs(vLocalPos);
                vLocalPos += swgl_interpStep(vLocalPos);
                swgl_commitTextureLinearColorYUV(sColor0, clamp(uv0, min_uv0, max_uv0), layer0,
                                                 vYuvColorSpace, vRescaleFactor, alpha);
                uv0 += step_uv0;
            }
            return;
        }
        #endif

        while (swgl_SpanLength > 0) {
            swgl_commitTextureLinearYUV(sColor0, clamp(uv0, min_uv0, max_uv0), layer0,
                                        vYuvColorSpace, vRescaleFactor);
            uv0 += step_uv0;
        }
    }
}
#endif

#endif
