/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define VECS_PER_SPECIFIC_BRUSH 3

#include shared,prim_shared,brush

#ifdef WR_FEATURE_ALPHA_PASS
varying vec2 v_local_pos;
#endif

// Interpolated UV coordinates to sample.
varying vec2 v_uv;

#ifdef WR_FEATURE_ALPHA_PASS
flat varying vec4 v_color;
flat varying vec2 v_mask_swizzle;
flat varying vec2 v_tile_repeat;
#endif

// Normalized bounds of the source image in the texture.
flat varying vec4 v_uv_bounds;
// Normalized bounds of the source image in the texture, adjusted to avoid
// sampling artifacts.
flat varying vec4 v_uv_sample_bounds;
// x: Layer index to sample.
// y: Flag to allow perspective interpolation of UV.
flat varying vec2 v_layer_and_perspective;

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
    vec2 texture_size = vec2(textureSize(sColor0, 0));
#endif

    ImageResource res = fetch_image_resource(specific_resource_address);
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
                        repeated_stretch_size.x = prim_rect.p0.x + prim_rect.size.x
                            - segment_rect.p0.x - segment_rect.size.x;
                    }

                    // Adjust the the referecne uv size to compute horizontal repetitions
                    // for the fill area.
                    horizontal_uv_size.y = uv0.y - res.uv_rect.p0.y;
                    if (horizontal_uv_size.y < epsilon || repeated_stretch_size.y < epsilon) {
                        horizontal_uv_size.y = res.uv_rect.p1.y - uv1.y;
                        repeated_stretch_size.y = prim_rect.p0.y + prim_rect.size.y
                            - segment_rect.p0.y - segment_rect.size.y;
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
                float nx = max(1.0, round(segment_rect.size.x / stretch_size.x));
                stretch_size.x = segment_rect.size.x / nx;
            }
            if ((brush_flags & BRUSH_FLAG_SEGMENT_REPEAT_Y_ROUND) != 0) {
                float ny = max(1.0, round(segment_rect.size.y / stretch_size.y));
                stretch_size.y = segment_rect.size.y / ny;
            }
        #endif
    }

    float perspective_interpolate = (brush_flags & BRUSH_FLAG_PERSPECTIVE_INTERPOLATION) != 0 ? 1.0 : 0.0;
    v_layer_and_perspective = vec2(res.layer, perspective_interpolate);

    // Handle case where the UV coords are inverted (e.g. from an
    // external image).
    vec2 min_uv = min(uv0, uv1);
    vec2 max_uv = max(uv0, uv1);

    v_uv_sample_bounds = vec4(
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
            f = get_image_quad_uv(specific_resource_address, f);
            break;
        }
        default:
            break;
    }
#endif

    // Offset and scale v_uv here to avoid doing it in the fragment shader.
    vec2 repeat = local_rect.size / stretch_size;
    v_uv = mix(uv0, uv1, f) - min_uv;
    v_uv /= texture_size;
    v_uv *= repeat.xy;
    if (perspective_interpolate == 0.0) {
        v_uv *= vi.world_pos.w;
    }

#ifdef WR_FEATURE_TEXTURE_RECT
    v_uv_bounds = vec4(0.0, 0.0, vec2(textureSize(sColor0)));
#else
    v_uv_bounds = vec4(min_uv, max_uv) / texture_size.xyxy;
#endif

#ifdef WR_FEATURE_ALPHA_PASS
    v_tile_repeat = repeat.xy;

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
            v_mask_swizzle = vec2(0.0, 1.0);
            v_color = image_data.color;
            break;
        case COLOR_MODE_SUBPX_BG_PASS2:
        case COLOR_MODE_SUBPX_DUAL_SOURCE:
        case COLOR_MODE_IMAGE:
            v_mask_swizzle = vec2(1.0, 0.0);
            v_color = image_data.color;
            break;
        case COLOR_MODE_SUBPX_CONST_COLOR:
        case COLOR_MODE_SUBPX_BG_PASS0:
        case COLOR_MODE_COLOR_BITMAP:
            v_mask_swizzle = vec2(1.0, 0.0);
            v_color = vec4(image_data.color.a);
            break;
        case COLOR_MODE_SUBPX_BG_PASS1:
            v_mask_swizzle = vec2(-1.0, 1.0);
            v_color = vec4(image_data.color.a) * image_data.background_color;
            break;
        default:
            v_mask_swizzle = vec2(0.0);
            v_color = vec4(1.0);
    }

    v_local_pos = vi.local_pos;
#endif
}
#endif

#ifdef WR_FRAGMENT_SHADER

vec2 compute_repeated_uvs(float perspective_divisor) {
    vec2 uv_size = v_uv_bounds.zw - v_uv_bounds.xy;

#ifdef WR_FEATURE_ALPHA_PASS
    // This prevents the uv on the top and left parts of the primitive that was inflated
    // for anti-aliasing purposes from going beyound the range covered by the regular
    // (non-inflated) primitive.
    vec2 local_uv = max(v_uv * perspective_divisor, vec2(0.0));

    // Handle horizontal and vertical repetitions.
    vec2 repeated_uv = mod(local_uv, uv_size) + v_uv_bounds.xy;

    // This takes care of the bottom and right inflated parts.
    // We do it after the modulo because the latter wraps around the values exactly on
    // the right and bottom edges, which we do not want.
    if (local_uv.x >= v_tile_repeat.x * uv_size.x) {
        repeated_uv.x = v_uv_bounds.z;
    }
    if (local_uv.y >= v_tile_repeat.y * uv_size.y) {
        repeated_uv.y = v_uv_bounds.w;
    }
#else
    vec2 repeated_uv = mod(v_uv * perspective_divisor, uv_size) + v_uv_bounds.xy;
#endif

    return repeated_uv;
}

