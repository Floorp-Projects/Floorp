/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/// This shader renders radial graidents in a color or alpha target.

#include ps_quad,gradient

#define PI                  3.141592653589793

// x: start offset, y: offset scale, z: angle
// Packed in to a vector to work around bug 1630356.
flat varying highp vec3 v_start_offset_offset_scale_angle_vec;
#define v_start_offset v_start_offset_offset_scale_angle_vec.x
#define v_offset_scale v_start_offset_offset_scale_angle_vec.y
#define v_angle v_start_offset_offset_scale_angle_vec.z

varying highp vec2 v_dir;

#ifdef WR_VERTEX_SHADER
struct ConicGradient {
    vec2 center;
    vec2 scale;
    float start_offset;
    float end_offset;
    float angle;
    // 1.0 if the gradient should be repeated, 0.0 otherwise.
    float repeat;
};

ConicGradient fetch_conic_gradient(int address) {
    vec4[2] data = fetch_from_gpu_buffer_2f(address);

    return ConicGradient(
        data[0].xy,
        data[0].zw,
        data[1].x,
        data[1].y,
        data[1].z,
        data[1].w
    );
}

void pattern_vertex(PrimitiveInfo info) {
    ConicGradient gradient = fetch_conic_gradient(info.pattern_input.x);
    v_gradient_address.x = info.pattern_input.y;
    v_gradient_repeat.x = gradient.repeat;

    // Store 1/d where d = end_offset - start_offset
    // If d = 0, we can't get its reciprocal. Instead, just use a zero scale.
    float d = gradient.end_offset - gradient.start_offset;
    v_offset_scale = d != 0.0 ? 1.0 / d : 0.0;

    v_angle = PI / 2.0 - gradient.angle;
    v_start_offset = gradient.start_offset * v_offset_scale;
    v_dir = ((info.local_pos - info.local_prim_rect.p0) * gradient.scale - gradient.center);
}

#endif


#ifdef WR_FRAGMENT_SHADER

// From https://math.stackexchange.com/questions/1098487/atan2-faster-approximation
float approx_atan2(float y, float x) {
    vec2 a = abs(vec2(x, y));
    float slope = min(a.x, a.y) / max(a.x, a.y);
    float s2 = slope * slope;
    float r = ((-0.0464964749 * s2 + 0.15931422) * s2 - 0.327622764) * s2 * slope + slope;

    r = if_then_else(float(a.y > a.x), 1.57079637 - r, r);
    r = if_then_else(float(x < 0.0),   3.14159274 - r, r);
    // To match atan2's behavior, -0.0 should count as negative and flip the sign of r.
    // Does this matter in practice in the context of conic gradients?
    r = r * sign(y);

    return r;
}

vec4 pattern_fragment(vec4 color) {
    // Use inverse trig to find the angle offset from the relative position.
    vec2 current_dir = v_dir;
    float current_angle = approx_atan2(current_dir.y, current_dir.x) + v_angle;
    float offset = fract(current_angle / (2.0 * PI)) * v_offset_scale - v_start_offset;

    color *= sample_gradient(offset);
    return color;
}

#endif
