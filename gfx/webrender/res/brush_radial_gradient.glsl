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
flat varying vec2 vRepeatedSize;

#ifdef WR_FEATURE_ALPHA_PASS
varying vec2 vLocalPos;
flat varying vec2 vTileRepeat;
#endif

#ifdef WR_VERTEX_SHADER

struct RadialGradient {
    vec4 center_start_end_radius;
    float ratio_xy;
    int extend_mode;
    vec2 stretch_size;
};

RadialGradient fetch_radial_gradient(int address) {
    vec4 data[2] = fetch_from_resource_cache_2(address);
    return RadialGradient(
        data[0],
        data[1].x,
        int(data[1].y),
        data[1].zw
    );
}

void brush_vs(
    VertexInfo vi,
    int prim_address,
    RectWithSize local_rect,
    RectWithSize segment_rect,
    ivec3 user_data,
    mat4 transform,
    PictureTask pic_task,
    int brush_flags,
    vec4 texel_rect
) {
    RadialGradient gradient = fetch_radial_gradient(prim_address);

    if ((brush_flags & BRUSH_FLAG_SEGMENT_RELATIVE) != 0) {
        vPos = (vi.local_pos - segment_rect.p0) / segment_rect.size;
        vPos = vPos * (texel_rect.zw - texel_rect.xy) + texel_rect.xy;
    } else {
        vPos = vi.local_pos - local_rect.p0;
    }

    vCenter = gradient.center_start_end_radius.xy;
    vStartRadius = gradient.center_start_end_radius.z;
    vEndRadius = gradient.center_start_end_radius.w;

    // Transform all coordinates by the y scale so the
    // fragment shader can work with circles
    vec2 tile_repeat = local_rect.size / gradient.stretch_size;
    vPos.y *= gradient.ratio_xy;
    vCenter.y *= gradient.ratio_xy;
    vRepeatedSize = gradient.stretch_size;
    vRepeatedSize.y *=  gradient.ratio_xy;

    vGradientAddress = user_data.x;

    // Whether to repeat the gradient instead of clamping.
    vGradientRepeat = float(gradient.extend_mode != EXTEND_MODE_CLAMP);

#ifdef WR_FEATURE_ALPHA_PASS
    vTileRepeat = tile_repeat.xy;
    vLocalPos = vi.local_pos;
#endif
}
#endif

#ifdef WR_FRAGMENT_SHADER
Fragment brush_fs() {

#ifdef WR_FEATURE_ALPHA_PASS
    // Handle top and left inflated edges (see brush_image).
    vec2 local_pos = max(vPos, vec2(0.0));

    // Apply potential horizontal and vertical repetitions.
    vec2 pos = mod(local_pos, vRepeatedSize);

    vec2 prim_size = vRepeatedSize * vTileRepeat;
    // Handle bottom and right inflated edges (see brush_image).
    if (local_pos.x >= prim_size.x) {
        pos.x = vRepeatedSize.x;
    }
    if (local_pos.y >= prim_size.y) {
        pos.y = vRepeatedSize.y;
    }
#else
    // Apply potential horizontal and vertical repetitions.
    vec2 pos = mod(vPos, vRepeatedSize);
#endif

    vec2 pd = pos - vCenter;
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

    return Fragment(color);
}
#endif
