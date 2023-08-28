/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define WR_FEATURE_TEXTURE_2D

#include shared,rect,transform,render_task,gpu_buffer

flat varying mediump vec4 v_color;
flat varying mediump vec4 v_uv_sample_bounds;
flat varying lowp ivec4 v_flags;
varying highp vec2 v_uv;

#ifdef WR_VERTEX_SHADER

#define EDGE_AA_LEFT    1
#define EDGE_AA_TOP     2
#define EDGE_AA_RIGHT   4
#define EDGE_AA_BOTTOM  8

#define PART_CENTER     0
#define PART_LEFT       1
#define PART_TOP        2
#define PART_RIGHT      3
#define PART_BOTTOM     4
#define PART_ALL        5

#define QF_IS_OPAQUE            1
#define QF_APPLY_DEVICE_CLIP    2
#define QF_IGNORE_DEVICE_SCALE  4
#define QF_USE_AA_SEGMENTS      8
#define QF_SAMPLE_AS_MASK       16

#define INVALID_SEGMENT_INDEX   0xff

#define AA_PIXEL_RADIUS 2.0

PER_INSTANCE in ivec4 aData;

struct PrimitiveInfo {
    vec2 local_pos;

    RectWithEndpoint local_prim_rect;
    RectWithEndpoint local_clip_rect;

    int edge_flags;
    int quad_flags;
};

struct QuadSegment {
    RectWithEndpoint rect;
    vec4 uv_rect;
};

struct QuadPrimitive {
    RectWithEndpoint bounds;
    RectWithEndpoint clip;
    vec4 color;
};

QuadSegment fetch_segment(int base, int index) {
    QuadSegment seg;

    vec4 texels[2] = fetch_from_gpu_buffer_2(base + 3 + index * 2);

    seg.rect = RectWithEndpoint(texels[0].xy, texels[0].zw);
    seg.uv_rect = texels[1];

    return seg;
}

QuadPrimitive fetch_primitive(int index) {
    QuadPrimitive prim;

    vec4 texels[3] = fetch_from_gpu_buffer_3(index);

    prim.bounds = RectWithEndpoint(texels[0].xy, texels[0].zw);
    prim.clip = RectWithEndpoint(texels[1].xy, texels[1].zw);
    prim.color = texels[2];

    return prim;
}

struct QuadInstance {
    // x
    int prim_address;

    // y
    int quad_flags;
    int edge_flags;
    int picture_task_address;

    // z
    int part_index;
    int z_id;

    // w
    int segment_index;
    int transform_id;
};

QuadInstance decode_instance() {
    QuadInstance qi = QuadInstance(
        aData.x,

        (aData.y >> 24) & 0xff,
        (aData.y >> 16) & 0xff,
        aData.y & 0xffff,

        (aData.z >> 24) & 0xff,
        aData.z & 0xffffff,

        (aData.w >> 24) & 0xff,
        aData.w & 0xffffff
    );

    return qi;
}

struct VertexInfo {
    vec2 local_pos;
};

VertexInfo write_vertex(vec2 local_pos,
                        float z,
                        Transform transform,
                        vec2 content_origin,
                        RectWithEndpoint task_rect,
                        float device_pixel_scale,
                        int quad_flags) {
    VertexInfo vi;

    // Transform the current vertex to world space.
    vec4 world_pos = transform.m * vec4(local_pos, 0.0, 1.0);

    // Convert the world positions to device pixel space.
    vec2 device_pos = world_pos.xy * device_pixel_scale;

    if ((quad_flags & QF_APPLY_DEVICE_CLIP) != 0) {
        RectWithEndpoint device_clip_rect = RectWithEndpoint(
            content_origin,
            content_origin + task_rect.p1 - task_rect.p0
        );

        // Clip to task rect
        device_pos = rect_clamp(device_clip_rect, device_pos);

        vi.local_pos = (transform.inv_m * vec4(device_pos / device_pixel_scale, 0.0, 1.0)).xy;
    } else {
        vi.local_pos = local_pos;
    }

    // Apply offsets for the render task to get correct screen location.
    vec2 final_offset = -content_origin + task_rect.p0;

    gl_Position = uTransform * vec4(device_pos + final_offset * world_pos.w, z * world_pos.w, world_pos.w);

    return vi;
}

float edge_aa_offset(int edge, int flags) {
    return ((flags & edge) != 0) ? AA_PIXEL_RADIUS : 0.0;
}

