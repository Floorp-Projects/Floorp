/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define VECS_PER_SPECIFIC_BRUSH 3

#include shared,prim_shared,brush

// Interpolated UV coordinates to sample.
varying highp vec2 v_uv;

#ifdef WR_FEATURE_ALPHA_PASS
flat varying mediump vec4 v_color;
flat varying mediump vec2 v_mask_swizzle;
flat varying mediump vec2 v_tile_repeat_bounds;
#endif

// Normalized bounds of the source image in the texture.
flat varying highp vec4 v_uv_bounds;
// Normalized bounds of the source image in the texture, adjusted to avoid
// sampling artifacts.
flat varying highp vec4 v_uv_sample_bounds;

// Flag to allow perspective interpolation of UV.
// Packed in to vector to work around bug 1630356.
flat varying mediump vec2 v_perspective;

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

vec2 modf2(vec2 x, vec2 y) {
    return x - y * floor(x/y);
}

void brush_vs(
    VertexInfo vi,
    int prim_address,
    RectWithEndpoint prim_rect,
    RectWithEndpoint segment_rect,
    ivec4 prim_user_data,
    int specific_resource_address,
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
    vec2 texture_size = vec2(TEX_SIZE(sColor0));
#endif

    ImageSource res = fetch_image_source(specific_resource_address);
    vec2 uv0 = res.uv_rect.p0;
    vec2 uv1 = res.uv_rect.p1;

    RectWithEndpoint local_rect = prim_rect;
    vec2 stretch_size = image_data.stretch_size;
    if (stretch_size.x < 0.0) {
        stretch_size = rect_size(local_rect);
    }

    // If this segment should interpolate relative to the
    // segment, modify the parameters for that.
    if ((brush_flags & BRUSH_FLAG_SEGMENT_RELATIVE) != 0) {
        local_rect = segment_rect;
        stretch_size = rect_size(local_rect);

        if ((brush_flags & BRUSH_FLAG_TEXEL_RECT) != 0) {
            // If the extra data is a texel rect, modify the UVs.
            vec2 uv_size = res.uv_rect.p1 - res.uv_rect.p0;
            uv0 = res.uv_rect.p0 + segment_data.xy * uv_size;
            uv1 = res.uv_rect.p0 + segment_data.zw * uv_size;
        }

        #ifdef WR_FEATURE_REPETITION
            // TODO(bug 1609893): Move this logic to the CPU as well as other sources of
            // branchiness in this shader.
            if ((brush_flags & BRUSH_FLAG_TEXEL_RECT) != 0) {
                // Value of the stretch size with repetition. We have to compute it for
                // both axis even if we only repeat on one axis because the value for
                // each axis depends on what the repeated value would have been for the
                // other axis.
                vec2 repeated_stretch_size = stretch_size;
                // Size of the uv rect of the segment we are considering when computing
                // the repetitions. For the fill area it is a tad more complicated as we
                // have to use the uv size of the top-middle segment to drive horizontal
                // repetitions, and the size of the left-middle segment to drive vertical
                // repetitions. So we track the reference sizes for both axis separately
                // even though in the common case (the border segments) they are the same.
                vec2 horizontal_uv_size = uv1 - uv0;
                vec2 vertical_uv_size = uv1 - uv0;
                // We use top and left sizes by default and fall back to bottom and right
                // when a size is empty.
                if ((brush_flags & BRUSH_FLAG_SEGMENT_NINEPATCH_MIDDLE) != 0) {
                    repeated_stretch_size = segment_rect.p0 - prim_rect.p0;

                    float epsilon = 0.001;

                    // Adjust the the referecne uv size to compute vertical repetitions for
                    // the fill area.
                    vertical_uv_size.x = uv0.x - res.uv_rect.p0.x;
                    if (vertical_uv_size.x < epsilon || repeated_stretch_size.x < epsilon) {
                        vertical_uv_size.x = res.uv_rect.p1.x - uv1.x;
                        repeated_stretch_size.x = prim_rect.p1.x - segment_rect.p1.x;
                    }

                    // Adjust the the referecne uv size to compute horizontal repetitions
                    // for the fill area.
                    horizontal_uv_size.y = uv0.y - res.uv_rect.p0.y;
                    if (horizontal_uv_size.y < epsilon || repeated_stretch_size.y < epsilon) {
                        horizontal_uv_size.y = res.uv_rect.p1.y - uv1.y;
                        repeated_stretch_size.y = prim_rect.p1.y - segment_rect.p1.y;
                    }
                }

                if ((brush_flags & BRUSH_FLAG_SEGMENT_REPEAT_X) != 0) {
                    float uv_ratio = horizontal_uv_size.x / horizontal_uv_size.y;
                    stretch_size.x = repeated_stretch_size.y * uv_ratio;
                }
                if ((brush_flags & BRUSH_FLAG_SEGMENT_REPEAT_Y) != 0) {
                    float uv_ratio = vertical_uv_size.y / vertical_uv_size.x;
                    stretch_size.y = repeated_stretch_size.x * uv_ratio;
                }

            } else {
                if ((brush_flags & BRUSH_FLAG_SEGMENT_REPEAT_X) != 0) {
                    stretch_size.x = segment_data.z - segment_data.x;
                }
                if ((brush_flags & BRUSH_FLAG_SEGMENT_REPEAT_Y) != 0) {
                    stretch_size.y = segment_data.w - segment_data.y;
                }
            }
            if ((brush_flags & BRUSH_FLAG_SEGMENT_REPEAT_X_ROUND) != 0) {
                float segment_rect_width = segment_rect.p1.x - segment_rect.p0.x;
                float nx = max(1.0, round(segment_rect_width / stretch_size.x));
                stretch_size.x = segment_rect_width / nx;
            }
            if ((brush_flags & BRUSH_FLAG_SEGMENT_REPEAT_Y_ROUND) != 0) {
                float segment_rect_height = segment_rect.p1.y - segment_rect.p0.y;
                float ny = max(1.0, round(segment_rect_height / stretch_size.y));
                stretch_size.y = segment_rect_height / ny;
            }
        #endif
    }

    float perspective_interpolate = (brush_flags & BRUSH_FLAG_PERSPECTIVE_INTERPOLATION) != 0 ? 1.0 : 0.0;
    v_perspective.x = perspective_interpolate;

    // Handle case where the UV coords are inverted (e.g. from an
    // external image).
    vec2 min_uv = min(uv0, uv1);
    vec2 max_uv = max(uv0, uv1);

    v_uv_sample_bounds = vec4(
        min_uv + vec2(0.5),
        max_uv - vec2(0.5)
    ) / texture_size.xyxy;

    vec2 f = (vi.local_pos - local_rect.p0) / rect_size(local_rect);

#ifdef WR_FEATURE_ALPHA_PASS
    int color_mode = prim_user_data.x & 0xffff;
    int blend_mode = prim_user_data.x >> 16;

#endif

    // Derive the texture coordinates for this image, based on
    // whether the source image is a local-space or screen-space
    // image.
    int raster_space = prim_user_data.y;
    if (raster_space == RASTER_SCREEN) {
        // Since the screen space UVs specify an arbitrary quad, do
        // a bilinear interpolation to get the correct UV for this
        // local position.
        f = get_image_quad_uv(specific_resource_address, f);
    }

    // Offset and scale v_uv here to avoid doing it in the fragment shader.
    vec2 repeat = rect_size(local_rect) / stretch_size;
    v_uv = mix(uv0, uv1, f) - min_uv;
    v_uv *= repeat.xy;

    vec2 normalized_offset = vec2(0.0);
#ifdef WR_FEATURE_REPETITION
    // In the case of border-image-repeat: repeat, we must apply an offset so that
    // the first tile is centered.
    //
    // This is derived from:
    //   uv_size = max_uv - min_uv
    //   repeat = local_rect.size / stetch_size
    //   layout_offset = local_rect.size / 2 - strecth_size / 2
    //   texel_offset = layout_offset * uv_size / stretch_size
    //   texel_offset = uv_size / 2 * (local_rect.size / stretch_size - stretch_size / stretch_size)
    //   texel_offset = uv_size / 2 * (repeat - 1)
    //
    // The offset is then adjusted so that it loops in the [0, uv_size] range.
    // In principle this is simply a modulo:
    //
    //   adjusted_offset = fact((repeat - 1)/2) * uv_size
    //
    // However we don't want fract's behavior with negative numbers which happens when the pattern
    // is larger than the local rect (repeat is between 0 and 1), so we shift the content by 1 to
    // remain positive.
    //
    //   adjusted_offset = fract(repeat/2 - 1/2 + 1) * uv_size
    //
    // `uv - offset` will go through another modulo in the fragment shader for which we again don't
    // want the behavior for nagative numbers. We rearrange this here in the form
    // `uv + (uv_size - offset)` to prevent that.
    //
    //   adjusted_offset = (1 - fract(repeat/2 - 1/2 + 1)) * uv_size
    //
    // We then separate the normalized part of the offset which we also need elsewhere.
    bvec2 centered = bvec2(brush_flags & BRUSH_FLAG_SEGMENT_REPEAT_X_CENTERED,
                           brush_flags & BRUSH_FLAG_SEGMENT_REPEAT_Y_CENTERED);
    // Use mix() rather than if statements due to a miscompilation on Adreno 3xx. See bug 1853573.
    normalized_offset = mix(vec2(0.0), 1.0 - fract(repeat * 0.5 + 0.5), centered);
    v_uv += normalized_offset * (max_uv - min_uv);
#endif
    v_uv /= texture_size;
    if (perspective_interpolate == 0.0) {
        v_uv *= vi.world_pos.w;
    }

#ifdef WR_FEATURE_TEXTURE_RECT
    v_uv_bounds = vec4(0.0, 0.0, vec2(textureSize(sColor0)));
#else
    v_uv_bounds = vec4(min_uv, max_uv) / texture_size.xyxy;
#endif

#ifdef WR_FEATURE_REPETITION
    // Normalize UV to 0..1 scale only if using repetition. Otherwise, leave
    // UVs unnormalized since we won't compute a modulus without repetition
    // enabled.
    v_uv /= (v_uv_bounds.zw - v_uv_bounds.xy);
#endif

#ifdef WR_FEATURE_ALPHA_PASS
    v_tile_repeat_bounds = repeat.xy + normalized_offset;

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
        case COLOR_MODE_BITMAP_SHADOW:
            #ifdef SWGL_BLEND
                swgl_blendDropShadow(image_data.color);
                v_mask_swizzle = vec2(1.0, 0.0);
                v_color = vec4(1.0);
            #else
                v_mask_swizzle = vec2(0.0, 1.0);
                v_color = image_data.color;
            #endif
            break;
        case COLOR_MODE_IMAGE:
            v_mask_swizzle = vec2(1.0, 0.0);
            v_color = image_data.color;
            break;
        case COLOR_MODE_COLOR_BITMAP:
            v_mask_swizzle = vec2(1.0, 0.0);
            v_color = vec4(image_data.color.a);
            break;
        case COLOR_MODE_SUBPX_DUAL_SOURCE:
            v_mask_swizzle = vec2(image_data.color.a, 0.0);
            v_color = image_data.color;
            break;
        case COLOR_MODE_MULTIPLY_DUAL_SOURCE:
            v_mask_swizzle = vec2(-image_data.color.a, image_data.color.a);
            v_color = image_data.color;
            break;
        default:
            v_mask_swizzle = vec2(0.0);
            v_color = vec4(1.0);
    }
