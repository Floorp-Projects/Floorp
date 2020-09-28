/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define VECS_PER_CONIC_GRADIENT_BRUSH 2
#define VECS_PER_SPECIFIC_BRUSH VECS_PER_CONIC_GRADIENT_BRUSH

#define WR_BRUSH_VS_FUNCTION conic_gradient_brush_vs
#define WR_BRUSH_FS_FUNCTION conic_gradient_brush_fs

#include shared,prim_shared,brush

flat varying HIGHP_FS_ADDRESS int  v_gradient_address;

flat varying vec2 v_center;
flat varying float v_start_offset;
flat varying float v_end_offset;
flat varying float v_angle;

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

#define PI                  3.141592653589793

#ifdef WR_VERTEX_SHADER

struct ConicGradient {
    vec2 center_point;
    vec2 start_end_offset;
    float angle;
    int extend_mode;
    vec2 stretch_size;
};

ConicGradient fetch_gradient(int address) {
    vec4 data[2] = fetch_from_gpu_cache_2(address);
    return ConicGradient(
        data[0].xy,
        data[0].zw,
        float(data[1].x),
        int(data[1].y),
        data[1].zw
    );
}

void conic_gradient_brush_vs(
    VertexInfo vi,
    int prim_address,
    RectWithSize local_rect,
    RectWithSize segment_rect,
    ivec4 prim_user_data,
    int specific_resource_address,
    mat4 transform,
    PictureTask pic_task,
    int brush_flags,
    vec4 texel_rect
) {
    ConicGradient gradient = fetch_gradient(prim_address);

    if ((brush_flags & BRUSH_FLAG_SEGMENT_RELATIVE) != 0) {
        v_pos = (vi.local_pos - segment_rect.p0) / segment_rect.size;
        v_pos = v_pos * (texel_rect.zw - texel_rect.xy) + texel_rect.xy;
        v_pos = v_pos * local_rect.size;
    } else {
        v_pos = vi.local_pos - local_rect.p0;
    }

    v_center = gradient.center_point;
    v_angle = gradient.angle;
    v_start_offset = gradient.start_end_offset.x;
    v_end_offset = gradient.start_end_offset.y;

    vec2 tile_repeat = local_rect.size / gradient.stretch_size;
    v_repeated_size = gradient.stretch_size;

    v_gradient_address = prim_user_data.x;

    // Whether to repeat the gradient along the line instead of clamping.
    v_gradient_repeat = float(gradient.extend_mode != EXTEND_MODE_CLAMP);

#ifdef WR_FEATURE_ALPHA_PASS
    v_tile_repeat = tile_repeat;
    v_local_pos = vi.local_pos;
#endif
}
#endif

#ifdef WR_FRAGMENT_SHADER
Fragment conic_gradient_brush_fs() {

#ifdef WR_FEATURE_ALPHA_PASS
    // Handle top and left inflated edges (see brush_image).
    vec2 local_pos = max(v_pos, vec2(0.0));

    // Apply potential horizontal and vertical repetitions.
    vec2 pos = mod(local_pos, v_repeated_size);

    vec2 prim_size = v_repeated_size * v_tile_repeat;
    // Handle bottom and right inflated edges (see brush_image).
    if (local_pos.x >= prim_size.x) {
        pos.x = v_repeated_size.x;
    }
    if (local_pos.y >= prim_size.y) {
        pos.y = v_repeated_size.y;
    }
#else
    // Apply potential horizontal and vertical repetitions.
    vec2 pos = mod(v_pos, v_repeated_size);
#endif

    vec2 current_dir = pos - v_center;
    float current_angle = atan(current_dir.y, current_dir.x) + (PI / 2.0 - v_angle);
    float offset = mod(current_angle / (2.0 * PI), 1.0) - v_start_offset;
    offset = offset / (v_end_offset - v_start_offset);

    vec4 color = sample_gradient(v_gradient_address,
                                 offset,
                                 v_gradient_repeat);

#ifdef WR_FEATURE_ALPHA_PASS
    color *= init_transform_fs(v_local_pos);
#endif

    return Fragment(color);
}
#endif
