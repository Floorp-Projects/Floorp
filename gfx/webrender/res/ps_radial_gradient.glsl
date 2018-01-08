/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include shared,prim_shared

flat varying int vGradientAddress;
flat varying float vGradientRepeat;

flat varying vec2 vStartCenter;
flat varying vec2 vEndCenter;
flat varying float vStartRadius;
flat varying float vEndRadius;

flat varying vec2 vTileSize;
flat varying vec2 vTileRepeat;

varying vec2 vPos;

#ifdef WR_VERTEX_SHADER
void main(void) {
    Primitive prim = load_primitive();
    RadialGradient gradient = fetch_radial_gradient(prim.specific_prim_address);

    VertexInfo vi = write_vertex(prim.local_rect,
                                 prim.local_clip_rect,
                                 prim.z,
                                 prim.scroll_node,
                                 prim.task,
                                 prim.local_rect);

    vPos = vi.local_pos - prim.local_rect.p0;

    vStartCenter = gradient.start_end_center.xy;
    vEndCenter = gradient.start_end_center.zw;

    vStartRadius = gradient.start_end_radius_ratio_xy_extend_mode.x;
    vEndRadius = gradient.start_end_radius_ratio_xy_extend_mode.y;

    vTileSize = gradient.tile_size_repeat.xy;
    vTileRepeat = gradient.tile_size_repeat.zw;

    // Transform all coordinates by the y scale so the
    // fragment shader can work with circles
    float ratio_xy = gradient.start_end_radius_ratio_xy_extend_mode.z;
    vPos.y *= ratio_xy;
    vStartCenter.y *= ratio_xy;
    vEndCenter.y *= ratio_xy;
    vTileSize.y *= ratio_xy;
    vTileRepeat.y *= ratio_xy;

    vGradientAddress = prim.specific_prim_address + VECS_PER_GRADIENT;

    // Whether to repeat the gradient instead of clamping.
    vGradientRepeat = float(int(gradient.start_end_radius_ratio_xy_extend_mode.w) != EXTEND_MODE_CLAMP);

    write_clip(vi.screen_pos, prim.clip_area);
}
#endif

#ifdef WR_FRAGMENT_SHADER
void main(void) {
    vec2 pos = mod(vPos, vTileRepeat);

    if (pos.x >= vTileSize.x ||
        pos.y >= vTileSize.y) {
        discard;
    }

    vec2 cd = vEndCenter - vStartCenter;
    vec2 pd = pos - vStartCenter;
    float rd = vEndRadius - vStartRadius;

    // Solve for t in length(t * cd - pd) = vStartRadius + t * rd
    // using a quadratic equation in form of At^2 - 2Bt + C = 0
    float A = dot(cd, cd) - rd * rd;
    float B = dot(pd, cd) + vStartRadius * rd;
    float C = dot(pd, pd) - vStartRadius * vStartRadius;

    float offset;
    if (A == 0.0) {
        // Since A is 0, just solve for -2Bt + C = 0
        if (B == 0.0) {
            discard;
        }
        float t = 0.5 * C / B;
        if (vStartRadius + rd * t >= 0.0) {
            offset = t;
        } else {
            discard;
        }
    } else {
        float discr = B * B - A * C;
        if (discr < 0.0) {
            discard;
        }
        discr = sqrt(discr);
        float t0 = (B + discr) / A;
        float t1 = (B - discr) / A;
        if (vStartRadius + rd * t0 >= 0.0) {
            offset = t0;
        } else if (vStartRadius + rd * t1 >= 0.0) {
            offset = t1;
        } else {
            discard;
        }
    }

    vec4 color = sample_gradient(vGradientAddress,
                                 offset,
                                 vGradientRepeat);

    oFragColor = color * do_clip();
}
#endif
