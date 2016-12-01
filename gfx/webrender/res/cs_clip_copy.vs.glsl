#line 1
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

void main(void) {
    CacheClipInstance cci = fetch_clip_item(gl_InstanceID);
    ClipArea area = fetch_clip_area(cci.render_task_index);
    ClipArea source = fetch_clip_area(cci.base_task_index);

    vec2 final_pos = mix(area.task_bounds.xy, area.task_bounds.zw, aPosition.xy);

    gl_Position = uTransform * vec4(final_pos, 0.0, 1.0);

    // convert to the source task space via the screen space
    vec2 tuv = final_pos - area.task_bounds.xy + area.screen_origin_target_index.xy +
        source.task_bounds.xy - source.screen_origin_target_index.xy;
    vClipMaskUv = vec3(tuv, source.screen_origin_target_index.z);
}
