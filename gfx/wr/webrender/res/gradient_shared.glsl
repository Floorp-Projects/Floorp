/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

flat varying HIGHP_FS_ADDRESS int v_gradient_address;

// Size of the gradient pattern's rectangle, used to compute horizontal and vertical
// repetitions. Not to be confused with another kind of repetition of the pattern
// which happens along the gradient stops.
flat varying vec2 v_repeated_size;
// Repetition along the gradient stops.
flat varying float v_gradient_repeat;

varying vec2 v_pos;

#ifdef WR_FEATURE_ALPHA_PASS
varying vec2 v_local_pos;
flat varying vec2 v_tile_repeat;
#endif

#ifdef WR_VERTEX_SHADER
void write_gradient_vertex(
    VertexInfo vi,
    RectWithSize local_rect,
    RectWithSize segment_rect,
    ivec4 prim_user_data,
    int brush_flags,
    vec4 texel_rect,
    int extend_mode,
    vec2 stretch_size
) {
    if ((brush_flags & BRUSH_FLAG_SEGMENT_RELATIVE) != 0) {
        v_pos = (vi.local_pos - segment_rect.p0) / segment_rect.size;
        v_pos = v_pos * (texel_rect.zw - texel_rect.xy) + texel_rect.xy;
        v_pos = v_pos * local_rect.size;
    } else {
        v_pos = vi.local_pos - local_rect.p0;
    }

    vec2 tile_repeat = local_rect.size / stretch_size;
    v_repeated_size = stretch_size;

    // Normalize UV to 0..1 scale.
    v_pos /= v_repeated_size;

    v_gradient_address = prim_user_data.x;

    // Whether to repeat the gradient along the line instead of clamping.
    v_gradient_repeat = float(extend_mode != EXTEND_MODE_CLAMP);

#ifdef WR_FEATURE_ALPHA_PASS
    v_tile_repeat = tile_repeat;
    v_local_pos = vi.local_pos;
#endif
}
#endif //WR_VERTEX_SHADER

#ifdef WR_FRAGMENT_SHADER
vec2 compute_gradient_pos() {
#ifdef WR_FEATURE_ALPHA_PASS
    // Handle top and left inflated edges (see brush_image).
    vec2 local_pos = max(v_pos, vec2(0.0));

    // Apply potential horizontal and vertical repetitions.
    vec2 pos = fract(local_pos);

    // Handle bottom and right inflated edges (see brush_image).
    if (local_pos.x >= v_tile_repeat.x) {
        pos.x = 1.0;
    }
    if (local_pos.y >= v_tile_repeat.y) {
        pos.y = 1.0;
    }
    return pos;
#else
    // Apply potential horizontal and vertical repetitions.
    return fract(v_pos);
#endif
}

#ifdef WR_FEATURE_DITHERING
vec4 dither(vec4 color) {
    const int matrix_mask = 7;

    ivec2 pos = ivec2(gl_FragCoord.xy) & ivec2(matrix_mask);
    float noise_normalized = (texelFetch(sDither, pos, 0).r * 255.0 + 0.5) / 64.0;
    float noise = (noise_normalized - 0.5) / 256.0; // scale down to the unit length

    return color + vec4(noise, noise, noise, 0);
}
#else
vec4 dither(vec4 color) {
    return color;
}
#endif //WR_FEATURE_DITHERING

vec4 sample_gradient(float offset) {
    // Modulo the offset if the gradient repeats.
    float x = offset - floor(offset) * v_gradient_repeat;

    // Calculate the color entry index to use for this offset:
    //     offsets < 0 use the first color entry, 0
    //     offsets from [0, 1) use the color entries in the range of [1, N-1)
    //     offsets >= 1 use the last color entry, N-1
    //     so transform the range [0, 1) -> [1, N-1)

    // TODO(gw): In the future we might consider making the size of the
    // LUT vary based on number / distribution of stops in the gradient.
    // Ensure we don't fetch outside the valid range of the LUT.
    const float GRADIENT_ENTRIES = 128.0;
    x = clamp(1.0 + x * GRADIENT_ENTRIES, 0.0, 1.0 + GRADIENT_ENTRIES);

    // Calculate the texel to index into the gradient color entries:
    //     floor(x) is the gradient color entry index
    //     fract(x) is the linear filtering factor between start and end
    float entry_index = floor(x);
    float entry_fract = x - entry_index;

    // Fetch the start and end color. There is a [start, end] color per entry.
    vec4 texels[2] = fetch_from_gpu_cache_2(v_gradient_address + 2 * int(entry_index));

    // Finally interpolate and apply dithering
    return dither(mix(texels[0], texels[1], entry_fract));
}

#endif //WR_FRAGMENT_SHADER

