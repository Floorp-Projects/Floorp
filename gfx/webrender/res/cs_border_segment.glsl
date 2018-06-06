/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include shared,ellipse

// For edges, the colors are the same. For corners, these
// are the colors of each edge making up the corner.
flat varying vec4 vColor0[2];
flat varying vec4 vColor1[2];

// A point + tangent defining the line where the edge
// transition occurs. Used for corners only.
flat varying vec4 vColorLine;

// x = segment, y = styles, z = edge axes, w = clip mode
flat varying ivec4 vConfig;

// xy = Local space position of the clip center.
// zw = Scale the rect origin by this to get the outer
// corner from the segment rectangle.
flat varying vec4 vClipCenter_Sign;

// An outer and inner elliptical radii for border
// corner clipping.
flat varying vec4 vClipRadii;

// Reference point for determine edge clip lines.
flat varying vec4 vEdgeReference;

// Stores widths/2 and widths/3 to save doing this in FS.
flat varying vec4 vPartialWidths;

// Clipping parameters for dot or dash.
flat varying vec4 vClipParams1;
flat varying vec4 vClipParams2;

// Local space position
varying vec2 vPos;

#define SEGMENT_TOP_LEFT        0
#define SEGMENT_TOP_RIGHT       1
#define SEGMENT_BOTTOM_RIGHT    2
#define SEGMENT_BOTTOM_LEFT     3
#define SEGMENT_LEFT            4
#define SEGMENT_TOP             5
#define SEGMENT_RIGHT           6
#define SEGMENT_BOTTOM          7

// Border styles as defined in webrender_api/types.rs
#define BORDER_STYLE_NONE         0
#define BORDER_STYLE_SOLID        1
#define BORDER_STYLE_DOUBLE       2
#define BORDER_STYLE_DOTTED       3
#define BORDER_STYLE_DASHED       4
#define BORDER_STYLE_HIDDEN       5
#define BORDER_STYLE_GROOVE       6
#define BORDER_STYLE_RIDGE        7
#define BORDER_STYLE_INSET        8
#define BORDER_STYLE_OUTSET       9

#define CLIP_NONE   0
#define CLIP_DASH   1
#define CLIP_DOT    2

#ifdef WR_VERTEX_SHADER

in vec2 aTaskOrigin;
in vec4 aRect;
in vec4 aColor0;
in vec4 aColor1;
in int aFlags;
in vec2 aWidths;
in vec2 aRadii;
in vec4 aClipParams1;
in vec4 aClipParams2;

vec2 get_outer_corner_scale(int segment) {
    vec2 p;

    switch (segment) {
        case SEGMENT_TOP_LEFT:
            p = vec2(0.0, 0.0);
            break;
        case SEGMENT_TOP_RIGHT:
            p = vec2(1.0, 0.0);
            break;
        case SEGMENT_BOTTOM_RIGHT:
            p = vec2(1.0, 1.0);
            break;
        case SEGMENT_BOTTOM_LEFT:
            p = vec2(0.0, 1.0);
            break;
        default:
            // Should never get hit
            p = vec2(0.0);
            break;
    }

    return p;
}

vec4 mod_color(vec4 color, float f) {
    return vec4(clamp(color.rgb * f, vec3(0.0), vec3(color.a)), color.a);
}

vec4[2] get_colors_for_side(vec4 color, int style) {
    vec4 result[2];
    const vec2 f = vec2(1.3, 0.7);

    switch (style) {
        case BORDER_STYLE_GROOVE:
            result[0] = mod_color(color, f.x);
            result[1] = mod_color(color, f.y);
            break;
        case BORDER_STYLE_RIDGE:
            result[0] = mod_color(color, f.y);
            result[1] = mod_color(color, f.x);
            break;
        default:
            result[0] = color;
            result[1] = color;
            break;
    }

    return result;
}

