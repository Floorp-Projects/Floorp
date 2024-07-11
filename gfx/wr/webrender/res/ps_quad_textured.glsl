/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/// This shader renders solid colors or simple images in a color or alpha target.

#include ps_quad,sample_color0

#define v_flags_textured v_flags.x

#ifdef WR_VERTEX_SHADER

void pattern_vertex(PrimitiveInfo info) {
    // Note: Since the uv rect is passed via segments, This shader cannot sample from a
    // texture if no segments are provided
    if (info.segment.uv_rect.p0 != info.segment.uv_rect.p1) {
        // Textured
        v_flags_textured = 1;
        // TODO: Ideally we would unconditionally modulate the texture with the provided
        // base color, however we are currently getting glitches on Adreno GPUs on Windows
        // if the base color is set to white for composite primitives. While we figure this
        // out, v_color is forced to white here in the textured case, which restores the
        // behavior from before the patch that introduced the glitches.
        // See comment in `add_composite_prim`.
        v_color = vec4(1.0);

        vec2 f = (info.local_pos - info.segment.rect.p0) / rect_size(info.segment.rect);
        vs_init_sample_color0(f, info.segment.uv_rect);
    } else {
        // Solid color
        v_flags_textured = 0;
    }
}

#endif

#ifdef WR_FRAGMENT_SHADER

vec4 pattern_fragment(vec4 color) {
    if (v_flags_textured != 0) {
        vec4 texel = fs_sample_color0();
        color *= texel;
    }

    return color;
}

#if defined(SWGL_DRAW_SPAN)
void swgl_drawSpanRGBA8() {
    if (v_flags_textured != 0) {
        if (v_flags_is_mask != 0) {
            // Fall back to fragment shader as we don't specialize for mask yet. Perhaps
            // we can use an existing swgl commit or add a new one though?
        } else {
            swgl_commitTextureLinearColorRGBA8(sColor0, v_uv0, v_uv0_sample_bounds, v_color);
        }
    } else {
        swgl_commitSolidRGBA8(v_color);
    }
}
#endif

#endif