PrimitiveInfo ps_quad_main(void) {
    QuadInstance qi = decode_instance();

    Transform transform = fetch_transform(qi.transform_id);
    PictureTask task = fetch_picture_task(qi.picture_task_address);
    QuadPrimitive prim = fetch_primitive(qi.prim_address);
    float z = float(qi.z_id);

    QuadSegment seg;
    if (qi.segment_index == INVALID_SEGMENT_INDEX) {
        seg.rect = prim.bounds;
        seg.uv_rect = vec4(0.0);
    } else {
        seg = fetch_segment(qi.prim_address, qi.segment_index);
    }

    // The local space rect that we will draw, which is effectively:
    //  - The tile within the primitive we will draw
    //  - Intersected with any local-space clip rect(s)
    //  - Expanded for AA edges where appropriate
    RectWithEndpoint local_coverage_rect = seg.rect;

    // Apply local clip rect
    local_coverage_rect.p0 = max(local_coverage_rect.p0, prim.clip.p0);
    local_coverage_rect.p1 = min(local_coverage_rect.p1, prim.clip.p1);
    local_coverage_rect.p1 = max(local_coverage_rect.p0, local_coverage_rect.p1);

    switch (qi.part_index) {
        case PART_LEFT:
            local_coverage_rect.p1.x = local_coverage_rect.p0.x + AA_PIXEL_RADIUS;
#ifdef SWGL_ANTIALIAS
            swgl_antiAlias(EDGE_AA_LEFT);
#else
            local_coverage_rect.p0.x -= AA_PIXEL_RADIUS;
            local_coverage_rect.p0.y -= AA_PIXEL_RADIUS;
            local_coverage_rect.p1.y += AA_PIXEL_RADIUS;
#endif
            break;
        case PART_TOP:
            local_coverage_rect.p0.x = local_coverage_rect.p0.x + AA_PIXEL_RADIUS;
            local_coverage_rect.p1.x = local_coverage_rect.p1.x - AA_PIXEL_RADIUS;
            local_coverage_rect.p1.y = local_coverage_rect.p0.y + AA_PIXEL_RADIUS;
#ifdef SWGL_ANTIALIAS
            swgl_antiAlias(EDGE_AA_TOP);
#else
            local_coverage_rect.p0.y -= AA_PIXEL_RADIUS;
#endif
            break;
        case PART_RIGHT:
            local_coverage_rect.p0.x = local_coverage_rect.p1.x - AA_PIXEL_RADIUS;
#ifdef SWGL_ANTIALIAS
            swgl_antiAlias(EDGE_AA_RIGHT);
#else
            local_coverage_rect.p1.x += AA_PIXEL_RADIUS;
            local_coverage_rect.p0.y -= AA_PIXEL_RADIUS;
            local_coverage_rect.p1.y += AA_PIXEL_RADIUS;
#endif
            break;
        case PART_BOTTOM:
            local_coverage_rect.p0.x = local_coverage_rect.p0.x + AA_PIXEL_RADIUS;
            local_coverage_rect.p1.x = local_coverage_rect.p1.x - AA_PIXEL_RADIUS;
            local_coverage_rect.p0.y = local_coverage_rect.p1.y - AA_PIXEL_RADIUS;
#ifdef SWGL_ANTIALIAS
            swgl_antiAlias(EDGE_AA_BOTTOM);
#else
            local_coverage_rect.p1.y += AA_PIXEL_RADIUS;
#endif
            break;
        case PART_CENTER:
            local_coverage_rect.p0.x += edge_aa_offset(EDGE_AA_LEFT, qi.edge_flags);
            local_coverage_rect.p1.x -= edge_aa_offset(EDGE_AA_RIGHT, qi.edge_flags);
            local_coverage_rect.p0.y += edge_aa_offset(EDGE_AA_TOP, qi.edge_flags);
            local_coverage_rect.p1.y -= edge_aa_offset(EDGE_AA_BOTTOM, qi.edge_flags);
            break;
        case PART_ALL:
        default:
#ifdef SWGL_ANTIALIAS
            swgl_antiAlias(qi.edge_flags);
#else
            local_coverage_rect.p0.x -= edge_aa_offset(EDGE_AA_LEFT, qi.edge_flags);
            local_coverage_rect.p1.x += edge_aa_offset(EDGE_AA_RIGHT, qi.edge_flags);
            local_coverage_rect.p0.y -= edge_aa_offset(EDGE_AA_TOP, qi.edge_flags);
            local_coverage_rect.p1.y += edge_aa_offset(EDGE_AA_BOTTOM, qi.edge_flags);
#endif
            break;
    }

    vec2 local_pos = mix(local_coverage_rect.p0, local_coverage_rect.p1, aPosition);

    float device_pixel_scale = task.device_pixel_scale;
    if ((qi.quad_flags & QF_IGNORE_DEVICE_SCALE) != 0) {
        device_pixel_scale = 1.0f;
    }

    VertexInfo vi = write_vertex(
        local_pos,
        z,
        transform,
        task.content_origin,
        task.task_rect,
        device_pixel_scale,
        qi.quad_flags
    );

    if (seg.uv_rect.xy == seg.uv_rect.zw) {
        v_color = prim.color;
        v_flags.y = 0;
    } else {
        v_color = vec4(1.0);
        v_flags.y = 1;

        vec2 f = (vi.local_pos - seg.rect.p0) / (seg.rect.p1 - seg.rect.p0);

        vec2 uv = mix(
            seg.uv_rect.xy,
            seg.uv_rect.zw,
            f
        );

        vec2 texture_size = vec2(TEX_SIZE(sColor0));

        v_uv = uv / texture_size;

        v_uv_sample_bounds = vec4(
            seg.uv_rect.xy + vec2(0.5),
            seg.uv_rect.zw - vec2(0.5)
        ) / texture_size.xyxy;
    }

    return PrimitiveInfo(
        vi.local_pos,
        prim.bounds,
        prim.clip,
        qi.edge_flags,
        qi.quad_flags
    );
}
#endif
