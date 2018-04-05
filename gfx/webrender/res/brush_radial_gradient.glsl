/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define VECS_PER_SPECIFIC_BRUSH 2

#include shared,prim_shared,brush

flat varying int vGradientAddress;
flat varying float vGradientRepeat;

flat varying vec2 vCenter;
flat varying float vStartRadius;
flat varying float vEndRadius;

varying vec2 vPos;

#ifdef WR_FEATURE_ALPHA_PASS
varying vec2 vLocalPos;
#endif

#ifdef WR_VERTEX_SHADER

struct RadialGradient {
    vec4 center_start_end_radius;
    vec4 ratio_xy_extend_mode;
};

RadialGradient fetch_radial_gradient(int address) {
    vec4 data[2] = fetch_from_resource_cache_2(address);
    return RadialGradient(data[0], data[1]);
}

void brush_vs(
    VertexInfo vi,
    int prim_address,
    RectWithSize local_rect,
    ivec3 user_data,
    mat4 transform,
    PictureTask pic_task
) {
    RadialGradient gradient = fetch_radial_gradient(prim_address);

    vPos = vi.local_pos - local_rect.p0;

    vCenter = gradient.center_start_end_radius.xy;
    vStartRadius = gradient.center_start_end_radius.z;
    vEndRadius = gradient.center_start_end_radius.w;

    // Transform all coordinates by the y scale so the
    // fragment shader can work with circles
    float ratio_xy = gradient.ratio_xy_extend_mode.x;
    vPos.y *= ratio_xy;
    vCenter.y *= ratio_xy;

    vGradientAddress = user_data.x;

    // Whether to repeat the gradient instead of clamping.
    vGradientRepeat = float(int(gradient.ratio_xy_extend_mode.y) != EXTEND_MODE_CLAMP);

#ifdef WR_FEATURE_ALPHA_PASS
    vLocalPos = vi.local_pos;
#endif
}
#endif

#ifdef WR_FRAGMENT_SHADER
vec4 brush_fs() {
    vec2 pd = vPos - vCenter;
    float rd = vEndRadius - vStartRadius;

    // Solve for t in length(t - pd) = vStartRadius + t * rd
    // using a quadratic equation in form of At^2 - 2Bt + C = 0
    float A = -(rd * rd);
    float B = vStartRadius * rd;
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

#ifdef WR_FEATURE_ALPHA_PASS
    color *= init_transform_fs(vLocalPos);
#endif

    return color;
}
#endif
