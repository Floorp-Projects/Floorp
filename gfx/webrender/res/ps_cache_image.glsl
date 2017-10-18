/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include shared,prim_shared

varying vec3 vUv;
flat varying vec4 vUvBounds;

#if defined WR_FEATURE_ALPHA
flat varying vec4 vColor;
#endif

#ifdef WR_VERTEX_SHADER
// Draw a cached primitive (e.g. a blurred text run) from the
// target cache to the framebuffer, applying tile clip boundaries.

void main(void) {
    Primitive prim = load_primitive();

    VertexInfo vi = write_vertex(prim.local_rect,
                                 prim.local_clip_rect,
                                 prim.z,
                                 prim.layer,
                                 prim.task,
                                 prim.local_rect);

    RenderTaskData child_task = fetch_render_task(prim.user_data1);
    vUv.z = child_task.data1.x;

#if defined WR_FEATURE_COLOR
    vec2 texture_size = vec2(textureSize(sColor0, 0).xy);
#else
    Picture pic = fetch_picture(prim.specific_prim_address);

    vec2 texture_size = vec2(textureSize(sColor1, 0).xy);
    vColor = pic.color;
#endif
    vec2 uv0 = child_task.data0.xy;
    vec2 uv1 = (child_task.data0.xy + child_task.data0.zw);

    vec2 f = (vi.local_pos - prim.local_rect.p0) / prim.local_rect.size;

    vUv.xy = mix(uv0 / texture_size,
                 uv1 / texture_size,
                 f);
    vUvBounds = vec4(uv0 + vec2(0.5), uv1 - vec2(0.5)) / texture_size.xyxy;

    write_clip(vi.screen_pos, prim.clip_area);
}
#endif

#ifdef WR_FRAGMENT_SHADER
void main(void) {
    vec2 uv = clamp(vUv.xy, vUvBounds.xy, vUvBounds.zw);

#if defined WR_FEATURE_COLOR
    vec4 color = texture(sColor0, vec3(uv, vUv.z));
#else
    vec4 color = vColor * texture(sColor1, vec3(uv, vUv.z)).r;
#endif

    // Un-premultiply the color from sampling the gradient.
    if (color.a > 0.0) {
        color.rgb /= color.a;

        // Apply the clip mask
        color.a = min(color.a, do_clip());

        // Pre-multiply the result.
        color.rgb *= color.a;
    }

    oFragColor = color;
}
#endif
