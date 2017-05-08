#line 1
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Header for a border corner clip.
struct BorderCorner {
    RectWithSize rect;
    vec2 clip_center;
    vec2 sign_modifier;
};

BorderCorner fetch_border_corner(int index) {
    vec4 data[2] = fetch_data_2(index);
    return BorderCorner(RectWithSize(data[0].xy, data[0].zw),
                        data[1].xy,
                        data[1].zw);
}

// Per-dash clip information.
// TODO: Expand this to handle dots in the future!
struct BorderClip {
    vec4 point_tangent_0;
    vec4 point_tangent_1;
};

BorderClip fetch_border_clip(int index) {
    vec4 data[2] = fetch_data_2(index);
    return BorderClip(data[0], data[1]);
}

void main(void) {
    CacheClipInstance cci = fetch_clip_item(gl_InstanceID);
    ClipArea area = fetch_clip_area(cci.render_task_index);
    Layer layer = fetch_layer(cci.layer_index);

    // Fetch the header information for this corner clip.
    BorderCorner corner = fetch_border_corner(cci.data_index);
    vClipCenter = corner.clip_center;

    // Fetch the information about this particular dash.
    BorderClip clip = fetch_border_clip(cci.data_index + cci.segment_index + 1);
    vPoint_Tangent0 = clip.point_tangent_0 * corner.sign_modifier.xyxy;
    vPoint_Tangent1 = clip.point_tangent_1 * corner.sign_modifier.xyxy;

    // Get local vertex position for the corner rect.
    // TODO(gw): We could reduce the number of pixels written here
    // by calculating a tight fitting bounding box of the dash itself.
    vec2 pos = corner.rect.p0 + aPosition.xy * corner.rect.size;

    // Transform to world pos
    vec4 world_pos = layer.transform * vec4(pos, 0.0, 1.0);
    world_pos.xyz /= world_pos.w;

    // Scale into device pixels.
    vec2 device_pos = world_pos.xy * uDevicePixelRatio;

    // Position vertex within the render task area.
    vec2 final_pos = device_pos -
                     area.screen_origin_target_index.xy +
                     area.task_bounds.xy;

    // Calculate the local space position for this vertex.
    vec4 layer_pos = get_layer_pos(world_pos.xy, layer);
    vPos = layer_pos.xyw;

    gl_Position = uTransform * vec4(final_pos, 0.0, 1.0);
}