#endif
}
#endif

#ifdef WR_FRAGMENT_SHADER

vec2 compute_repeated_uvs(float perspective_divisor) {
#ifdef WR_FEATURE_REPETITION
    vec2 uv_size = v_uv_bounds.zw - v_uv_bounds.xy;

    #ifdef WR_FEATURE_ALPHA_PASS
    vec2 local_uv = v_uv * perspective_divisor;
    // This prevents the uv on the top and left parts of the primitive that was inflated
    // for anti-aliasing purposes from going beyound the range covered by the regular
    // (non-inflated) primitive.
    local_uv = max(local_uv, vec2(0.0));

    // Handle horizontal and vertical repetitions.
    vec2 repeated_uv = fract(local_uv) * uv_size + v_uv_bounds.xy;

    // This takes care of the bottom and right inflated parts.
    // We do it after the modulo because the latter wraps around the values exactly on
    // the right and bottom edges, which we do not want.
    if (local_uv.x >= v_tile_repeat_bounds.x) {
        repeated_uv.x = v_uv_bounds.z;
    }
    if (local_uv.y >= v_tile_repeat_bounds.y) {
        repeated_uv.y = v_uv_bounds.w;
    }
    #else
    vec2 repeated_uv = fract(v_uv * perspective_divisor) * uv_size + v_uv_bounds.xy;
    #endif

    return repeated_uv;
#else
    return v_uv * perspective_divisor + v_uv_bounds.xy;
#endif
}

