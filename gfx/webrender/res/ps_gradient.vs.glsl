#line 1
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

void main(void) {
    Primitive prim = load_primitive();
    Gradient gradient = fetch_gradient(prim.prim_index);

    GradientStop g0 = fetch_gradient_stop(prim.sub_index + 0);
    GradientStop g1 = fetch_gradient_stop(prim.sub_index + 1);

    RectWithSize segment_rect;
    vec2 axis;
    vec4 adjusted_color_g0 = g0.color;
    vec4 adjusted_color_g1 = g1.color;
    if (gradient.start_end_point.y == gradient.start_end_point.w) {
        vec2 x = mix(gradient.start_end_point.xx, gradient.start_end_point.zz,
                     vec2(g0.offset.x, g1.offset.x));
        // The start and end point of gradient might exceed the geometry rect. So clamp
        // it to the geometry rect.
        x = clamp(x, prim.local_rect.p0.xx, prim.local_rect.p0.xx + prim.local_rect.size.xx);
        vec2 adjusted_offset =
            (x - gradient.start_end_point.xx) / (gradient.start_end_point.zz - gradient.start_end_point.xx);
        adjusted_color_g0 = mix(g0.color, g1.color, adjusted_offset.x);
        adjusted_color_g1 = mix(g0.color, g1.color, adjusted_offset.y);
        segment_rect.p0 = vec2(x.x, prim.local_rect.p0.y);
        segment_rect.size = vec2(x.y - x.x, prim.local_rect.size.y);
        axis = vec2(1.0, 0.0);
    } else {
        vec2 y = mix(gradient.start_end_point.yy, gradient.start_end_point.ww,
                     vec2(g0.offset.x, g1.offset.x));
        // The start and end point of gradient might exceed the geometry rect. So clamp
        // it to the geometry rect.
        y = clamp(y, prim.local_rect.p0.yy, prim.local_rect.p0.yy + prim.local_rect.size.yy);
        vec2 adjusted_offset =
            (y - gradient.start_end_point.yy) / (gradient.start_end_point.ww - gradient.start_end_point.yy);
        adjusted_color_g0 = mix(g0.color, g1.color, adjusted_offset.x);
        adjusted_color_g1 = mix(g0.color, g1.color, adjusted_offset.y);
        segment_rect.p0 = vec2(prim.local_rect.p0.x, y.x);
        segment_rect.size = vec2(prim.local_rect.size.x, y.y - y.x);
        axis = vec2(0.0, 1.0);
    }

#ifdef WR_FEATURE_TRANSFORM
    TransformVertexInfo vi = write_transform_vertex(segment_rect,
                                                    prim.local_clip_rect,
                                                    prim.z,
                                                    prim.layer,
                                                    prim.task);
    vLocalRect = vi.clipped_local_rect;
    vLocalPos = vi.local_pos;
    vec2 f = (vi.local_pos.xy - prim.local_rect.p0) / prim.local_rect.size;
#else
    VertexInfo vi = write_vertex(segment_rect,
                                 prim.local_clip_rect,
                                 prim.z,
                                 prim.layer,
                                 prim.task);

    vec2 f = (vi.local_pos - segment_rect.p0) / segment_rect.size;
    vPos = vi.local_pos;
#endif

    write_clip(vi.screen_pos, prim.clip_area);

    vColor = mix(adjusted_color_g0, adjusted_color_g1, dot(f, axis));
}
