#line 1
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

void main(void) {
    Primitive prim = load_primitive();
    Gradient gradient = fetch_gradient(prim.prim_index);

    vec4 abs_start_end_point = gradient.start_end_point + prim.local_rect.p0.xyxy;

    GradientStop g0 = fetch_gradient_stop(prim.user_data0 + 0);
    GradientStop g1 = fetch_gradient_stop(prim.user_data0 + 1);

    RectWithSize segment_rect;
    vec2 axis;
    vec4 adjusted_color_g0 = g0.color;
    vec4 adjusted_color_g1 = g1.color;
    if (abs_start_end_point.y == abs_start_end_point.w) {
        // Calculate the x coord of the gradient stops
        vec2 g01_x = mix(abs_start_end_point.xx, abs_start_end_point.zz,
                         vec2(g0.offset.x, g1.offset.x));

        // The gradient stops might exceed the geometry rect so clamp them
        vec2 g01_x_clamped = clamp(g01_x,
                                   prim.local_rect.p0.xx,
                                   prim.local_rect.p0.xx + prim.local_rect.size.xx);

        // Calculate the segment rect using the clamped coords
        segment_rect.p0 = vec2(g01_x_clamped.x, prim.local_rect.p0.y);
        segment_rect.size = vec2(g01_x_clamped.y - g01_x_clamped.x, prim.local_rect.size.y);
        axis = vec2(1.0, 0.0);

        // Adjust the stop colors by how much they were clamped
        vec2 adjusted_offset = (g01_x_clamped - g01_x.xx) / (g01_x.y - g01_x.x);
        adjusted_color_g0 = mix(g0.color, g1.color, adjusted_offset.x);
        adjusted_color_g1 = mix(g0.color, g1.color, adjusted_offset.y);
    } else {
        // Calculate the y coord of the gradient stops
        vec2 g01_y = mix(abs_start_end_point.yy, abs_start_end_point.ww,
                         vec2(g0.offset.x, g1.offset.x));

        // The gradient stops might exceed the geometry rect so clamp them
        vec2 g01_y_clamped = clamp(g01_y,
                                   prim.local_rect.p0.yy,
                                   prim.local_rect.p0.yy + prim.local_rect.size.yy);

        // Calculate the segment rect using the clamped coords
        segment_rect.p0 = vec2(prim.local_rect.p0.x, g01_y_clamped.x);
        segment_rect.size = vec2(prim.local_rect.size.x, g01_y_clamped.y - g01_y_clamped.x);
        axis = vec2(0.0, 1.0);

        // Adjust the stop colors by how much they were clamped
        vec2 adjusted_offset = (g01_y_clamped - g01_y.xx) / (g01_y.y - g01_y.x);
        adjusted_color_g0 = mix(g0.color, g1.color, adjusted_offset.x);
        adjusted_color_g1 = mix(g0.color, g1.color, adjusted_offset.y);
    }

#ifdef WR_FEATURE_TRANSFORM
    TransformVertexInfo vi = write_transform_vertex(segment_rect,
                                                    prim.local_clip_rect,
                                                    prim.z,
                                                    prim.layer,
                                                    prim.task,
                                                    prim.local_rect.p0);
    vLocalPos = vi.local_pos;
    vec2 f = (vi.local_pos.xy - prim.local_rect.p0) / prim.local_rect.size;
#else
    VertexInfo vi = write_vertex(segment_rect,
                                 prim.local_clip_rect,
                                 prim.z,
                                 prim.layer,
                                 prim.task,
                                 prim.local_rect.p0);

    vec2 f = (vi.local_pos - segment_rect.p0) / segment_rect.size;
    vPos = vi.local_pos;
#endif

    write_clip(vi.screen_pos, prim.clip_area);

    vColor = mix(adjusted_color_g0, adjusted_color_g1, dot(f, axis));
}
