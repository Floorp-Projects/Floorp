/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/// This shader renders radial graidents in a color or alpha target.

#include ps_quad,gradient

// Start radius. Packed in to a vector to work around bug 1630356.
flat varying highp vec2 v_start_radius;
varying highp vec2 v_pos;

struct RadialGradient {
    vec2 center;
    vec2 scale;
    float start_radius;
    float end_radius;
    float xy_ratio;
    // 1.0 if the gradient should be repeated, 0.0 otherwise.
    float repeat;
};

RadialGradient fetch_radial_gradient(int address) {
    vec4[2] data = fetch_from_gpu_buffer_2f(address);

    return RadialGradient(
        data[0].xy,
        data[0].zw,
        data[1].x,
        data[1].y,
        data[1].z,
        data[1].w
    );
}

#ifdef WR_VERTEX_SHADER
void pattern_vertex(PrimitiveInfo info) {
    RadialGradient gradient = fetch_radial_gradient(info.pattern_input.x);
    v_gradient_address.x = info.pattern_input.y;

    // Store 1/rd where rd = end_radius - start_radius
    // If rd = 0, we can't get its reciprocal. Instead, just use a zero scale.
    float rd = gradient.end_radius - gradient.start_radius;
    float radius_scale = rd != 0.0 ? 1.0 / rd : 0.0;

    v_start_radius.x = gradient.start_radius * radius_scale;

    // Transform all coordinates by the y scale so the
    // fragment shader can work with circles

    // v_pos is in a coordinate space relative to the task rect
    // (so it is independent of the task origin).
    v_pos = ((info.local_pos - info.local_prim_rect.p0) * gradient.scale - gradient.center) * radius_scale;
    v_pos.y *= gradient.xy_ratio;

    v_gradient_repeat.x = gradient.repeat;
}
#endif

#ifdef WR_FRAGMENT_SHADER
vec4 pattern_fragment(vec4 color) {
    // Solve for t in length(pd) = v_start_radius + t * rd
    float offset = length(v_pos) - v_start_radius.x;
    color *= sample_gradient(offset);

    return color;
}

#if defined(SWGL_DRAW_SPAN)
void swgl_drawSpanRGBA8() {
    int address = swgl_validateGradient(sGpuBufferF, get_gpu_buffer_uv(v_gradient_address.x),
                                        int(GRADIENT_ENTRIES + 2.0));
    if (address < 0) {
        return;
    }
    swgl_commitRadialGradientRGBA8(sGpuBufferF, address, GRADIENT_ENTRIES, v_gradient_repeat.x != 0.0,
                                   v_pos, v_start_radius.x);
}
#endif

#endif
