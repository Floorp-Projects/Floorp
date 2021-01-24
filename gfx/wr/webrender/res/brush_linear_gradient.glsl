/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define VECS_PER_SPECIFIC_BRUSH 2

#include shared,prim_shared,brush,gradient_shared

flat varying vec2 v_start_point;
flat varying vec2 v_scale_dir;

#ifdef WR_VERTEX_SHADER

struct Gradient {
    vec4 start_end_point;
    int extend_mode;
    vec2 stretch_size;
};

Gradient fetch_gradient(int address) {
    vec4 data[2] = fetch_from_gpu_cache_2(address);
    return Gradient(
        data[0],
        int(data[1].x),
        data[1].yz
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
    Gradient gradient = fetch_gradient(prim_address);

    write_gradient_vertex(
        vi,
        local_rect,
        segment_rect,
        prim_user_data,
        brush_flags,
        texel_rect,
        gradient.extend_mode,
        gradient.stretch_size
    );

    vec2 start_point = gradient.start_end_point.xy;
    vec2 end_point = gradient.start_end_point.zw;
    vec2 dir = end_point - start_point;

    v_start_point = start_point;
    v_scale_dir = dir / dot(dir, dir);

    // Normalize UV and offsets to 0..1 scale.
    v_start_point /= v_repeated_size;
    v_scale_dir *= v_repeated_size;
}
#endif

#ifdef WR_FRAGMENT_SHADER
Fragment brush_fs() {
    vec2 pos = compute_gradient_pos();

    float offset = dot(pos - v_start_point, v_scale_dir);

    vec4 color = sample_gradient(offset);

#ifdef WR_FEATURE_ALPHA_PASS
    color *= init_transform_fs(v_local_pos);
#endif

    return Fragment(color);
}
#endif
