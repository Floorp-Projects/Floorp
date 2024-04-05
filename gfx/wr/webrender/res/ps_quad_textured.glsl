/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/// This shader renders solid colors or simple images in a color or alpha target.

#include ps_quad

#ifdef WR_VERTEX_SHADER
void pattern_vertex(PrimitiveInfo info) {
    if ((info.quad_flags & QF_SAMPLE_AS_MASK) != 0) {
        v_flags.z = 1;
    } else {
        v_flags.z = 0;
    }
}
#endif

#ifdef WR_FRAGMENT_SHADER
vec4 pattern_fragment(vec4 color) {
    if (v_flags.y != 0) {
        vec2 uv = clamp(v_uv, v_uv_sample_bounds.xy, v_uv_sample_bounds.zw);
        vec4 texel = TEX_SAMPLE(sColor0, uv);
        if (v_flags.z != 0) {
            texel = texel.rrrr;
        }
        color *= texel;
    }

    return color;
}

#if defined(SWGL_DRAW_SPAN)
void swgl_drawSpanRGBA8() {
    if (v_flags.y != 0) {
        if (v_flags.z != 0) {
            // Fall back to fragment shader as we don't specialize for mask yet. Perhaps
            // we can use an existing swgl commit or add a new one though?
        } else {
            swgl_commitTextureLinearColorRGBA8(sColor0, v_uv, v_uv_sample_bounds, v_color);
        }
    } else {
        swgl_commitSolidRGBA8(v_color);
    }
}
#endif

#endif