void main(void) {
    int segment = aFlags & 0xff;
    int style0 = (aFlags >> 8) & 0xff;
    int style1 = (aFlags >> 16) & 0xff;
    int clip_mode = (aFlags >> 24) & 0xff;

    vec2 outer_scale = get_outer_corner_scale(segment);
    vec2 outer = outer_scale * aRect.zw;
    vec2 clip_sign = 1.0 - 2.0 * outer_scale;

    // Set some flags used by the FS to determine the
    // orientation of the two edges in this corner.
    ivec2 edge_axis;
    // Derive the positions for the edge clips, which must be handled
    // differently between corners and edges.
    vec2 edge_reference;
    switch (segment) {
        case SEGMENT_TOP_LEFT:
            edge_axis = ivec2(0, 1);
            edge_reference = outer;
            break;
        case SEGMENT_TOP_RIGHT:
            edge_axis = ivec2(1, 0);
            edge_reference = vec2(outer.x - aWidths.x, outer.y);
            break;
        case SEGMENT_BOTTOM_RIGHT:
            edge_axis = ivec2(0, 1);
            edge_reference = outer - aWidths;
            break;
        case SEGMENT_BOTTOM_LEFT:
            edge_axis = ivec2(1, 0);
            edge_reference = vec2(outer.x, outer.y - aWidths.y);
            break;
        case SEGMENT_TOP:
        case SEGMENT_BOTTOM:
            edge_axis = ivec2(1, 1);
            edge_reference = vec2(0.0);
            break;
        case SEGMENT_LEFT:
        case SEGMENT_RIGHT:
        default:
            edge_axis = ivec2(0, 0);
            edge_reference = vec2(0.0);
            break;
    }

    vConfig = ivec4(
        segment,
        style0 | (style1 << 16),
        edge_axis.x | (edge_axis.y << 16),
        clip_mode
    );
    vPartialWidths = vec4(aWidths / 3.0, aWidths / 2.0);
    vPos = aRect.zw * aPosition.xy;

    vColor0 = get_colors_for_side(aColor0, style0);
    vColor1 = get_colors_for_side(aColor1, style1);
    vClipCenter_Sign = vec4(outer + clip_sign * aRadii, clip_sign);
    vClipRadii = vec4(aRadii, max(aRadii - aWidths, 0.0));
    vColorLine = vec4(outer, aWidths.y * -clip_sign.y, aWidths.x * clip_sign.x);
    vEdgeReference = vec4(edge_reference, edge_reference + aWidths);
    vClipParams1 = aClipParams1;
    vClipParams2 = aClipParams2;

    // For the case of dot clips, optimize the number of pixels that
    // are hit to just include the dot itself.
    // TODO(gw): We should do something similar in the future for
    //           dash clips!
    if (clip_mode == CLIP_DOT) {
        // Expand by a small amount to allow room for AA around
        // the dot.
        float expanded_radius = aClipParams1.z + 2.0;
        vPos = vClipParams1.xy + expanded_radius * (2.0 * aPosition.xy - 1.0);
        vPos = clamp(vPos, vec2(0.0), aRect.zw);
    }

    gl_Position = uTransform * vec4(aTaskOrigin + aRect.xy + vPos, 0.0, 1.0);
}
#endif

#ifdef WR_FRAGMENT_SHADER
vec4 evaluate_color_for_style_in_corner(
    vec2 clip_relative_pos,
    int style,
    vec4 color[2],
    vec4 clip_radii,
    float mix_factor,
    int segment,
    float aa_range
) {
    switch (style) {
        case BORDER_STYLE_DOUBLE: {
            // Get the distances from 0.33 of the radii, and
            // also 0.67 of the radii. Use these to form a
            // SDF subtraction which will clip out the inside
            // third of the rounded edge.
            float d_radii_a = distance_to_ellipse(
                clip_relative_pos,
                clip_radii.xy - vPartialWidths.xy,
                aa_range
            );
            float d_radii_b = distance_to_ellipse(
                clip_relative_pos,
                clip_radii.xy - 2.0 * vPartialWidths.xy,
                aa_range
            );
            float d = min(-d_radii_a, d_radii_b);
            float alpha = distance_aa(aa_range, d);
            return alpha * color[0];
        }
        case BORDER_STYLE_GROOVE:
        case BORDER_STYLE_RIDGE: {
            float d = distance_to_ellipse(
                clip_relative_pos,
                clip_radii.xy - vPartialWidths.zw,
                aa_range
            );
            float alpha = distance_aa(aa_range, d);
            float swizzled_factor;
            switch (segment) {
                case SEGMENT_TOP_LEFT: swizzled_factor = 0.0; break;
                case SEGMENT_TOP_RIGHT: swizzled_factor = mix_factor; break;
                case SEGMENT_BOTTOM_RIGHT: swizzled_factor = 1.0; break;
                case SEGMENT_BOTTOM_LEFT: swizzled_factor = 1.0 - mix_factor; break;
                default: swizzled_factor = 0.0; break;
            };
            vec4 c0 = mix(color[1], color[0], swizzled_factor);
            vec4 c1 = mix(color[0], color[1], swizzled_factor);
            return mix(c0, c1, alpha);
        }
        default:
            break;
    }

    return color[0];
}

