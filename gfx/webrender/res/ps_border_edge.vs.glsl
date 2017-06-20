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
        {
            modulate = flip ? vec2(1.3, 0.7) : vec2(0.7, 1.3);
            break;
        }
        case BORDER_STYLE_RIDGE:
        {
            modulate = flip ? vec2(0.7, 1.3) : vec2(1.3, 0.7);
            break;
        }
        default:
            modulate = vec2(1.0);
            break;
    }

    vColor0 = vec4(color.rgb * modulate.x, color.a);
    vColor1 = vec4(color.rgb * modulate.y, color.a);
}

void write_clip_params(float style,
                       float border_width,
                       float edge_length,
                       float edge_offset,
                       float center_line) {
    // x = offset
    // y = dash on + off length
    // z = dash length
    // w = center line of edge cross-axis (for dots only)
    switch (int(style)) {
        case BORDER_STYLE_DASHED: {
            float desired_dash_length = border_width * 3.0;
            // Consider half total length since there is an equal on/off for each dash.
            float dash_count = ceil(0.5 * edge_length / desired_dash_length);
            float dash_length = 0.5 * edge_length / dash_count;
            vClipParams = vec4(edge_offset - 0.5 * dash_length,
                               2.0 * dash_length,
                               dash_length,
                               0.0);
            vClipSelect = 0.0;
            break;
        }
        case BORDER_STYLE_DOTTED: {
            float diameter = border_width;
            float radius = 0.5 * diameter;
            float dot_count = ceil(0.5 * edge_length / diameter);
            float empty_space = edge_length - dot_count * diameter;
            float distance_between_centers = diameter + empty_space / dot_count;
            vClipParams = vec4(edge_offset - radius,
                               distance_between_centers,
                               radius,
                               center_line);
            vClipSelect = 1.0;
            break;
        }
        default:
            vClipParams = vec4(1.0);
            vClipSelect = 0.0;
            break;
    }
}

void main(void) {
    Primitive prim = load_primitive();
    Border border = fetch_border(prim.specific_prim_address);
    int sub_part = prim.user_data0;
    BorderCorners corners = get_border_corners(border, prim.local_rect);
    vec4 color = border.colors[sub_part];

    // TODO(gw): Now that all border styles are supported, the switch
    //           statement below can be tidied up quite a bit.

    float style;
    bool color_flip;

    RectWithSize segment_rect;
    switch (sub_part) {
        case 0: {
            segment_rect.p0 = vec2(corners.tl_outer.x, corners.tl_inner.y);
            segment_rect.size = vec2(border.widths.x, corners.bl_inner.y - corners.tl_inner.y);
            vec4 adjusted_widths = get_effective_border_widths(border, int(border.style.x));
            write_edge_distance(segment_rect.p0.x, border.widths.x, adjusted_widths.x, border.style.x, 0.0, 1.0);
            style = border.style.x;
            color_flip = false;
            write_clip_params(border.style.x,
                              border.widths.x,
                              segment_rect.size.y,
                              segment_rect.p0.y,
                              segment_rect.p0.x + 0.5 * segment_rect.size.x);
            break;
        }
        case 1: {
            segment_rect.p0 = vec2(corners.tl_inner.x, corners.tl_outer.y);
            segment_rect.size = vec2(corners.tr_inner.x - corners.tl_inner.x, border.widths.y);
            vec4 adjusted_widths = get_effective_border_widths(border, int(border.style.y));
            write_edge_distance(segment_rect.p0.y, border.widths.y, adjusted_widths.y, border.style.y, 1.0, 1.0);
            style = border.style.y;
            color_flip = false;
            write_clip_params(border.style.y,
                              border.widths.y,
                              segment_rect.size.x,
                              segment_rect.p0.x,
                              segment_rect.p0.y + 0.5 * segment_rect.size.y);
            break;
        }
        case 2: {
            segment_rect.p0 = vec2(corners.tr_outer.x - border.widths.z, corners.tr_inner.y);
            segment_rect.size = vec2(border.widths.z, corners.br_inner.y - corners.tr_inner.y);
            vec4 adjusted_widths = get_effective_border_widths(border, int(border.style.z));
            write_edge_distance(segment_rect.p0.x, border.widths.z, adjusted_widths.z, border.style.z, 0.0, -1.0);
            style = border.style.z;
            color_flip = true;
            write_clip_params(border.style.z,
                              border.widths.z,
                              segment_rect.size.y,
                              segment_rect.p0.y,
                              segment_rect.p0.x + 0.5 * segment_rect.size.x);
            break;
        }
        case 3: {
            segment_rect.p0 = vec2(corners.bl_inner.x, corners.bl_outer.y - border.widths.w);
            segment_rect.size = vec2(corners.br_inner.x - corners.bl_inner.x, border.widths.w);
            vec4 adjusted_widths = get_effective_border_widths(border, int(border.style.w));
            write_edge_distance(segment_rect.p0.y, border.widths.w, adjusted_widths.w, border.style.w, 1.0, -1.0);
            style = border.style.w;
            color_flip = true;
            write_clip_params(border.style.w,
                              border.widths.w,
                              segment_rect.size.x,
                              segment_rect.p0.x,
                              segment_rect.p0.y + 0.5 * segment_rect.size.y);
            break;
        }
    }

    write_alpha_select(style);
    write_color(color, style, color_flip);

#ifdef WR_FEATURE_TRANSFORM
    TransformVertexInfo vi = write_transform_vertex(segment_rect,
                                                    prim.local_clip_rect,
                                                    prim.z,
                                                    prim.layer,
                                                    prim.task,
                                                    prim.local_rect);
#else
    VertexInfo vi = write_vertex(segment_rect,
                                 prim.local_clip_rect,
                                 prim.z,
                                 prim.layer,
                                 prim.task,
                                 prim.local_rect);
#endif

    vLocalPos = vi.local_pos;
    write_clip(vi.screen_pos, prim.clip_area);
}
