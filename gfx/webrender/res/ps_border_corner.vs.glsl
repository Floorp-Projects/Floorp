#line 1
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

vec2 get_radii(vec2 radius, vec2 invalid) {
    if (all(greaterThan(radius, vec2(0.0)))) {
        return radius;
    }

    return invalid;
}

void set_radii(float style,
               vec2 radii,
               vec2 widths,
               vec2 adjusted_widths) {
    vRadii0.xy = get_radii(radii, 2.0 * widths);
    vRadii0.zw = get_radii(radii - widths, -widths);

    switch (int(style)) {
        case BORDER_STYLE_RIDGE:
        case BORDER_STYLE_GROOVE:
            vRadii1.xy = radii - adjusted_widths;
            vRadii1.zw = -widths;
            break;
        case BORDER_STYLE_DOUBLE:
            vRadii1.xy = get_radii(radii - adjusted_widths, -widths);
            vRadii1.zw = get_radii(radii - widths + adjusted_widths, -widths);
            break;
        default:
            vRadii1.xy = -widths;
            vRadii1.zw = -widths;
            break;
    }
}

void set_edge_line(vec2 border_width,
                   vec2 outer_corner,
                   vec2 gradient_sign) {
    vec2 gradient = border_width * gradient_sign;
    vColorEdgeLine = vec4(outer_corner, vec2(-gradient.y, gradient.x));
}

void write_color(vec4 color0, vec4 color1, int style, vec2 delta) {
    vec4 modulate;

    switch (style) {
        case BORDER_STYLE_GROOVE:
            modulate = vec4(1.0 - 0.3 * delta.x,
                            1.0 + 0.3 * delta.x,
                            1.0 - 0.3 * delta.y,
                            1.0 + 0.3 * delta.y);

            break;
        case BORDER_STYLE_RIDGE:
            modulate = vec4(1.0 + 0.3 * delta.x,
                            1.0 - 0.3 * delta.x,
                            1.0 + 0.3 * delta.y,
                            1.0 - 0.3 * delta.y);
            break;
        default:
            modulate = vec4(1.0);
            break;
    }

    vColor00 = vec4(color0.rgb * modulate.x, color0.a);
    vColor01 = vec4(color0.rgb * modulate.y, color0.a);
    vColor10 = vec4(color1.rgb * modulate.z, color1.a);
    vColor11 = vec4(color1.rgb * modulate.w, color1.a);
}

void main(void) {
    Primitive prim = load_primitive();
    Border border = fetch_border(prim.prim_index);
    int sub_part = prim.sub_index;
    BorderCorners corners = get_border_corners(border, prim.local_rect);

    vec4 adjusted_widths = get_effective_border_widths(border);
    vec4 inv_adjusted_widths = border.widths - adjusted_widths;
    vec2 p0, p1;

    // TODO(gw): We'll need to pass through multiple styles
    //           once we support style transitions per corner.
    int style;
    vec4 edge_distances;
    vec4 color0, color1;
    vec2 color_delta;

    switch (sub_part) {
        case 0: {
            p0 = corners.tl_outer;
            p1 = corners.tl_inner;
            color0 = border.colors[0];
            color1 = border.colors[1];
            vClipCenter = corners.tl_outer + border.radii[0].xy;
            vClipSign = vec2(1.0);
            set_radii(border.style.x,
                      border.radii[0].xy,
                      border.widths.xy,
                      adjusted_widths.xy);
            set_edge_line(border.widths.xy,
                          corners.tl_outer,
                          vec2(1.0, 1.0));
            style = int(border.style.x);
            edge_distances = vec4(p0 + adjusted_widths.xy,
                                  p0 + inv_adjusted_widths.xy);
            color_delta = vec2(1.0);
            break;
        }
        case 1: {
            p0 = vec2(corners.tr_inner.x, corners.tr_outer.y);
            p1 = vec2(corners.tr_outer.x, corners.tr_inner.y);
            color0 = border.colors[1];
            color1 = border.colors[2];
            vClipCenter = corners.tr_outer + vec2(-border.radii[0].z, border.radii[0].w);
            vClipSign = vec2(-1.0, 1.0);
            set_radii(border.style.y,
                      border.radii[0].zw,
                      border.widths.zy,
                      adjusted_widths.zy);
            set_edge_line(border.widths.zy,
                          corners.tr_outer,
                          vec2(-1.0, 1.0));
            style = int(border.style.y);
            edge_distances = vec4(p1.x - adjusted_widths.z,
                                  p0.y + adjusted_widths.y,
                                  p1.x - border.widths.z + adjusted_widths.z,
                                  p0.y + inv_adjusted_widths.y);
            color_delta = vec2(1.0, -1.0);
            break;
        }
        case 2: {
            p0 = corners.br_inner;
            p1 = corners.br_outer;
            color0 = border.colors[2];
            color1 = border.colors[3];
            vClipCenter = corners.br_outer - border.radii[1].xy;
            vClipSign = vec2(-1.0, -1.0);
            set_radii(border.style.z,
                      border.radii[1].xy,
                      border.widths.zw,
                      adjusted_widths.zw);
            set_edge_line(border.widths.zw,
                          corners.br_outer,
                          vec2(-1.0, -1.0));
            style = int(border.style.z);
            edge_distances = vec4(p1.x - adjusted_widths.z,
                                  p1.y - adjusted_widths.w,
                                  p1.x - border.widths.z + adjusted_widths.z,
                                  p1.y - border.widths.w + adjusted_widths.w);
            color_delta = vec2(-1.0);
            break;
        }
        case 3: {
            p0 = vec2(corners.bl_outer.x, corners.bl_inner.y);
            p1 = vec2(corners.bl_inner.x, corners.bl_outer.y);
            color0 = border.colors[3];
            color1 = border.colors[0];
            vClipCenter = corners.bl_outer + vec2(border.radii[1].z, -border.radii[1].w);
            vClipSign = vec2(1.0, -1.0);
            set_radii(border.style.w,
                      border.radii[1].zw,
                      border.widths.xw,
                      adjusted_widths.xw);
            set_edge_line(border.widths.xw,
                          corners.bl_outer,
                          vec2(1.0, -1.0));
            style = int(border.style.w);
            edge_distances = vec4(p0.x + adjusted_widths.x,
                                  p1.y - adjusted_widths.w,
                                  p0.x + inv_adjusted_widths.x,
                                  p1.y - border.widths.w + adjusted_widths.w);
            color_delta = vec2(-1.0, 1.0);
            break;
        }
    }

    switch (int(style)) {
        case BORDER_STYLE_DOUBLE: {
            vEdgeDistance = edge_distances;
            vAlphaSelect = 0.0;
            vSDFSelect = 0.0;
            break;
        }
        case BORDER_STYLE_GROOVE:
        case BORDER_STYLE_RIDGE:
            vEdgeDistance = vec4(edge_distances.xy, 0.0, 0.0);
            vAlphaSelect = 1.0;
            vSDFSelect = 1.0;
            break;
        default: {
            vEdgeDistance = vec4(0.0);
            vAlphaSelect = 1.0;
            vSDFSelect = 0.0;
            break;
        }
    }

    write_color(color0, color1, style, color_delta);

    RectWithSize segment_rect;
    segment_rect.p0 = p0;
    segment_rect.size = p1 - p0;

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
