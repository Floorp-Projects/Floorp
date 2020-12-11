/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Composite a picture cache tile into the framebuffer.

#include shared,yuv

#ifdef WR_FEATURE_YUV
flat varying mat3 vYuvColorMatrix;
flat varying vec3 vYuvOffsetVector;
flat varying float vYuvCoefficient;
flat varying int vYuvFormat;
flat varying vec3 vYuvLayers;
#ifdef SWGL
flat varying int vYuvColorSpace;
flat varying int vRescaleFactor;
#endif
varying vec2 vUV_y;
varying vec2 vUV_u;
varying vec2 vUV_v;
flat varying vec4 vUVBounds_y;
flat varying vec4 vUVBounds_u;
flat varying vec4 vUVBounds_v;
#else
flat varying vec4 vColor;
flat varying float vLayer;
varying vec2 vUv;
flat varying vec4 vUVBounds;
#endif

#ifdef WR_VERTEX_SHADER
// CPU side data is in CompositeInstance (gpu_types.rs) and is
// converted to GPU data using desc::COMPOSITE (renderer.rs) by
// filling vaos.composite_vao with VertexArrayKind::Composite.
PER_INSTANCE in vec4 aDeviceRect;
PER_INSTANCE in vec4 aDeviceClipRect;
PER_INSTANCE in vec4 aColor;
PER_INSTANCE in vec4 aParams;
PER_INSTANCE in vec3 aTextureLayers;

#ifdef WR_FEATURE_YUV
// YUV treats these as a UV clip rect (clamp)
PER_INSTANCE in vec4 aUvRect0;
PER_INSTANCE in vec4 aUvRect1;
PER_INSTANCE in vec4 aUvRect2;
#else
PER_INSTANCE in vec4 aUvRect0;
#endif

void main(void) {
	// Get world position
    vec2 world_pos = aDeviceRect.xy + aPosition.xy * aDeviceRect.zw;

    // Clip the position to the world space clip rect
    vec2 clipped_world_pos = clamp(world_pos, aDeviceClipRect.xy, aDeviceClipRect.xy + aDeviceClipRect.zw);

    // Derive the normalized UV from the clipped vertex position
    vec2 uv = (clipped_world_pos - aDeviceRect.xy) / aDeviceRect.zw;

#ifdef WR_FEATURE_YUV
    int yuv_color_space = int(aParams.y);
    int yuv_format = int(aParams.z);
    float yuv_coefficient = aParams.w;

    vYuvColorMatrix = get_yuv_color_matrix(yuv_color_space);
    vYuvOffsetVector = get_yuv_offset_vector(yuv_color_space);
    vYuvCoefficient = yuv_coefficient;
    vYuvFormat = yuv_format;
    vYuvLayers = aTextureLayers.xyz;

#ifdef SWGL
    // swgl_commitTextureLinearYUV needs to know the color space specifier and
    // also needs to know how many bits of scaling are required to normalize
    // HDR textures.
    vYuvColorSpace = yuv_color_space;
    vRescaleFactor = int(log2(yuv_coefficient));
#endif

    write_uv_rect(
        aUvRect0.xy,
        aUvRect0.zw,
        uv,
        TEX_SIZE(sColor0),
        vUV_y,
        vUVBounds_y
    );
    write_uv_rect(
        aUvRect1.xy,
        aUvRect1.zw,
        uv,
        TEX_SIZE(sColor1),
        vUV_u,
        vUVBounds_u
    );
    write_uv_rect(
        aUvRect2.xy,
        aUvRect2.zw,
        uv,
        TEX_SIZE(sColor2),
        vUV_v,
        vUVBounds_v
    );
#else
    vUv = mix(aUvRect0.xy, aUvRect0.zw, uv);
    // flip_y might have the UV rect "upside down", make sure
    // clamp works correctly:
    vUVBounds = vec4(aUvRect0.x, min(aUvRect0.y, aUvRect0.w),
                     aUvRect0.z, max(aUvRect0.y, aUvRect0.w));
    int rescale_uv = int(aParams.y);
    if (rescale_uv == 1)
    {
        // using an atlas, so UVs are in pixels, and need to be
        // normalized and clamped.
        vec2 texture_size = TEX_SIZE(sColor0);
        vUVBounds += vec4(0.5, 0.5, -0.5, -0.5);
    #ifndef WR_FEATURE_TEXTURE_RECT
        vUv /= texture_size;
        vUVBounds /= texture_size.xyxy;
    #endif
    }
    // Pass through color and texture array layer
    vColor = aColor;
    vLayer = aTextureLayers.x;
#endif

    gl_Position = uTransform * vec4(clipped_world_pos, aParams.x /* z_id */, 1.0);
}
#endif

