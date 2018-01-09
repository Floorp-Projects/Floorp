/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include shared,prim_shared,clip_shared

varying vec3 vPos;

flat varying vec2 vClipCenter;

flat varying vec4 vPoint_Tangent0;
flat varying vec4 vPoint_Tangent1;
flat varying vec3 vDotParams;
flat varying vec2 vAlphaMask;

#ifdef WR_VERTEX_SHADER
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

BorderCorner fetch_border_corner(ivec2 address) {
    vec4 data[2] = fetch_from_resource_cache_2_direct(address);
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

BorderClipDash fetch_border_clip_dash(ivec2 address, int segment) {
    vec4 data[2] = fetch_from_resource_cache_2_direct(address + ivec2(2 + 2 * (segment - 1), 0));
    return BorderClipDash(data[0], data[1]);
}

// Per-dot clip information.
struct BorderClipDot {
    vec3 center_radius;
};

BorderClipDot fetch_border_clip_dot(ivec2 address, int segment) {
    vec4 data = fetch_from_resource_cache_1_direct(address + ivec2(2 + (segment - 1), 0));
    return BorderClipDot(data.xyz);
}

void main(void) {
    ClipMaskInstance cmi = fetch_clip_item();
    ClipArea area = fetch_clip_area(cmi.render_task_address);
    ClipScrollNode scroll_node = fetch_clip_scroll_node(cmi.scroll_node_id);

    // Fetch the header information for this corner clip.
    BorderCorner corner = fetch_border_corner(cmi.clip_data_address);
    vClipCenter = corner.clip_center;

    if (cmi.segment == 0) {
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
                BorderClipDash dash = fetch_border_clip_dash(cmi.clip_data_address, cmi.segment);
                vPoint_Tangent0 = dash.point_tangent_0 * sign_modifier.xyxy;
                vPoint_Tangent1 = dash.point_tangent_1 * sign_modifier.xyxy;
                vDotParams = vec3(0.0);
                vAlphaMask = vec2(0.0, 1.0);
                break;
            }
            case CLIP_MODE_DOT: {
                BorderClipDot cdot = fetch_border_clip_dot(cmi.clip_data_address, cmi.segment);
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
    vec4 world_pos = scroll_node.transform * vec4(pos, 0.0, 1.0);
    world_pos.xyz /= world_pos.w;

    // Scale into device pixels.
    vec2 device_pos = world_pos.xy * uDevicePixelRatio;

    // Position vertex within the render task area.
    vec2 final_pos = device_pos -
                     area.screen_origin +
                     area.common_data.task_rect.p0;

    // Calculate the local space position for this vertex.
    vec4 node_pos = get_node_pos(world_pos.xy, scroll_node);
    vPos = node_pos.xyw;

    gl_Position = uTransform * vec4(final_pos, 0.0, 1.0);
}
#endif

#ifdef WR_FRAGMENT_SHADER
void main(void) {
    vec2 local_pos = vPos.xy / vPos.z;

    // Get local space position relative to the clip center.
    vec2 clip_relative_pos = local_pos - vClipCenter;

    // Get the signed distances to the two clip lines.
    float d0 = distance_to_line(vPoint_Tangent0.xy,
                                vPoint_Tangent0.zw,
                                clip_relative_pos);
    float d1 = distance_to_line(vPoint_Tangent1.xy,
                                vPoint_Tangent1.zw,
                                clip_relative_pos);

    // Get AA widths based on zoom / scale etc.
    float aa_range = compute_aa_range(local_pos);

    // SDF subtract edges for dash clip
    float dash_distance = max(d0, -d1);

    // Get distance from dot.
    float dot_distance = distance(clip_relative_pos, vDotParams.xy) - vDotParams.z;

    // Select between dot/dash clip based on mode.
    float d = mix(dash_distance, dot_distance, vAlphaMask.x);

    // Apply AA.
    d = distance_aa(aa_range, d);

    // Completely mask out clip if zero'ing out the rect.
    d = d * vAlphaMask.y;

    oFragColor = vec4(d, 0.0, 0.0, 1.0);
}
#endif
