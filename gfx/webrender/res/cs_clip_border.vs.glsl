#line 1
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Matches BorderCorner enum in border.rs
#define CORNER_TOP_LEFT     0
#define CORNER_TOP_RIGHT    1
#define CORNER_BOTTOM_LEFT  2
#define CORNER_BOTTOM_RIGHT 3

// Matches BorderCornerClipKind enum in border.rs
#define CLIP_MODE_DASH      0
#define CLIP_MODE_DOT       1

// Header for a border corner clip.
struct BorderCorner {
    RectWithSize rect;
    vec2 clip_center;
    int corner;
    int clip_mode;
};

BorderCorner fetch_border_corner(int index) {
    vec4 data[2] = fetch_from_resource_cache_2(index);
    return BorderCorner(RectWithSize(data[0].xy, data[0].zw),
                        data[1].xy,
                        int(data[1].z),
                        int(data[1].w));
}

// Per-dash clip information.
struct BorderClipDash {
    vec4 point_tangent_0;
    vec4 point_tangent_1;
};

BorderClipDash fetch_border_clip_dash(int index) {
    vec4 data[2] = fetch_from_resource_cache_2(index);
    return BorderClipDash(data[0], data[1]);
}

// Per-dot clip information.
struct BorderClipDot {
    vec3 center_radius;
};

BorderClipDot fetch_border_clip_dot(int index) {
    vec4 data = fetch_from_resource_cache_1(index);
    return BorderClipDot(data.xyz);
}

void main(void) {
    CacheClipInstance cci = fetch_clip_item(gl_InstanceID);
    ClipArea area = fetch_clip_area(cci.render_task_index);
    Layer layer = fetch_layer(cci.layer_index);

    // Fetch the header information for this corner clip.
    BorderCorner corner = fetch_border_corner(cci.data_index);
    vClipCenter = corner.clip_center;

    if (cci.segment_index == 0) {
        // The first segment is used to zero out the border corner.
        vAlphaMask = vec2(0.0);
        vDotParams = vec3(0.0);
        vPoint_Tangent0 = vec4(1.0);
        vPoint_Tangent1 = vec4(1.0);
    } else {
        vec2 sign_modifier;
        switch (corner.corner) {
            case CORNER_TOP_LEFT:
                sign_modifier = vec2(-1.0);
                break;
            case CORNER_TOP_RIGHT:
                sign_modifier = vec2(1.0, -1.0);
                break;
            case CORNER_BOTTOM_RIGHT:
                sign_modifier = vec2(1.0);
                break;
            case CORNER_BOTTOM_LEFT:
                sign_modifier = vec2(-1.0, 1.0);
                break;
        };

        switch (corner.clip_mode) {
            case CLIP_MODE_DASH: {
                // Fetch the information about this particular dash.
                BorderClipDash dash = fetch_border_clip_dash(cci.data_index + 2 + 2 * (cci.segment_index - 1));
                vPoint_Tangent0 = dash.point_tangent_0 * sign_modifier.xyxy;
                vPoint_Tangent1 = dash.point_tangent_1 * sign_modifier.xyxy;
                vDotParams = vec3(0.0);
                vAlphaMask = vec2(0.0, 1.0);
                break;
            }
            case CLIP_MODE_DOT: {
                BorderClipDot cdot = fetch_border_clip_dot(cci.data_index + 2 + (cci.segment_index - 1));
                vPoint_Tangent0 = vec4(1.0);
                vPoint_Tangent1 = vec4(1.0);
                vDotParams = vec3(cdot.center_radius.xy * sign_modifier, cdot.center_radius.z);
                vAlphaMask = vec2(1.0, 1.0);
                break;
            }
        }
    }

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
