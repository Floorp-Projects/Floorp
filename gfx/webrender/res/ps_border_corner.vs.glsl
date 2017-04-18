#line 1
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

void set_radii(vec2 border_radius,
               vec2 border_width,
               vec2 outer_corner,
               vec2 gradient_sign) {
    if (border_radius.x > 0.0 && border_radius.y > 0.0) {
        // Set inner/outer radius on valid border radius.
        vOuterRadii = border_radius;
    } else {
        // No border radius - ensure clip has no effect.
        vOuterRadii = vec2(2.0 * border_width);
    }

    if (border_radius.x > border_width.x && border_radius.y > border_width.y) {
        vInnerRadii = max(vec2(0.0), border_radius - border_width);
    } else {
        vInnerRadii = vec2(0.0);
    }

    vec2 gradient = border_width * gradient_sign;
    vColorEdgeLine = vec4(outer_corner, vec2(-gradient.y, gradient.x));
}

void main(void) {
    Primitive prim = load_primitive();
    Border border = fetch_border(prim.prim_index);
    int sub_part = prim.sub_index;
    BorderCorners corners = get_border_corners(border, prim.local_rect);

    RectWithSize segment_rect;
    switch (sub_part) {
        case 0: {
            segment_rect.p0 = corners.tl_outer;
            segment_rect.size = corners.tl_inner - corners.tl_outer;
            vColor0 = border.colors[0];
            vColor1 = border.colors[1];
            vClipCenter = corners.tl_outer + border.radii[0].xy;
            vClipSign = vec2(1.0);
            set_radii(border.radii[0].xy,
                      border.widths.xy,
                      corners.tl_outer,
                      vec2(1.0, 1.0));
            break;
        }
        case 1: {
            segment_rect.p0 = vec2(corners.tr_inner.x, corners.tr_outer.y);
            segment_rect.size = vec2(corners.tr_outer.x - corners.tr_inner.x,
                                     corners.tr_inner.y - corners.tr_outer.y);
            vColor0 = border.colors[1];
            vColor1 = border.colors[2];
            vClipCenter = corners.tr_outer + vec2(-border.radii[0].z, border.radii[0].w);
            vClipSign = vec2(-1.0, 1.0);
            set_radii(border.radii[0].zw,
                      border.widths.zy,
                      corners.tr_outer,
                      vec2(-1.0, 1.0));
            break;
        }
        case 2: {
            segment_rect.p0 = corners.br_inner;
            segment_rect.size = corners.br_outer - corners.br_inner;
            vColor0 = border.colors[2];
            vColor1 = border.colors[3];
            vClipCenter = corners.br_outer - border.radii[1].xy;
            vClipSign = vec2(-1.0, -1.0);
            set_radii(border.radii[1].xy,
                      border.widths.zw,
                      corners.br_outer,
                      vec2(-1.0, -1.0));
            break;
        }
        case 3: {
            segment_rect.p0 = vec2(corners.bl_outer.x, corners.bl_inner.y);
            segment_rect.size = vec2(corners.bl_inner.x - corners.bl_outer.x,
                                     corners.bl_outer.y - corners.bl_inner.y);
            vColor0 = border.colors[3];
            vColor1 = border.colors[0];
            vClipCenter = corners.bl_outer + vec2(border.radii[1].z, -border.radii[1].w);
            vClipSign = vec2(1.0, -1.0);
            set_radii(border.radii[1].zw,
                      border.widths.xw,
                      corners.bl_outer,
                      vec2(1.0, -1.0));
            break;
        }
    }

#ifdef WR_FEATURE_TRANSFORM
    TransformVertexInfo vi = write_transform_vertex(segment_rect,
                                                    prim.local_clip_rect,
                                                    prim.z,
                                                    prim.layer,
                                                    prim.task,
                                                    prim.local_rect.p0);
    vLocalPos = vi.local_pos;
    vLocalRect = segment_rect;
#else
    VertexInfo vi = write_vertex(segment_rect,
                                 prim.local_clip_rect,
                                 prim.z,
                                 prim.layer,
                                 prim.task,
                                 prim.local_rect.p0);
    vLocalPos = vi.local_pos.xy;
#endif

    write_clip(vi.screen_pos, prim.clip_area);
}
