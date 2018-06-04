/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

#ifdef WR_VERTEX_SHADER

struct Border {
    vec4 style;
    vec4 widths;
    vec4 colors[4];
    vec4 radii[2];
};

struct BorderCorners {
    vec2 tl_outer;
    vec2 tl_inner;
    vec2 tr_outer;
    vec2 tr_inner;
    vec2 br_outer;
    vec2 br_inner;
    vec2 bl_outer;
    vec2 bl_inner;
};

vec4 get_effective_border_widths(Border border, int style) {
    switch (style) {
        case BORDER_STYLE_DOUBLE:
            // Calculate the width of a border segment in a style: double
            // border. Round to the nearest CSS pixel.

            // The CSS spec doesn't define what width each of the segments
            // in a style: double border should be. It only says that the
            // sum of the segments should be equal to the total border
            // width. We pick to make the segments (almost) equal thirds
            // for now - we can adjust this if we find other browsers pick
            // different values in some cases.
            // SEE: https://drafts.csswg.org/css-backgrounds-3/#double
            return max(floor(0.5 + border.widths / 3.0), 1.0);
        case BORDER_STYLE_GROOVE:
        case BORDER_STYLE_RIDGE:
            return floor(0.5 + border.widths * 0.5);
        default:
            return border.widths;
    }
}

Border fetch_border(int address) {
    vec4 data[8] = fetch_from_resource_cache_8(address);
    return Border(data[0], data[1],
                  vec4[4](data[2], data[3], data[4], data[5]),
                  vec4[2](data[6], data[7]));
}

BorderCorners get_border_corners(Border border, RectWithSize local_rect) {
    vec2 tl_outer = local_rect.p0;
    vec2 tl_inner = tl_outer + vec2(max(border.radii[0].x, border.widths.x),
                                    max(border.radii[0].y, border.widths.y));

    vec2 tr_outer = vec2(local_rect.p0.x + local_rect.size.x,
                         local_rect.p0.y);
    vec2 tr_inner = tr_outer + vec2(-max(border.radii[0].z, border.widths.z),
                                    max(border.radii[0].w, border.widths.y));

    vec2 br_outer = vec2(local_rect.p0.x + local_rect.size.x,
                         local_rect.p0.y + local_rect.size.y);
    vec2 br_inner = br_outer - vec2(max(border.radii[1].x, border.widths.z),
                                    max(border.radii[1].y, border.widths.w));

    vec2 bl_outer = vec2(local_rect.p0.x,
                         local_rect.p0.y + local_rect.size.y);
    vec2 bl_inner = bl_outer + vec2(max(border.radii[1].z, border.widths.x),
                                    -max(border.radii[1].w, border.widths.w));

    return BorderCorners(
        tl_outer,
        tl_inner,
        tr_outer,
        tr_inner,
        br_outer,
        br_inner,
        bl_outer,
        bl_inner
    );
}

#endif
