/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include rect,render_task,gpu_cache,snap,transform

#ifdef WR_VERTEX_SHADER

#define SEGMENT_ALL         0
#define SEGMENT_CORNER_TL   1
#define SEGMENT_CORNER_TR   2
#define SEGMENT_CORNER_BL   3
#define SEGMENT_CORNER_BR   4

in int aClipRenderTaskAddress;
in int aClipTransformId;
in int aPrimTransformId;
in int aClipSegment;
in ivec4 aClipDataResourceAddress;

struct ClipMaskInstance {
    int render_task_address;
    int clip_transform_id;
    int prim_transform_id;
    int segment;
    ivec2 clip_data_address;
    ivec2 resource_address;
};

ClipMaskInstance fetch_clip_item() {
    ClipMaskInstance cmi;

    cmi.render_task_address = aClipRenderTaskAddress;
    cmi.clip_transform_id = aClipTransformId;
    cmi.prim_transform_id = aPrimTransformId;
    cmi.segment = aClipSegment;
    cmi.clip_data_address = aClipDataResourceAddress.xy;
    cmi.resource_address = aClipDataResourceAddress.zw;

    return cmi;
}

struct ClipVertexInfo {
    vec3 local_pos;
    RectWithSize clipped_local_rect;
};

RectWithSize intersect_rect(RectWithSize a, RectWithSize b) {
    vec4 p = clamp(vec4(a.p0, a.p0 + a.size), b.p0.xyxy, b.p0.xyxy + b.size.xyxy);
    return RectWithSize(p.xy, max(vec2(0.0), p.zw - p.xy));
}

// The transformed vertex function that always covers the whole clip area,
// which is the intersection of all clip instances of a given primitive
ClipVertexInfo write_clip_tile_vertex(RectWithSize local_clip_rect,
                                      Transform prim_transform,
                                      Transform clip_transform,
                                      ClipArea area) {
    vec2 device_pos = area.screen_origin +
                      aPosition.xy * area.common_data.task_rect.size;

    if (clip_transform.is_axis_aligned && prim_transform.is_axis_aligned) {
        mat4 snap_mat = clip_transform.m * prim_transform.inv_m;
        vec4 snap_positions = compute_snap_positions(
            snap_mat,
            local_clip_rect
        );

        vec2 snap_offsets = compute_snap_offset_impl(
            device_pos,
            snap_mat,
            local_clip_rect,
            RectWithSize(snap_positions.xy, snap_positions.zw - snap_positions.xy),
            snap_positions
        );

        device_pos -= snap_offsets;
    }

    vec2 world_pos = device_pos / uDevicePixelRatio;

    vec4 pos = prim_transform.m * vec4(world_pos, 0.0, 1.0);
    pos.xyz /= pos.w;

    vec4 p = get_node_pos(pos.xy, clip_transform);
    vec3 local_pos = p.xyw * pos.w;

    vec4 vertex_pos = vec4(
        area.common_data.task_rect.p0 + aPosition.xy * area.common_data.task_rect.size,
        0.0,
        1.0
    );

    gl_Position = uTransform * vertex_pos;

    init_transform_vs(vec4(local_clip_rect.p0, local_clip_rect.p0 + local_clip_rect.size));

    ClipVertexInfo vi = ClipVertexInfo(local_pos, local_clip_rect);
    return vi;
}

#endif //WR_VERTEX_SHADER
