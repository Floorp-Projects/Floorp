/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include shared,rect,render_task,gpu_cache,gpu_buffer,gradient

varying highp vec2 v_pos;

// Start radius. Packed in to a vector to work around bug 1630356.
flat varying highp vec2 v_start_radius;

#ifdef WR_VERTEX_SHADER

#define EXTEND_MODE_REPEAT 1

PER_INSTANCE in vec4 aTaskRect;
PER_INSTANCE in vec2 aCenter;
PER_INSTANCE in vec2 aScale;
PER_INSTANCE in float aStartRadius;
PER_INSTANCE in float aEndRadius;
PER_INSTANCE in float aXYRatio;
PER_INSTANCE in int aExtendMode;
PER_INSTANCE in int aGradientStopsAddress;

void main(void) {
    // Store 1/rd where rd = end_radius - start_radius
    // If rd = 0, we can't get its reciprocal. Instead, just use a zero scale.
    float rd = aEndRadius - aStartRadius;
    float radius_scale = rd != 0.0 ? 1.0 / rd : 0.0;

    vec2 pos = mix(aTaskRect.xy, aTaskRect.zw, aPosition.xy);
    gl_Position = uTransform * vec4(pos, 0.0, 1.0);

    v_start_radius.x = aStartRadius * radius_scale;

    // Transform all coordinates by the y scale so the
    // fragment shader can work with circles

    // v_pos is in a coordinate space relative to the task rect
    // (so it is independent of the task origin).
    v_pos = ((aTaskRect.zw - aTaskRect.xy) * aPosition.xy * aScale - aCenter) * radius_scale;
    v_pos.y *= aXYRatio;

    v_gradient_repeat.x = float(aExtendMode == EXTEND_MODE_REPEAT);
    v_gradient_address.x = aGradientStopsAddress;
}
#endif


#ifdef WR_FRAGMENT_SHADER

void main(void) {
    // Solve for t in length(pd) = v_start_radius + t * rd
    float offset = length(v_pos) - v_start_radius.x;

    oFragColor = sample_gradient(offset);
}

#ifdef SWGL_DRAW_SPAN
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
