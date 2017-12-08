/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include shared,prim_shared

varying vec3 vUv;
flat varying vec4 vUvBounds;

#ifdef WR_VERTEX_SHADER
void main(void) {
    CompositeInstance ci = fetch_composite_instance();
    PictureTask dest_task = fetch_picture_task(ci.render_task_index);
    PictureTask src_task = fetch_picture_task(ci.src_task_index);

    vec2 dest_origin = dest_task.common_data.task_rect.p0 -
                       dest_task.content_origin +
                       vec2(ci.user_data0, ci.user_data1);

    vec2 local_pos = mix(dest_origin,
                         dest_origin + vec2(ci.user_data2, ci.user_data3),
                         aPosition.xy);

    vec2 texture_size = vec2(textureSize(sCacheRGBA8, 0));
    vec2 st0 = src_task.common_data.task_rect.p0;
    vec2 st1 = src_task.common_data.task_rect.p0 + src_task.common_data.task_rect.size;
    vUv = vec3(mix(st0, st1, aPosition.xy) / texture_size, src_task.common_data.texture_layer_index);
    vUvBounds = vec4(st0 + 0.5, st1 - 0.5) / texture_size.xyxy;

    gl_Position = uTransform * vec4(local_pos, ci.z, 1.0);
}
#endif

#ifdef WR_FRAGMENT_SHADER
void main(void) {
    vec2 uv = clamp(vUv.xy, vUvBounds.xy, vUvBounds.zw);
    oFragColor = texture(sColor0, vec3(uv, vUv.z));
}
#endif