#ifdef WR_FRAGMENT_SHADER
void main(void) {
#ifdef WR_FEATURE_YUV
    vec4 color = sample_yuv(
        vYuvFormat,
        vYuvColorMatrix,
        vYuvOffsetVector,
        vYuvCoefficient,
        vYuvLayers,
        vUV_y,
        vUV_u,
        vUV_v,
        vUVBounds_y,
        vUVBounds_u,
        vUVBounds_v
    );
#else
    // The color is just the texture sample modulated by a supplied color
    vec2 uv = clamp(vUv.xy, vUVBounds.xy, vUVBounds.zw);
#   if defined(WR_FEATURE_TEXTURE_EXTERNAL) || defined(WR_FEATURE_TEXTURE_2D) || defined(WR_FEATURE_TEXTURE_RECT)
    vec4 texel = TEX_SAMPLE(sColor0, vec3(uv, vLayer));
#   else
    vec4 texel = textureLod(sColor0, vec3(uv, vLayer), 0.0);
#   endif
    vec4 color = vColor * texel;
#endif
    write_output(color);
}

#ifdef SWGL
void swgl_drawSpanRGBA8() {
#ifdef WR_FEATURE_YUV
    if (vYuvFormat == YUV_FORMAT_PLANAR) {
        if (!swgl_isTextureLinear(sColor0) || !swgl_isTextureLinear(sColor1) || !swgl_isTextureLinear(sColor2)) {
            return;
        }

        int layer0 = swgl_textureLayerOffset(sColor0, vYuvLayers.x);
        vec2 uv0 = swgl_linearQuantize(sColor0, vUV_y);
        vec2 min_uv0 = swgl_linearQuantize(sColor0, vUVBounds_y.xy);
        vec2 max_uv0 = swgl_linearQuantize(sColor0, vUVBounds_y.zw);
        vec2 step_uv0 = swgl_linearQuantizeStep(sColor0, swgl_interpStep(vUV_y));

        int layer1 = swgl_textureLayerOffset(sColor1, vYuvLayers.y);
        vec2 uv1 = swgl_linearQuantize(sColor1, vUV_u);
        vec2 min_uv1 = swgl_linearQuantize(sColor1, vUVBounds_u.xy);
        vec2 max_uv1 = swgl_linearQuantize(sColor1, vUVBounds_u.zw);
        vec2 step_uv1 = swgl_linearQuantizeStep(sColor1, swgl_interpStep(vUV_u));

        int layer2 = swgl_textureLayerOffset(sColor2, vYuvLayers.z);
        vec2 uv2 = swgl_linearQuantize(sColor2, vUV_v);
        vec2 min_uv2 = swgl_linearQuantize(sColor2, vUVBounds_v.xy);
        vec2 max_uv2 = swgl_linearQuantize(sColor2, vUVBounds_v.zw);
        vec2 step_uv2 = swgl_linearQuantizeStep(sColor2, swgl_interpStep(vUV_v));

        while (swgl_SpanLength > 0) {
            swgl_commitTextureLinearYUV(sColor0, clamp(uv0, min_uv0, max_uv0), layer0,
                                        sColor1, clamp(uv1, min_uv1, max_uv1), layer1,
                                        sColor2, clamp(uv2, min_uv2, max_uv2), layer2,
                                        vYuvColorSpace, vRescaleFactor);
            uv0 += step_uv0;
            uv1 += step_uv1;
            uv2 += step_uv2;
        }
    } else if (vYuvFormat == YUV_FORMAT_NV12) {
        if (!swgl_isTextureLinear(sColor0) || !swgl_isTextureLinear(sColor1)) {
            return;
        }

        int layer0 = swgl_textureLayerOffset(sColor0, vYuvLayers.x);
        vec2 uv0 = swgl_linearQuantize(sColor0, vUV_y);
        vec2 min_uv0 = swgl_linearQuantize(sColor0, vUVBounds_y.xy);
        vec2 max_uv0 = swgl_linearQuantize(sColor0, vUVBounds_y.zw);
        vec2 step_uv0 = swgl_linearQuantizeStep(sColor0, swgl_interpStep(vUV_y));

        int layer1 = swgl_textureLayerOffset(sColor1, vYuvLayers.y);
        vec2 uv1 = swgl_linearQuantize(sColor1, vUV_u);
        vec2 min_uv1 = swgl_linearQuantize(sColor1, vUVBounds_u.xy);
        vec2 max_uv1 = swgl_linearQuantize(sColor1, vUVBounds_u.zw);
        vec2 step_uv1 = swgl_linearQuantizeStep(sColor1, swgl_interpStep(vUV_u));

        while (swgl_SpanLength > 0) {
            swgl_commitTextureLinearYUV(sColor0, clamp(uv0, min_uv0, max_uv0), layer0,
                                        sColor1, clamp(uv1, min_uv1, max_uv1), layer1,
                                        vYuvColorSpace, vRescaleFactor);
            uv0 += step_uv0;
            uv1 += step_uv1;
        }
    } else if (vYuvFormat == YUV_FORMAT_INTERLEAVED) {
        if (!swgl_isTextureLinear(sColor0) || !swgl_isTextureLinear(sColor1)) {
            return;
        }

        int layer0 = swgl_textureLayerOffset(sColor0, vYuvLayers.x);
        vec2 uv0 = swgl_linearQuantize(sColor0, vUV_y);
        vec2 min_uv0 = swgl_linearQuantize(sColor0, vUVBounds_y.xy);
        vec2 max_uv0 = swgl_linearQuantize(sColor0, vUVBounds_y.zw);
        vec2 step_uv0 = swgl_linearQuantizeStep(sColor0, swgl_interpStep(vUV_y));

        while (swgl_SpanLength > 0) {
            swgl_commitTextureLinearYUV(sColor0, clamp(uv0, min_uv0, max_uv0), layer0,
                                        vYuvColorSpace, vRescaleFactor);
            uv0 += step_uv0;
        }
    }
#else
    if (!swgl_isTextureRGBA8(sColor0) || !swgl_isTextureLinear(sColor0)) {
        return;
    }

    int layer = swgl_textureLayerOffset(sColor0, vLayer);

    vec2 uv = swgl_linearQuantize(sColor0, vUv);
    vec2 min_uv = swgl_linearQuantize(sColor0, vUVBounds.xy);
    vec2 max_uv = swgl_linearQuantize(sColor0, vUVBounds.zw);
    vec2 step_uv = swgl_linearQuantizeStep(sColor0, swgl_interpStep(vUv));

    if (vColor != vec4(1.0)) {
        while (swgl_SpanLength > 0) {
            swgl_commitTextureLinearColorRGBA8(sColor0, clamp(uv, min_uv, max_uv), vColor, layer);
            uv += step_uv;
        }
    } else {
        while (swgl_SpanLength > 0) {
            swgl_commitTextureLinearRGBA8(sColor0, clamp(uv, min_uv, max_uv), layer);
            uv += step_uv;
        }
    }
#endif
}
#endif

#endif
