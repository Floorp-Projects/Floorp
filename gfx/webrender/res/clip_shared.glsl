#line 1
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef WR_VERTEX_SHADER

in int aClipRenderTaskIndex;
in int aClipLayerIndex;
in int aClipDataIndex;
in int aClipBaseTaskIndex;

struct CacheClipInstance {
    int render_task_index;
    int layer_index;
    int data_index;
    int base_task_index;
};

CacheClipInstance fetch_clip_item(int index) {
    CacheClipInstance cci;

    cci.render_task_index = aClipRenderTaskIndex;
    cci.layer_index = aClipLayerIndex;
    cci.data_index = aClipDataIndex;
    cci.base_task_index = aClipBaseTaskIndex;

    return cci;
}

// The transformed vertex function that always covers the whole clip area,
// which is the intersection of all clip instances of a given primitive
TransformVertexInfo write_clip_tile_vertex(vec4 local_clip_rect,
                                           Layer layer,
                                           ClipArea area) {
    vec2 lp0_base = local_clip_rect.xy;
    vec2 lp1_base = local_clip_rect.xy + local_clip_rect.zw;

    vec2 lp0 = clamp_rect(lp0_base, layer.local_clip_rect);
    vec2 lp1 = clamp_rect(lp1_base, layer.local_clip_rect);
    vec4 clipped_local_rect = vec4(lp0, lp1 - lp0);

    vec2 final_pos = mix(area.task_bounds.xy, area.task_bounds.zw, aPosition.xy);

    // compute the point position in side the layer, in CSS space
    vec2 clamped_pos = final_pos + area.screen_origin_target_index.xy - area.task_bounds.xy;
    vec4 layer_pos = get_layer_pos(clamped_pos / uDevicePixelRatio, layer);

    gl_Position = uTransform * vec4(final_pos, 0.0, 1);

    return TransformVertexInfo(layer_pos.xyw, clamped_pos, clipped_local_rect);
}

#endif //WR_VERTEX_SHADER
