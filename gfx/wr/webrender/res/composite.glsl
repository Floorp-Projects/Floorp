/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Composite a picture cache tile into the framebuffer.

// This shader must remain compatible with ESSL 1, at least for the
// WR_FEATURE_TEXTURE_EXTERNAL_ESSL1 feature, so that it can be used to render
// video on GLES devices without GL_OES_EGL_image_external_essl3 support.
// This means we cannot use textureSize(), int inputs/outputs, etc.

#include shared

#ifdef WR_FEATURE_YUV
#include yuv
#endif

#ifdef WR_FEATURE_YUV
YUV_PRECISION flat varying vec3 vYcbcrBias;
YUV_PRECISION flat varying mat3 vRgbFromDebiasedYcbcr;
// YUV format. Packed in to vector to avoid bug 1630356.
flat varying ivec2 vYuvFormat;

#ifdef SWGL_DRAW_SPAN
flat varying int vRescaleFactor;
#endif
varying vec2 vUV_y;
varying vec2 vUV_u;
varying vec2 vUV_v;
flat varying vec4 vUVBounds_y;
flat varying vec4 vUVBounds_u;
flat varying vec4 vUVBounds_v;
#else
varying vec2 vUv;
#ifndef WR_FEATURE_FAST_PATH
flat varying vec4 vColor;
flat varying vec4 vUVBounds;
#endif
#ifdef WR_FEATURE_TEXTURE_EXTERNAL_ESSL1
uniform vec2 uTextureSize;
#endif
#endif

#ifdef WR_VERTEX_SHADER
// CPU side data is in CompositeInstance (gpu_types.rs) and is
// converted to GPU data using desc::COMPOSITE (renderer.rs) by
// filling vaos.composite_vao with VertexArrayKind::Composite.
PER_INSTANCE attribute vec4 aLocalRect;
PER_INSTANCE attribute vec4 aDeviceClipRect;
PER_INSTANCE attribute vec4 aColor;
PER_INSTANCE attribute vec4 aParams;
PER_INSTANCE attribute vec4 aTransform;

#ifdef WR_FEATURE_YUV
// YUV treats these as a UV clip rect (clamp)
PER_INSTANCE attribute vec4 aUvRect0;
PER_INSTANCE attribute vec4 aUvRect1;
PER_INSTANCE attribute vec4 aUvRect2;
#else
PER_INSTANCE attribute vec4 aUvRect0;
#endif

vec2 apply_transform(vec2 p, vec4 transform) {
    return p * transform.xy + transform.zw;
}

#ifdef WR_FEATURE_YUV
YuvPrimitive fetch_yuv_primitive() {
    // From ExternalSurfaceDependency::Yuv:
    int color_space = int(aParams.y);
    int yuv_format = int(aParams.z);
    int channel_bit_depth = int(aParams.w);
    return YuvPrimitive(channel_bit_depth, color_space, yuv_format);
}
#endif

void main(void) {
	// Get world position
    vec2 world_p0 = apply_transform(aLocalRect.xy, aTransform);
    vec2 world_p1 = apply_transform(aLocalRect.zw, aTransform);
    vec2 world_pos = mix(world_p0, world_p1, aPosition.xy);

    // Clip the position to the world space clip rect
    vec2 clipped_world_pos = clamp(world_pos, aDeviceClipRect.xy, aDeviceClipRect.zw);

    // Derive the normalized UV from the clipped vertex position
    vec2 uv = (clipped_world_pos - world_p0) / (world_p1 - world_p0);

#ifdef WR_FEATURE_YUV
    YuvPrimitive prim = fetch_yuv_primitive();

#ifdef SWGL_DRAW_SPAN
    // swgl_commitTextureLinearYUV needs to know the color space specifier and
    // also needs to know how many bits of scaling are required to normalize
    // HDR textures.
    vRescaleFactor = 0;
    if (prim.channel_bit_depth > 8) {
        vRescaleFactor = 16 - prim.channel_bit_depth;
    }
    // Since SWGL rescales filtered YUV values to 8bpc before yuv->rgb
    // conversion, don't embed a 10bpc channel multiplier into the yuv matrix.
    prim.channel_bit_depth = 8;
#endif

    YuvColorMatrixInfo mat_info = get_rgb_from_ycbcr_info(prim);
    vYcbcrBias = mat_info.ycbcr_bias;
    vRgbFromDebiasedYcbcr = mat_info.rgb_from_debiased_ycbrc;

    vYuvFormat.x = prim.yuv_format;

    write_uv_rect(
        aUvRect0.xy,
        aUvRect0.zw,
        uv,
        TEX_SIZE_YUV(sColor0),
        vUV_y,
        vUVBounds_y
    );
    write_uv_rect(
        aUvRect1.xy,
        aUvRect1.zw,
        uv,
        TEX_SIZE_YUV(sColor1),
        vUV_u,
        vUVBounds_u
    );
    write_uv_rect(
        aUvRect2.xy,
        aUvRect2.zw,
        uv,
        TEX_SIZE_YUV(sColor2),
        vUV_v,
        vUVBounds_v
    );
#else
    uv = mix(aUvRect0.xy, aUvRect0.zw, uv);
    vec4 uvBounds = aUvRect0;
    int rescale_uv = int(aParams.y);
    if (rescale_uv == 1)
    {
        // using an atlas, so UVs are in pixels, and need to be
        // normalized and clamped.
#if defined(WR_FEATURE_TEXTURE_RECT)
        vec2 texture_size = vec2(1.0, 1.0);
#elif defined(WR_FEATURE_TEXTURE_EXTERNAL_ESSL1)
        vec2 texture_size = uTextureSize;
#else
        vec2 texture_size = vec2(TEX_SIZE(sColor0));
#endif
        uvBounds += vec4(0.5, 0.5, -0.5, -0.5);
    #ifndef WR_FEATURE_TEXTURE_RECT
        uv /= texture_size;
        uvBounds /= texture_size.xyxy;
    #endif
    }

    vUv = uv;
#ifndef WR_FEATURE_FAST_PATH
    vUVBounds = uvBounds;
    // Pass through color
    vColor = aColor;
#endif
#endif

    gl_Position = uTransform * vec4(clipped_world_pos, aParams.x /* z_id */, 1.0);
}
#endif