Fragment brush_fs() {
    float perspective_divisor = mix(gl_FragCoord.w, 1.0, v_perspective.x);
    vec2 repeated_uv = compute_repeated_uvs(perspective_divisor);

    // Clamp the uvs to avoid sampling artifacts.
    vec2 uv = clamp(repeated_uv, v_uv_sample_bounds.xy, v_uv_sample_bounds.zw);

    vec4 texel = TEX_SAMPLE(sColor0, uv);

    Fragment frag;

#ifdef WR_FEATURE_ALPHA_PASS
    #ifdef WR_FEATURE_ANTIALIASING
        float alpha = antialias_brush();
    #else
        float alpha = 1.0;
    #endif
    #ifndef WR_FEATURE_DUAL_SOURCE_BLENDING
        texel.rgb = texel.rgb * v_mask_swizzle.x + texel.aaa * v_mask_swizzle.y;
    #endif

    vec4 alpha_mask = texel * alpha;
    frag.color = v_color * alpha_mask;

    #ifdef WR_FEATURE_DUAL_SOURCE_BLENDING
        frag.blend = alpha_mask * v_mask_swizzle.x + alpha_mask.aaaa * v_mask_swizzle.y;
    #endif
#else
    frag.color = texel;
#endif

    return frag;
}

#if defined(SWGL_DRAW_SPAN) && (!defined(WR_FEATURE_ALPHA_PASS) || !defined(WR_FEATURE_DUAL_SOURCE_BLENDING))
void swgl_drawSpanRGBA8() {
    if (!swgl_isTextureRGBA8(sColor0)) {
        return;
    }

    #ifdef WR_FEATURE_ALPHA_PASS
        if (v_mask_swizzle != vec2(1.0, 0.0)) {
            return;
        }
    #endif

    float perspective_divisor = mix(swgl_forceScalar(gl_FragCoord.w), 1.0, v_perspective.x);

    #ifdef WR_FEATURE_REPETITION
        // Get the UVs before any repetition, scaling, or offsetting has occurred...
        vec2 uv = v_uv * perspective_divisor;
    #else
        vec2 uv = compute_repeated_uvs(perspective_divisor);
    #endif

    #ifdef WR_FEATURE_ALPHA_PASS
    if (v_color != vec4(1.0)) {
        #ifdef WR_FEATURE_REPETITION
            swgl_commitTextureRepeatColorRGBA8(sColor0, uv, v_tile_repeat_bounds, v_uv_bounds, v_uv_sample_bounds, v_color);
        #else
            swgl_commitTextureColorRGBA8(sColor0, uv, v_uv_sample_bounds, v_color);
        #endif
        return;
    }
    // No color scaling required, so just fall through to a normal textured span...
    #endif

    #ifdef WR_FEATURE_REPETITION
        #ifdef WR_FEATURE_ALPHA_PASS
            swgl_commitTextureRepeatRGBA8(sColor0, uv, v_tile_repeat_bounds, v_uv_bounds, v_uv_sample_bounds);
        #else
            swgl_commitTextureRepeatRGBA8(sColor0, uv, vec2(0.0), v_uv_bounds, v_uv_sample_bounds);
        #endif
    #else
        swgl_commitTextureRGBA8(sColor0, uv, v_uv_sample_bounds);
    #endif
}
#endif

#endif
