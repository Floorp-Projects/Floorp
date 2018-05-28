/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include rect,clip_scroll,render_task,resource_cache,snap,transform

#ifdef WR_VERTEX_SHADER

#define SEGMENT_ALL         0
#define SEGMENT_CORNER_TL   1
#define SEGMENT_CORNER_TR   2
#define SEGMENT_CORNER_BL   3
#define SEGMENT_CORNER_BR   4

in int aClipRenderTaskAddress;
in int aScrollNodeId;
in int aClipSegment;
in ivec4 aClipDataResourceAddress;

struct ClipMaskInstance {
    int render_task_address;
    int scroll_node_id;
    int segment;
    ivec2 clip_data_address;
    ivec2 resource_address;
};

ClipMaskInstance fetch_clip_item() {
    ClipMaskInstance cmi;

    cmi.render_task_address = aClipRenderTaskAddress;
    cmi.scroll_node_id = aScrollNodeId;
    cmi.segment = aClipSegment;
    cmi.clip_data_address = aClipDataResourceAddress.xy;
    cmi.resource_address = aClipDataResourceAddress.zw;

    return cmi;
}

struct ClipVertexInfo {
    vec3 local_pos;
    vec2 screen_pos;
    RectWithSize clipped_local_rect;
};

RectWithSize intersect_rect(RectWithSize a, RectWithSize b) {
    vec4 p = clamp(vec4(a.p0, a.p0 + a.size), b.p0.xyxy, b.p0.xyxy + b.size.xyxy);
    return RectWithSize(p.xy, max(vec2(0.0), p.zw - p.xy));
}

// The transformed vertex function that always covers the whole clip area,
// which is the intersection of all clip instances of a given primitive
ClipVertexInfo write_clip_tile_vertex(RectWithSize local_clip_rect,
                                      ClipScrollNode scroll_node,
                                      ClipArea area) {
    vec2 device_pos = area.screen_origin + aPosition.xy * area.common_data.task_rect.size;
    vec2 actual_pos = device_pos;

    if (scroll_node.is_axis_aligned) {
        vec4 snap_positions = compute_snap_positions(
            scroll_node.transform,
            local_clip_rect
        );

        vec2 snap_offsets = compute_snap_offset_impl(
            device_pos,
            scroll_node.transform,
            local_clip_rect,
            RectWithSize(snap_positions.xy, snap_positions.zw - snap_positions.xy),
            snap_positions,
            vec2(0.5)
        );

        actual_pos -= snap_offsets;
    }

    vec4 node_pos;

    // Select the local position, based on whether we are rasterizing this
    // clip mask in local- or sccreen-space.
    if (area.local_space) {
        node_pos = vec4(actual_pos / uDevicePixelRatio, 0.0, 1.0);
    } else {
        node_pos = get_node_pos(actual_pos / uDevicePixelRatio, scroll_node);
    }

    // compute the point position inside the scroll node, in CSS space
    vec2 vertex_pos = device_pos +
                      area.common_data.task_rect.p0 -
                      area.screen_origin;

    gl_Position = uTransform * vec4(vertex_pos, 0.0, 1);

    init_transform_vs(vec4(local_clip_rect.p0, local_clip_rect.p0 + local_clip_rect.size));

    ClipVertexInfo vi = ClipVertexInfo(node_pos.xyw, actual_pos, local_clip_rect);
    return vi;
}

#endif //WR_VERTEX_SHADER

#ifdef WR_FRAGMENT_SHADER

//Note: identical to prim_shared
float distance_to_line(vec2 p0, vec2 perp_dir, vec2 p) {
    vec2 dir_to_p0 = p0 - p;
    return dot(normalize(perp_dir), dir_to_p0);
}

#endif //WR_FRAGMENT_SHADER