#ifdef WR_FRAGMENT_SHADER
void main(void) {
#ifdef WR_FEATURE_YUV
    vec4 color = sample_yuv(
        vYuvFormat.x,
        vYcbcrBias,
        vRgbFromDebiasedYcbcr,
        vUV_y,
        vUV_u,
        vUV_v,
        vUVBounds_y,
        vUVBounds_u,
        vUVBounds_v
    );
#else
    // The color is just the texture sample modulated by a supplied color.
    // In the fast path we avoid clamping the UV coordinates and modulating by the color.
#ifdef WR_FEATURE_FAST_PATH
    vec2 uv = vUv;
#else
    vec2 uv = clamp(vUv, vUVBounds.xy, vUVBounds.zw);
#endif
    vec4 texel = TEX_SAMPLE(sColor0, uv);
#ifdef WR_FEATURE_FAST_PATH
    vec4 color = texel;
#else
    vec4 color = vColor * texel;
#endif
#endif
    write_output(color);
}

#ifdef SWGL_DRAW_SPAN
void swgl_drawSpanRGBA8() {
#ifdef WR_FEATURE_YUV
    if (vYuvFormat.x == YUV_FORMAT_PLANAR) {
        swgl_commitTextureLinearYUV(sColor0, vUV_y, vUVBounds_y,
                                    sColor1, vUV_u, vUVBounds_u,
                                    sColor2, vUV_v, vUVBounds_v,
                                    vYcbcrBias,
                                    vRgbFromDebiasedYcbcr,
                                    vRescaleFactor);
    } else if (vYuvFormat.x == YUV_FORMAT_NV12) {
        swgl_commitTextureLinearYUV(sColor0, vUV_y, vUVBounds_y,
                                    sColor1, vUV_u, vUVBounds_u,
                                    vYcbcrBias,
                                    vRgbFromDebiasedYcbcr,
                                    vRescaleFactor);
    } else if (vYuvFormat.x == YUV_FORMAT_INTERLEAVED) {
        swgl_commitTextureLinearYUV(sColor0, vUV_y, vUVBounds_y,
                                    vYcbcrBias,
                                    vRgbFromDebiasedYcbcr,
                                    vRescaleFactor);
    }
#else
#ifdef WR_FEATURE_FAST_PATH
    vec4 color = vec4(1.0);
#ifdef WR_FEATURE_TEXTURE_RECT
    vec4 uvBounds = vec4(vec2(0.0), vec2(textureSize(sColor0)));
#else
    vec4 uvBounds = vec4(0.0, 0.0, 1.0, 1.0);
#endif
#else
    vec4 color = vColor;
    vec4 uvBounds = vUVBounds;
#endif
    if (color != vec4(1.0)) {
        swgl_commitTextureColorRGBA8(sColor0, vUv, uvBounds, color);
    } else {
        swgl_commitTextureRGBA8(sColor0, vUv, uvBounds);
    }
#endif
}
#endif

#endif
