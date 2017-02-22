#line 1
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

void main(void) {
    Primitive prim = load_primitive();
    Gradient gradient = fetch_gradient(prim.prim_index);

    GradientStop g0 = fetch_gradient_stop(prim.sub_index + 0);
    GradientStop g1 = fetch_gradient_stop(prim.sub_index + 1);

    vec4 segment_rect;
    vec2 axis;
    if (gradient.start_end_point.y == gradient.start_end_point.w) {
        float x0 = mix(gradient.start_end_point.x,
                       gradient.start_end_point.z,
                       g0.offset.x);
        float x1 = mix(gradient.start_end_point.x,
                       gradient.start_end_point.z,
                       g1.offset.x);
        segment_rect.yw = prim.local_rect.yw;
        segment_rect.x = x0;
        segment_rect.z = x1 - x0;
        axis = vec2(1.0, 0.0);
    } else {
        float y0 = mix(gradient.start_end_point.y,
                       gradient.start_end_point.w,
                       g0.offset.x);
        float y1 = mix(gradient.start_end_point.y,
                       gradient.start_end_point.w,
                       g1.offset.x);
        segment_rect.xz = prim.local_rect.xz;
        segment_rect.y = y0;
        segment_rect.w = y1 - y0;
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
    vec2 f = (vi.local_pos.xy - prim.local_rect.xy) / prim.local_rect.zw;
#else
    VertexInfo vi = write_vertex(segment_rect,
                                 prim.local_clip_rect,
                                 prim.z,
                                 prim.layer,
                                 prim.task);

    vec2 f = (vi.local_pos - segment_rect.xy) / segment_rect.zw;
    vPos = vi.local_pos;
#endif

    write_clip(vi.screen_pos, prim.clip_area);

    vColor = mix(g0.color, g1.color, dot(f, axis));
}
