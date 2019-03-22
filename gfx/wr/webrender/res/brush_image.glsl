/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define VECS_PER_SPECIFIC_BRUSH 3

#include shared,prim_shared,brush

#ifdef WR_FEATURE_ALPHA_PASS
varying vec2 vLocalPos;
#endif

// Interpolated UV coordinates to sample.
varying vec2 vUv;
// X = layer index to sample, Y = flag to allow perspective interpolation of UV.
flat varying vec2 vLayerAndPerspective;
// Normalized bounds of the source image in the texture.
flat varying vec4 vUvBounds;
// Normalized bounds of the source image in the texture, adjusted to avoid
// sampling artifacts.
flat varying vec4 vUvSampleBounds;

#ifdef WR_FEATURE_ALPHA_PASS
flat varying vec4 vColor;
flat varying vec2 vMaskSwizzle;
flat varying vec2 vTileRepeat;
#endif

#ifdef WR_VERTEX_SHADER

// Must match the AlphaType enum.
#define BLEND_MODE_ALPHA            0
#define BLEND_MODE_PREMUL_ALPHA     1

struct ImageBrushData {
    vec4 color;
    vec4 background_color;
    vec2 stretch_size;
};

ImageBrushData fetch_image_data(int address) {
    vec4[3] raw_data = fetch_from_gpu_cache_3(address);
    ImageBrushData data = ImageBrushData(
        raw_data[0],
        raw_data[1],
        raw_data[2].xy
    );
    return data;
}

void brush_vs(
    VertexInfo vi,
    int prim_address,
    RectWithSize prim_rect,
    RectWithSize segment_rect,
    ivec4 prim_user_data,
    int segment_user_data,
    mat4 transform,
    PictureTask pic_task,
    int brush_flags,
    vec4 segment_data
) {
    ImageBrushData image_data = fetch_image_data(prim_address);

    // If this is in WR_FEATURE_TEXTURE_RECT mode, the rect and size use
    // non-normalized texture coordinates.
#ifdef WR_FEATURE_TEXTURE_RECT
    vec2 texture_size = vec2(1, 1);
#else
    vec2 texture_size = vec2(textureSize(sColor0, 0));
#endif

    ImageResource res = fetch_image_resource(segment_user_data);
    vec2 uv0 = res.uv_rect.p0;
    vec2 uv1 = res.uv_rect.p1;

    RectWithSize local_rect = prim_rect;
    vec2 stretch_size = image_data.stretch_size;
    if (stretch_size.x < 0.0) {
        stretch_size = local_rect.size;
    }

    // If this segment should interpolate relative to the
    // segment, modify the parameters for that.
    if ((brush_flags & BRUSH_FLAG_SEGMENT_RELATIVE) != 0) {
        local_rect = segment_rect;
        stretch_size = local_rect.size;

        if ((brush_flags & BRUSH_FLAG_SEGMENT_REPEAT_X) != 0) {
            stretch_size.x = (segment_data.z - segment_data.x);
        }
        if ((brush_flags & BRUSH_FLAG_SEGMENT_REPEAT_Y) != 0) {
            stretch_size.y = (segment_data.w - segment_data.y);
        }

        // If the extra data is a texel rect, modify the UVs.
        if ((brush_flags & BRUSH_FLAG_TEXEL_RECT) != 0) {
            vec2 uv_size = res.uv_rect.p1 - res.uv_rect.p0;
            uv0 = res.uv_rect.p0 + segment_data.xy * uv_size;
            uv1 = res.uv_rect.p0 + segment_data.zw * uv_size;
            if ((brush_flags & BRUSH_FLAG_SEGMENT_REPEAT_X) != 0) {
              stretch_size.x = stretch_size.x * uv_size.x;
            }
            if ((brush_flags & BRUSH_FLAG_SEGMENT_REPEAT_Y) != 0) {
              stretch_size.y = stretch_size.y * uv_size.y;
            }
        }
    }

    float perspective_interpolate = (brush_flags & BRUSH_FLAG_PERSPECTIVE_INTERPOLATION) != 0 ? 1.0 : 0.0;
    vLayerAndPerspective = vec2(res.layer, perspective_interpolate);

    // Handle case where the UV coords are inverted (e.g. from an
    // external image).
    vec2 min_uv = min(uv0, uv1);
    vec2 max_uv = max(uv0, uv1);

    vUvSampleBounds = vec4(
        min_uv + vec2(0.5),
        max_uv - vec2(0.5)
    ) / texture_size.xyxy;

    vec2 f = (vi.local_pos - local_rect.p0) / local_rect.size;

#ifdef WR_FEATURE_ALPHA_PASS
    int color_mode = prim_user_data.x & 0xffff;
    int blend_mode = prim_user_data.x >> 16;
    int raster_space = prim_user_data.y;

    if (color_mode == COLOR_MODE_FROM_PASS) {
        color_mode = uMode;
    }

    // Derive the texture coordinates for this image, based on
    // whether the source image is a local-space or screen-space
    // image.
    switch (raster_space) {
        case RASTER_SCREEN: {
            // Since the screen space UVs specify an arbitrary quad, do
            // a bilinear interpolation to get the correct UV for this
            // local position.
            f = get_image_quad_uv(segment_user_data, f);
            break;
        }
        default:
            break;
    }
#endif

    // Offset and scale vUv here to avoid doing it in the fragment shader.
    vec2 repeat = local_rect.size / stretch_size;
    vUv = mix(uv0, uv1, f) - min_uv;
    vUv /= texture_size;
    vUv *= repeat.xy;
    if (perspective_interpolate == 0.0) {
        vUv *= vi.world_pos.w;
    }

#ifdef WR_FEATURE_TEXTURE_RECT
    vUvBounds = vec4(0.0, 0.0, vec2(textureSize(sColor0)));
#else
    vUvBounds = vec4(min_uv, max_uv) / texture_size.xyxy;
#endif

#ifdef WR_FEATURE_ALPHA_PASS
    vTileRepeat = repeat.xy;

    float opacity = float(prim_user_data.z) / 65535.0;
    switch (blend_mode) {
        case BLEND_MODE_ALPHA:
            image_data.color.a *= opacity;
            break;
        case BLEND_MODE_PREMUL_ALPHA:
        default:
            image_data.color *= opacity;
            break;
    }

    switch (color_mode) {
        case COLOR_MODE_ALPHA:
        case COLOR_MODE_BITMAP:
            vMaskSwizzle = vec2(0.0, 1.0);
            vColor = image_data.color;
            break;
        case COLOR_MODE_SUBPX_BG_PASS2:
        case COLOR_MODE_SUBPX_DUAL_SOURCE:
        case COLOR_MODE_IMAGE:
            vMaskSwizzle = vec2(1.0, 0.0);
            vColor = image_data.color;
            break;
        case COLOR_MODE_SUBPX_CONST_COLOR:
        case COLOR_MODE_SUBPX_BG_PASS0:
        case COLOR_MODE_COLOR_BITMAP:
            vMaskSwizzle = vec2(1.0, 0.0);
            vColor = vec4(image_data.color.a);
            break;
        case COLOR_MODE_SUBPX_BG_PASS1:
            vMaskSwizzle = vec2(-1.0, 1.0);
            vColor = vec4(image_data.color.a) * image_data.background_color;
            break;
        default:
            vMaskSwizzle = vec2(0.0);
            vColor = vec4(1.0);
    }

    vLocalPos = vi.local_pos;
#endif
}
#endif