Fragment brush_fs() {
    float perspective_divisor = mix(gl_FragCoord.w, 1.0, v_layer_and_perspective.y);

#ifdef WR_FEATURE_REPETITION
    vec2 repeated_uv = compute_repeated_uvs(perspective_divisor);
#else
    vec2 repeated_uv = v_uv * perspective_divisor + v_uv_bounds.xy;
#endif

    // Clamp the uvs to avoid sampling artifacts.
    vec2 uv = clamp(repeated_uv, v_uv_sample_bounds.xy, v_uv_sample_bounds.zw);

    vec4 texel = TEX_SAMPLE(sColor0, vec3(uv, v_layer_and_perspective.x));

    Fragment frag;

#ifdef WR_FEATURE_ALPHA_PASS
    #ifdef WR_FEATURE_ANTIALIASING
        float alpha = init_transform_fs(v_local_pos);
    #else
        float alpha = 1.0;
    #endif
    texel.rgb = texel.rgb * v_mask_swizzle.x + texel.aaa * v_mask_swizzle.y;

    vec4 alpha_mask = texel * alpha;
    frag.color = v_color * alpha_mask;

    #ifdef WR_FEATURE_DUAL_SOURCE_BLENDING
        frag.blend = alpha_mask * v_color.a;
    #endif
#else
    frag.color = texel;
#endif

    return frag;
}

#if defined(SWGL) && (!defined(WR_FEATURE_ALPHA_PASS) || !defined(WR_FEATURE_DUAL_SOURCE_BLENDING))
void swgl_drawSpanRGBA8() {
    if (!swgl_isTextureRGBA8(sColor0) || !swgl_isTextureLinear(sColor0)) {
        return;
    }

    #ifdef WR_FEATURE_ALPHA_PASS
        if (v_mask_swizzle != vec2(1.0, 0.0)) {
            return;
        }
    #endif

    int layer = swgl_textureLayerOffset(sColor0, v_layer_and_perspective.x);

    float perspective_divisor = mix(swgl_forceScalar(gl_FragCoord.w), 1.0, v_layer_and_perspective.y);

    #ifndef WR_FEATURE_REPETITION
        vec2 uv = swgl_linearQuantize(sColor0, v_uv * perspective_divisor + v_uv_bounds.xy);
        vec2 min_uv = swgl_linearQuantize(sColor0, v_uv_sample_bounds.xy);
        vec2 max_uv = swgl_linearQuantize(sColor0, v_uv_sample_bounds.zw);
        vec2 step_uv = swgl_linearQuantizeStep(sColor0, swgl_interpStep(v_uv)) * perspective_divisor;
    #endif

    #ifdef WR_FEATURE_ALPHA_PASS
        if (needs_clip()) {
            while (swgl_SpanLength > 0) {
                vec4 color = v_color * do_clip();
                #ifdef WR_FEATURE_ANTIALIASING
                    color *= init_transform_fs(v_local_pos);
                    v_local_pos += swgl_interpStep(v_local_pos);
                #endif
                #ifdef WR_FEATURE_REPETITION
                    vec2 repeated_uv = compute_repeated_uvs(perspective_divisor);
                    vec2 uv = clamp(repeated_uv, v_uv_sample_bounds.xy, v_uv_sample_bounds.zw);
                    swgl_commitTextureLinearColorRGBA8(sColor0, swgl_linearQuantize(sColor0, uv), color, layer);
                    v_uv += swgl_interpStep(v_uv);
                #else
                    swgl_commitTextureLinearColorRGBA8(sColor0, clamp(uv, min_uv, max_uv), color, layer);
                    uv += step_uv;
                #endif
                vClipMaskUv += swgl_interpStep(vClipMaskUv);
            }
            return;
        #ifdef WR_FEATURE_ANTIALIASING
        } else {
        #else
        } else if (v_color != vec4(1.0)) {
        #endif
            while (swgl_SpanLength > 0) {
                vec4 color = v_color;
                #ifdef WR_FEATURE_ANTIALIASING
                    color *= init_transform_fs(v_local_pos);
                    v_local_pos += swgl_interpStep(v_local_pos);
                #endif
                #ifdef WR_FEATURE_REPETITION
                    vec2 repeated_uv = compute_repeated_uvs(perspective_divisor);
                    vec2 uv = clamp(repeated_uv, v_uv_sample_bounds.xy, v_uv_sample_bounds.zw);
                    swgl_commitTextureLinearColorRGBA8(sColor0, swgl_linearQuantize(sColor0, uv), color, layer);
                    v_uv += swgl_interpStep(v_uv);
                #else
                    swgl_commitTextureLinearColorRGBA8(sColor0, clamp(uv, min_uv, max_uv), color, layer);
                    uv += step_uv;
                #endif
            }
            return;
        }
        // No clip or color scaling required, so just fall through to a normal textured span...
    #endif

    while (swgl_SpanLength > 0) {
        #ifdef WR_FEATURE_REPETITION
            vec2 repeated_uv = compute_repeated_uvs(perspective_divisor);
            vec2 uv = clamp(repeated_uv, v_uv_sample_bounds.xy, v_uv_sample_bounds.zw);
            swgl_commitTextureLinearRGBA8(sColor0, swgl_linearQuantize(sColor0, uv), layer);
            v_uv += swgl_interpStep(v_uv);
        #else
            swgl_commitTextureLinearRGBA8(sColor0, clamp(uv, min_uv, max_uv), layer);
            uv += step_uv;
        #endif
    }
}
#endif

#endif
