/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include ps_quad

#ifndef SWGL_ANTIALIAS
varying highp vec2 vLocalPos;
#endif

#ifdef WR_VERTEX_SHADER
void main(void) {
    PrimitiveInfo info = ps_quad_main();

#ifndef SWGL_ANTIALIAS
    RectWithEndpoint xf_bounds = RectWithEndpoint(
        max(info.local_prim_rect.p0, info.local_clip_rect.p0),
        min(info.local_prim_rect.p1, info.local_clip_rect.p1)
    );
    vTransformBounds = vec4(xf_bounds.p0, xf_bounds.p1);

    vLocalPos = info.local_pos;

    if (info.edge_flags == 0) {
        v_flags.x = 0;
    } else {
        v_flags.x = 1;
    }
#endif

    if ((info.quad_flags & QF_SAMPLE_AS_MASK) != 0) {
        v_flags.z = 1;
    } else {
        v_flags.z = 0;
    }
}
#endif

#ifdef WR_FRAGMENT_SHADER
void main(void) {
    vec4 color = v_color;

#ifndef SWGL_ANTIALIAS
    if (v_flags.x != 0) {
        float alpha = init_transform_fs(vLocalPos);
        color *= alpha;
    }
#endif

    if (v_flags.y != 0) {
        vec2 uv = clamp(v_uv, v_uv_sample_bounds.xy, v_uv_sample_bounds.zw);
        vec4 texel = TEX_SAMPLE(sColor0, uv);
        if (v_flags.z != 0) {
            texel = texel.rrrr;
        }
        color *= texel;
    }

    oFragColor = color;
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