#ifdef WR_FRAGMENT_SHADER

Fragment brush_fs() {
    vec2 uv_size = vUvBounds.zw - vUvBounds.xy;
    float perspective_divisor = mix(gl_FragCoord.w, 1.0, vLayerAndPerspective.y);

#ifdef WR_FEATURE_ALPHA_PASS
    // This prevents the uv on the top and left parts of the primitive that was inflated
    // for anti-aliasing purposes from going beyound the range covered by the regular
    // (non-inflated) primitive.
    vec2 local_uv = max(vUv * perspective_divisor, vec2(0.0));

    // Handle horizontal and vertical repetitions.
    vec2 repeated_uv = mod(local_uv, uv_size) + vUvBounds.xy;

    // This takes care of the bottom and right inflated parts.
    // We do it after the modulo because the latter wraps around the values exactly on
    // the right and bottom edges, which we do not want.
    if (local_uv.x >= vTileRepeat.x * uv_size.x) {
        repeated_uv.x = vUvBounds.z;
    }
    if (local_uv.y >= vTileRepeat.y * uv_size.y) {
        repeated_uv.y = vUvBounds.w;
    }
#else
    // Handle horizontal and vertical repetitions.
    vec2 repeated_uv = mod(vUv * perspective_divisor, uv_size) + vUvBounds.xy;
#endif

    // Clamp the uvs to avoid sampling artifacts.
    vec2 uv = clamp(repeated_uv, vUvSampleBounds.xy, vUvSampleBounds.zw);

    vec4 texel = TEX_SAMPLE(sColor0, vec3(uv, vLayerAndPerspective.x));

    Fragment frag;

#ifdef WR_FEATURE_ALPHA_PASS
    float alpha = init_transform_fs(vLocalPos);
    texel.rgb = texel.rgb * vMaskSwizzle.x + texel.aaa * vMaskSwizzle.y;

    vec4 alpha_mask = texel * alpha;
    frag.color = vColor * alpha_mask;

    #ifdef WR_FEATURE_DUAL_SOURCE_BLENDING
        frag.blend = alpha_mask * vColor.a;
    #endif
#else
    frag.color = texel;
#endif

    return frag;
}
#endif
