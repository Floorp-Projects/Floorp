#line 1
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

void main(void) {
    CompositeInstance ci = fetch_composite_instance();
    AlphaBatchTask dest_task = fetch_alpha_batch_task(ci.render_task_index);
    AlphaBatchTask src_task = fetch_alpha_batch_task(ci.src_task_index);

    vec2 dest_origin = dest_task.render_target_origin -
                       dest_task.screen_space_origin +
                       src_task.screen_space_origin;

    vec2 local_pos = mix(dest_origin,
                         dest_origin + src_task.size,
                         aPosition.xy);

    vec2 texture_size = vec2(textureSize(sCacheRGBA8, 0));
    vec2 st0 = src_task.render_target_origin;
    vec2 st1 = src_task.render_target_origin + src_task.size;

    vec2 uv = src_task.render_target_origin + aPosition.xy * src_task.size;
    vUv = vec3(uv / texture_size, src_task.render_target_layer_index);
    vUvBounds = vec4(st0 + 0.5, st1 - 0.5) / texture_size.xyxy;

    vOp = ci.user_data0;
    vAmount = float(ci.user_data1) / 65535.0;

    gl_Position = uTransform * vec4(local_pos, ci.z, 1.0);
}
