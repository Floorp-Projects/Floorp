/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include shared,prim_shared,shared_border,ellipse

// Edge color transition
flat varying vec4 vColor00;
flat varying vec4 vColor01;
flat varying vec4 vColor10;
flat varying vec4 vColor11;
flat varying vec4 vColorEdgeLine;

// Border radius
flat varying vec2 vClipCenter;
flat varying vec4 vRadii0;
flat varying vec4 vRadii1;
flat varying vec2 vClipSign;
flat varying vec4 vEdgeDistance;
flat varying float vSDFSelect;

flat varying float vIsBorderRadiusLessThanBorderWidth;

// Border style
flat varying float vAlphaSelect;

varying vec2 vLocalPos;

#ifdef WR_VERTEX_SHADER
// Matches BorderCornerSide enum in border.rs
#define SIDE_BOTH       0
#define SIDE_FIRST      1
#define SIDE_SECOND     2

vec2 get_radii(vec2 radius, vec2 invalid) {
    if (all(greaterThan(radius, vec2(0.0)))) {
        return radius;
    }

    return invalid;
}

void set_radii(int style,
               vec2 radii,
               vec2 widths,
               vec2 adjusted_widths) {
    vRadii0.xy = get_radii(radii, 2.0 * widths);
    vRadii0.zw = get_radii(radii - widths, -widths);

    switch (style) {
        case BORDER_STYLE_RIDGE:
        case BORDER_STYLE_GROOVE:
            vRadii1.xy = radii - adjusted_widths;
            // See comment in default branch
            vRadii1.zw = vec2(-100.0);
            break;
        case BORDER_STYLE_DOUBLE:
            vRadii1.xy = get_radii(radii - adjusted_widths, -widths);
            vRadii1.zw = get_radii(radii - widths + adjusted_widths, -widths);
            break;
        default:
            // These aren't needed, so we set them to some reasonably large
            // negative value so later computations will discard them. This
            // avoids branches and numerical issues in the fragment shader.
            vRadii1.xy = vec2(-100.0);
            vRadii1.zw = vec2(-100.0);
            break;
    }
}

void set_edge_line(vec2 border_width,
                   vec2 outer_corner,
                   vec2 gradient_sign) {
    vec2 gradient = border_width * gradient_sign;
    vColorEdgeLine = vec4(outer_corner, vec2(-gradient.y, gradient.x));
}

void write_color(vec4 color0, vec4 color1, int style, vec2 delta, int instance_kind) {
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

    // Optionally mask out one side of the border corner,
    // depending on the instance kind.
    switch (instance_kind) {
        case SIDE_FIRST:
            color0.a = 0.0;
            break;
        case SIDE_SECOND:
            color1.a = 0.0;
            break;
    }

    vColor00 = vec4(clamp(color0.rgb * modulate.x, vec3(0.0), vec3(color0.a)), color0.a);
    vColor01 = vec4(clamp(color0.rgb * modulate.y, vec3(0.0), vec3(color0.a)), color0.a);
    vColor10 = vec4(clamp(color1.rgb * modulate.z, vec3(0.0), vec3(color1.a)), color1.a);
    vColor11 = vec4(clamp(color1.rgb * modulate.w, vec3(0.0), vec3(color1.a)), color1.a);
}

int select_style(int color_select, vec2 fstyle) {
    ivec2 style = ivec2(fstyle);

    switch (color_select) {
        case SIDE_BOTH:
        {
            // TODO(gw): A temporary hack! While we don't support
            //           border corners that have dots or dashes
            //           with another style, pretend they are solid
            //           border corners.
            bool has_dots = style.x == BORDER_STYLE_DOTTED ||
                            style.y == BORDER_STYLE_DOTTED;
            bool has_dashes = style.x == BORDER_STYLE_DASHED ||
                              style.y == BORDER_STYLE_DASHED;
            if (style.x != style.y && (has_dots || has_dashes))
                return BORDER_STYLE_SOLID;
            return style.x;
        }
        case SIDE_FIRST:
            return style.x;
        case SIDE_SECOND:
            return style.y;
    }
}

