/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include shared,prim_shared,brush

varying vec3 vUv;
flat varying vec4 vUvBounds;

#if defined WR_FEATURE_ALPHA_TARGET
flat varying vec4 vColor;
#endif

#ifdef WR_VERTEX_SHADER
void brush_vs(
    int prim_address,
    vec2 local_pos,
    RectWithSize local_rect,
    ivec2 user_data
) {
    // TODO(gw): For now, this brush_image shader is only
    //           being used to draw items from the intermediate
    //           surface cache (render tasks). In the future
    //           we can expand this to support items from
    //           the normal texture cache and unify this
    //           with the normal image shader.
    BlurTask task = fetch_blur_task(user_data.x);
    vUv.z = task.render_target_layer_index;

#if defined WR_FEATURE_COLOR_TARGET
    vec2 texture_size = vec2(textureSize(sColor0, 0).xy);
#else
    vec2 texture_size = vec2(textureSize(sColor1, 0).xy);
    vColor = task.color;
#endif

    vec2 uv0 = task.target_rect.p0;
    vec2 uv1 = (task.target_rect.p0 + task.target_rect.size);

    vec2 f = (local_pos - local_rect.p0) / local_rect.size;

    vUv.xy = mix(uv0 / texture_size,
                 uv1 / texture_size,
                 f);

    vUvBounds = vec4(uv0 + vec2(0.5), uv1 - vec2(0.5)) / texture_size.xyxy;
}
#endif

#ifdef WR_FRAGMENT_SHADER
vec4 brush_fs() {
    vec2 uv = clamp(vUv.xy, vUvBounds.xy, vUvBounds.zw);

#if defined WR_FEATURE_COLOR_TARGET
    vec4 color = texture(sColor0, vec3(uv, vUv.z));
#else
    vec4 color = vColor * texture(sColor1, vec3(uv, vUv.z)).r;
#endif

    return color;
}
#endif
