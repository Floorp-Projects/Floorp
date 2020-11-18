/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define VECS_PER_SPECIFIC_BRUSH 2

#include shared,prim_shared,brush

flat varying HIGHP_FS_ADDRESS int v_gradient_address;

flat varying vec2 v_center;
flat varying float v_start_radius;
flat varying float v_radius_scale;

flat varying vec2 v_repeated_size;
flat varying float v_gradient_repeat;

varying vec2 v_pos;

#ifdef WR_FEATURE_ALPHA_PASS
varying vec2 v_local_pos;
flat varying vec2 v_tile_repeat;
#endif

#ifdef WR_VERTEX_SHADER

struct RadialGradient {
    vec4 center_start_end_radius;
    float ratio_xy;
    int extend_mode;
    vec2 stretch_size;
};

RadialGradient fetch_radial_gradient(int address) {
    vec4 data[2] = fetch_from_gpu_cache_2(address);
    return RadialGradient(
        data[0],
        data[1].x,
        int(data[1].y),
        data[1].zw
    );
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
    vec4 texel_rect
) {
    RadialGradient gradient = fetch_radial_gradient(prim_address);

    if ((brush_flags & BRUSH_FLAG_SEGMENT_RELATIVE) != 0) {
        v_pos = (vi.local_pos - segment_rect.p0) / segment_rect.size;
        v_pos = v_pos * (texel_rect.zw - texel_rect.xy) + texel_rect.xy;
        v_pos = v_pos * local_rect.size;
    } else {
        v_pos = vi.local_pos - local_rect.p0;
    }

    v_center = gradient.center_start_end_radius.xy;
    v_start_radius = gradient.center_start_end_radius.z;
    if (gradient.center_start_end_radius.z != gradient.center_start_end_radius.w) {
      // Store 1/rd where rd = end_radius - start_start
      v_radius_scale = 1.0 / (gradient.center_start_end_radius.w - gradient.center_start_end_radius.z);
    } else {
      // If rd = 0, we can't get its reciprocal. Instead, just use a zero scale.
      v_radius_scale = 0.0;
    }

    // Transform all coordinates by the y scale so the
    // fragment shader can work with circles
    vec2 tile_repeat = local_rect.size / gradient.stretch_size;
    v_pos.y *= gradient.ratio_xy;
    v_center.y *= gradient.ratio_xy;
    v_repeated_size = gradient.stretch_size;
    v_repeated_size.y *=  gradient.ratio_xy;

    v_gradient_address = prim_user_data.x;

    // Whether to repeat the gradient instead of clamping.
    v_gradient_repeat = float(gradient.extend_mode != EXTEND_MODE_CLAMP);

#ifdef WR_FEATURE_ALPHA_PASS
    v_tile_repeat = tile_repeat.xy;
    v_local_pos = vi.local_pos;
#endif
}
#endif

#ifdef WR_FRAGMENT_SHADER
Fragment brush_fs() {

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

    // Solve for t in length(pd) = v_start_radius + t * rd
    vec2 pd = pos - v_center;
    float offset = (length(pd) - v_start_radius) * v_radius_scale;

    vec4 color = sample_gradient(v_gradient_address,
                                 offset,
                                 v_gradient_repeat);

#ifdef WR_FEATURE_ALPHA_PASS
    color *= init_transform_fs(v_local_pos);
#endif

    return Fragment(color);
}
#endif