void main(void) {
    Primitive prim = load_primitive();
    Border border = fetch_border(prim.specific_prim_address);
    int sub_part = prim.user_data0;
    BorderCorners corners = get_border_corners(border, prim.local_rect);

    vec2 p0, p1;

    // TODO(gw): We'll need to pass through multiple styles
    //           once we support style transitions per corner.
    int style;
    vec4 edge_distances;
    vec4 color0, color1;
    vec2 color_delta;

    // TODO(gw): Now that all border styles are supported, the switch
    //           statement below can be tidied up quite a bit.

    switch (sub_part) {
        case 0: {
            p0 = corners.tl_outer;
            p1 = corners.tl_inner;
            color0 = border.colors[0];
            color1 = border.colors[1];
            vClipCenter = corners.tl_outer + border.radii[0].xy;
            vClipSign = vec2(1.0);
            style = select_style(prim.user_data1, border.style.yx);
            vec4 adjusted_widths = get_effective_border_widths(border, style);
            vec4 inv_adjusted_widths = border.widths - adjusted_widths;
            set_radii(style,
                      border.radii[0].xy,
                      border.widths.xy,
                      adjusted_widths.xy);
            set_edge_line(border.widths.xy,
                          corners.tl_outer,
                          vec2(1.0, 1.0));
            edge_distances = vec4(p0 + adjusted_widths.xy,
                                  p0 + inv_adjusted_widths.xy);
            color_delta = vec2(1.0);
            vIsBorderRadiusLessThanBorderWidth = any(lessThan(border.radii[0].xy,
                                                              border.widths.xy)) ? 1.0 : 0.0;
            break;
        }
        case 1: {
            p0 = vec2(corners.tr_inner.x, corners.tr_outer.y);
            p1 = vec2(corners.tr_outer.x, corners.tr_inner.y);
            color0 = border.colors[1];
            color1 = border.colors[2];
            vClipCenter = corners.tr_outer + vec2(-border.radii[0].z, border.radii[0].w);
            vClipSign = vec2(-1.0, 1.0);
            style = select_style(prim.user_data1, border.style.zy);
            vec4 adjusted_widths = get_effective_border_widths(border, style);
            vec4 inv_adjusted_widths = border.widths - adjusted_widths;
            set_radii(style,
                      border.radii[0].zw,
                      border.widths.zy,
                      adjusted_widths.zy);
            set_edge_line(border.widths.zy,
                          corners.tr_outer,
                          vec2(-1.0, 1.0));
            edge_distances = vec4(p1.x - adjusted_widths.z,
                                  p0.y + adjusted_widths.y,
                                  p1.x - border.widths.z + adjusted_widths.z,
                                  p0.y + inv_adjusted_widths.y);
            color_delta = vec2(1.0, -1.0);
            vIsBorderRadiusLessThanBorderWidth = any(lessThan(border.radii[0].zw,
                                                              border.widths.zy)) ? 1.0 : 0.0;
            break;
        }
        case 2: {
            p0 = corners.br_inner;
            p1 = corners.br_outer;
            color0 = border.colors[2];
            color1 = border.colors[3];
            vClipCenter = corners.br_outer - border.radii[1].xy;
            vClipSign = vec2(-1.0, -1.0);
            style = select_style(prim.user_data1, border.style.wz);
            vec4 adjusted_widths = get_effective_border_widths(border, style);
            vec4 inv_adjusted_widths = border.widths - adjusted_widths;
            set_radii(style,
                      border.radii[1].xy,
                      border.widths.zw,
                      adjusted_widths.zw);
            set_edge_line(border.widths.zw,
                          corners.br_outer,
                          vec2(-1.0, -1.0));
            edge_distances = vec4(p1.x - adjusted_widths.z,
                                  p1.y - adjusted_widths.w,
                                  p1.x - border.widths.z + adjusted_widths.z,
                                  p1.y - border.widths.w + adjusted_widths.w);
            color_delta = vec2(-1.0);
            vIsBorderRadiusLessThanBorderWidth = any(lessThan(border.radii[1].xy,
                                                              border.widths.zw)) ? 1.0 : 0.0;
            break;
        }
        case 3: {
            p0 = vec2(corners.bl_outer.x, corners.bl_inner.y);
            p1 = vec2(corners.bl_inner.x, corners.bl_outer.y);
            color0 = border.colors[3];
            color1 = border.colors[0];
            vClipCenter = corners.bl_outer + vec2(border.radii[1].z, -border.radii[1].w);
            vClipSign = vec2(1.0, -1.0);
            style = select_style(prim.user_data1, border.style.xw);
            vec4 adjusted_widths = get_effective_border_widths(border, style);
            vec4 inv_adjusted_widths = border.widths - adjusted_widths;
            set_radii(style,
                      border.radii[1].zw,
                      border.widths.xw,
                      adjusted_widths.xw);
            set_edge_line(border.widths.xw,
                          corners.bl_outer,
                          vec2(1.0, -1.0));
            edge_distances = vec4(p0.x + adjusted_widths.x,
                                  p1.y - adjusted_widths.w,
                                  p0.x + inv_adjusted_widths.x,
                                  p1.y - border.widths.w + adjusted_widths.w);
            color_delta = vec2(-1.0, 1.0);
            vIsBorderRadiusLessThanBorderWidth = any(lessThan(border.radii[1].zw,
                                                              border.widths.xw)) ? 1.0 : 0.0;
            break;
        }
    }

    switch (style) {
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
        case BORDER_STYLE_DOTTED:
            // Disable normal clip radii for dotted corners, since
            // all the clipping is handled by the clip mask.
            vClipSign = vec2(0.0);
            vEdgeDistance = vec4(0.0);
            vAlphaSelect = 1.0;
            vSDFSelect = 0.0;
            break;
        default: {
            vEdgeDistance = vec4(0.0);
            vAlphaSelect = 1.0;
            vSDFSelect = 0.0;
            break;
        }
    }

    write_color(color0, color1, style, color_delta, prim.user_data1);

    RectWithSize segment_rect;
    segment_rect.p0 = p0;
    segment_rect.size = p1 - p0;

#ifdef WR_FEATURE_TRANSFORM
    VertexInfo vi = write_transform_vertex(segment_rect,
                                           prim.local_rect,
                                           prim.local_clip_rect,
                                           vec4(1.0),
                                           prim.z,
                                           prim.layer,
                                           prim.task);
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
#endif

#ifdef WR_FRAGMENT_SHADER
void main(void) {
    float alpha = 1.0;
#ifdef WR_FEATURE_TRANSFORM
    alpha = init_transform_fs(vLocalPos);
#endif

    alpha *= do_clip();

    float aa_range = compute_aa_range(vLocalPos);

    float distance_for_color;
    float color_mix_factor;

    // Only apply the clip AA if inside the clip region. This is
    // necessary for correctness when the border width is greater
    // than the border radius.
    if (vIsBorderRadiusLessThanBorderWidth == 0.0 ||
        all(lessThan(vLocalPos * vClipSign, vClipCenter * vClipSign))) {
        vec2 p = vLocalPos - vClipCenter;

        // The coordinate system is snapped to pixel boundaries. To sample the distance,
        // however, we are interested in the center of the pixels which introduces an
        // error of half a pixel towards the exterior of the curve (See issue #1750).
        // This error is corrected by offsetting the distance by half a device pixel.
        // This not entirely correct: it leaves an error that varries between
        // 0 and (sqrt(2) - 1)/2 = 0.2 pixels but it is hardly noticeable and is better
        // than the constant sqrt(2)/2 px error without the correction.
        // To correct this exactly we would need to offset p by half a pixel in the
        // direction of the center of the ellipse (a different offset for each corner).

        // Get signed distance from the inner/outer clips.
        float d0 = distance_to_ellipse(p, vRadii0.xy, aa_range);
        float d1 = distance_to_ellipse(p, vRadii0.zw, aa_range);
        float d2 = distance_to_ellipse(p, vRadii1.xy, aa_range);
        float d3 = distance_to_ellipse(p, vRadii1.zw, aa_range);

        // SDF subtract main radii
        float d_main = max(d0, -d1);

        // SDF subtract inner radii (double style borders)
        float d_inner = max(d2, -d3);

        // Select how to combine the SDF based on border style.
        float d = mix(max(d_main, -d_inner), d_main, vSDFSelect);

        // Only apply AA to fragments outside the signed distance field.
        alpha = min(alpha, distance_aa(aa_range, d));

        // Get the groove/ridge mix factor.
        color_mix_factor = distance_aa(aa_range, d2);
    } else {
        // Handle the case where the fragment is outside the clip
        // region in a corner. This occurs when border width is
        // greater than border radius.

        // Get linear distances along horizontal and vertical edges.
        vec2 d0 = vClipSign.xx * (vLocalPos.xx - vEdgeDistance.xz);
        vec2 d1 = vClipSign.yy * (vLocalPos.yy - vEdgeDistance.yw);
        // Apply union to get the outer edge signed distance.
        float da = min(d0.x, d1.x);
        // Apply intersection to get the inner edge signed distance.
        float db = max(-d0.y, -d1.y);
        // Apply union to get both edges.
        float d = min(da, db);
        // Select fragment on/off based on signed distance.
        // No AA here, since we know we're on a straight edge
        // and the width is rounded to a whole CSS pixel.
        alpha = min(alpha, mix(vAlphaSelect, 1.0, d < 0.0));

        // Get the groove/ridge mix factor.
        // TODO(gw): Support AA for groove/ridge border edge with transforms.
        color_mix_factor = mix(0.0, 1.0, da > 0.0);
    }

    // Mix inner/outer color.
    vec4 color0 = mix(vColor00, vColor01, color_mix_factor);
    vec4 color1 = mix(vColor10, vColor11, color_mix_factor);

    // Select color based on side of line. Get distance from the
    // reference line, and then apply AA along the edge.
    float ld = distance_to_line(vColorEdgeLine.xy, vColorEdgeLine.zw, vLocalPos);
    float m = distance_aa(aa_range, -ld);
    vec4 color = mix(color0, color1, m);

    oFragColor = color * alpha;
}
#endif
