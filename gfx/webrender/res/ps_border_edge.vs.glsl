#line 1
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

void write_edge_distance(float p0,
                         float original_width,
                         float adjusted_width,
                         float style,
                         float axis_select,
                         float sign_adjust) {
    switch (int(style)) {
        case BORDER_STYLE_DOUBLE:
            vEdgeDistance = vec2(p0 + adjusted_width,
                                 p0 + original_width - adjusted_width);
            break;
        case BORDER_STYLE_GROOVE:
        case BORDER_STYLE_RIDGE:
            vEdgeDistance = vec2(p0 + adjusted_width, sign_adjust);
            break;
        default:
            vEdgeDistance = vec2(0.0);
            break;
    }

    vAxisSelect = axis_select;
}

void write_alpha_select(float style) {
    switch (int(style)) {
        case BORDER_STYLE_DOUBLE:
            vAlphaSelect = 0.0;
            break;
        default:
            vAlphaSelect = 1.0;
            break;
    }
}

void write_color(vec4 color, float style, bool flip) {
    vec2 modulate;

    switch (int(style)) {
        case BORDER_STYLE_GROOVE:
            modulate = flip ? vec2(1.3, 0.7) : vec2(0.7, 1.3);
            break;
        case BORDER_STYLE_RIDGE:
            modulate = flip ? vec2(0.7, 1.3) : vec2(1.3, 0.7);
            break;
        default:
            modulate = vec2(1.0);
            break;
    }

    vColor0 = vec4(color.rgb * modulate.x, color.a);
    vColor1 = vec4(color.rgb * modulate.y, color.a);
}

void main(void) {
    Primitive prim = load_primitive();
    Border border = fetch_border(prim.prim_index);
    int sub_part = prim.sub_index;
    BorderCorners corners = get_border_corners(border, prim.local_rect);
    vec4 adjusted_widths = get_effective_border_widths(border);
    vec4 color = border.colors[sub_part];

    RectWithSize segment_rect;
    switch (sub_part) {
        case 0:
            segment_rect.p0 = vec2(corners.tl_outer.x, corners.tl_inner.y);
            segment_rect.size = vec2(border.widths.x, corners.bl_inner.y - corners.tl_inner.y);
            write_edge_distance(segment_rect.p0.x, border.widths.x, adjusted_widths.x, border.style.x, 0.0, 1.0);
            write_alpha_select(border.style.x);
            write_color(color, border.style.x, false);
            break;
        case 1:
            segment_rect.p0 = vec2(corners.tl_inner.x, corners.tl_outer.y);
            segment_rect.size = vec2(corners.tr_inner.x - corners.tl_inner.x, border.widths.y);
            write_edge_distance(segment_rect.p0.y, border.widths.y, adjusted_widths.y, border.style.y, 1.0, 1.0);
            write_alpha_select(border.style.y);
            write_color(color, border.style.y, false);
            break;
        case 2:
            segment_rect.p0 = vec2(corners.tr_outer.x - border.widths.z, corners.tr_inner.y);
            segment_rect.size = vec2(border.widths.z, corners.br_inner.y - corners.tr_inner.y);
            write_edge_distance(segment_rect.p0.x, border.widths.z, adjusted_widths.z, border.style.z, 0.0, -1.0);
            write_alpha_select(border.style.z);
            write_color(color, border.style.z, true);
            break;
        case 3:
            segment_rect.p0 = vec2(corners.bl_inner.x, corners.bl_outer.y - border.widths.w);
            segment_rect.size = vec2(corners.br_inner.x - corners.bl_inner.x, border.widths.w);
            write_edge_distance(segment_rect.p0.y, border.widths.w, adjusted_widths.w, border.style.w, 1.0, -1.0);
            write_alpha_select(border.style.w);
            write_color(color, border.style.w, true);
            break;
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
    vLocalPos = vi.local_pos;
#endif

    write_clip(vi.screen_pos, prim.clip_area);
}