vec4 evaluate_color_for_style_in_edge(
    vec2 pos,
    int style,
    vec4 color[2],
    float aa_range,
    int edge_axis
) {
    switch (style) {
        case BORDER_STYLE_DOUBLE: {
            float d0 = -1.0;
            float d1 = -1.0;
            if (vPartialWidths[edge_axis] > 1.0) {
                vec2 ref = vec2(
                    vEdgeReference[edge_axis] + vPartialWidths[edge_axis],
                    vEdgeReference[edge_axis+2] - vPartialWidths[edge_axis]
                );
                d0 = pos[edge_axis] - ref.x;
                d1 = ref.y - pos[edge_axis];
            }
            float d = min(d0, d1);
            float alpha = distance_aa(aa_range, d);
            return alpha * color[0];
        }
        case BORDER_STYLE_GROOVE:
        case BORDER_STYLE_RIDGE: {
            float ref = vEdgeReference[edge_axis] + vPartialWidths[edge_axis+2];
            float d = pos[edge_axis] - ref;
            float alpha = distance_aa(aa_range, d);
            return mix(color[0], color[1], alpha);
        }
        default:
            break;
    }

    return color[0];
}

void main(void) {
    float aa_range = compute_aa_range(vPos);
    vec4 color0, color1;

    int segment = vConfig.x;
    ivec2 style = ivec2(vConfig.y & 0xffff, vConfig.y >> 16);
    ivec2 edge_axis = ivec2(vConfig.z & 0xffff, vConfig.z >> 16);
    int clip_mode = vConfig.w;

    float mix_factor = 0.0;
    if (edge_axis.x != edge_axis.y) {
        float d_line = distance_to_line(vColorLine.xy, vColorLine.zw, vPos);
        mix_factor = distance_aa(aa_range, -d_line);
    }

    // Check if inside corner clip-region
    vec2 clip_relative_pos = vPos - vClipCenter_Sign.xy;
    bool in_clip_region = all(lessThan(vClipCenter_Sign.zw * clip_relative_pos, vec2(0.0)));
    float d = -1.0;

    switch (clip_mode) {
        case CLIP_DOT: {
            // Set clip distance based or dot position and radius.
            d = distance(vClipParams1.xy, vPos) - vClipParams1.z;
            break;
        }
        case CLIP_DASH: {
            // Get SDF for the two line/tangent clip lines,
            // do SDF subtract to get clip distance.
            float d0 = distance_to_line(vClipParams1.xy,
                                        vClipParams1.zw,
                                        vPos);
            float d1 = distance_to_line(vClipParams2.xy,
                                        vClipParams2.zw,
                                        vPos);
            d = max(d0, -d1);
            break;
        }
        case CLIP_NONE:
        default:
            break;
    }

    if (in_clip_region) {
        float d_radii_a = distance_to_ellipse(clip_relative_pos, vClipRadii.xy, aa_range);
        float d_radii_b = distance_to_ellipse(clip_relative_pos, vClipRadii.zw, aa_range);
        float d_radii = max(d_radii_a, -d_radii_b);
        d = max(d, d_radii);

        color0 = evaluate_color_for_style_in_corner(
            clip_relative_pos,
            style.x,
            vColor0,
            vClipRadii,
            mix_factor,
            segment,
            aa_range
        );
        color1 = evaluate_color_for_style_in_corner(
            clip_relative_pos,
            style.y,
            vColor1,
            vClipRadii,
            mix_factor,
            segment,
            aa_range
        );
    } else {
        color0 = evaluate_color_for_style_in_edge(
            vPos,
            style.x,
            vColor0,
            aa_range,
            edge_axis.x
        );
        color1 = evaluate_color_for_style_in_edge(
            vPos,
            style.y,
            vColor1,
            aa_range,
            edge_axis.y
        );
    }

    float alpha = distance_aa(aa_range, d);
    vec4 color = mix(color0, color1, mix_factor);
    oFragColor = color * alpha;
}
#endif
