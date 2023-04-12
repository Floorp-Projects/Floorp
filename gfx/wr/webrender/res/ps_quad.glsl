/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define WR_FEATURE_TEXTURE_2D

#include shared,rect,transform,render_task,gpu_buffer

flat varying mediump vec4 v_color;
flat varying mediump ivec2 v_flags;

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

#define QF_IS_OPAQUE            1

#define AA_PIXEL_RADIUS 2.0

PER_INSTANCE in ivec4 aData;

struct PrimitiveInfo {
    vec2 local_pos;

    RectWithEndpoint local_prim_rect;
    RectWithEndpoint local_clip_rect;

    int edge_flags;
};

struct QuadPrimitive {
    RectWithEndpoint bounds;
    RectWithEndpoint clip;
    vec4 color;
};

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

        aData.w
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

    vi.local_pos = local_pos;

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

    v_color = prim.color;

    // The local space rect that we will draw, which is effectively:
    //  - The tile within the primitive we will draw
    //  - Intersected with any local-space clip rect(s)
    //  - Expanded for AA edges where appropriate
    RectWithEndpoint local_coverage_rect = prim.bounds;

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
        default:
            local_coverage_rect.p0.x += edge_aa_offset(EDGE_AA_LEFT, qi.edge_flags);
            local_coverage_rect.p1.x -= edge_aa_offset(EDGE_AA_RIGHT, qi.edge_flags);
            local_coverage_rect.p0.y += edge_aa_offset(EDGE_AA_TOP, qi.edge_flags);
            local_coverage_rect.p1.y -= edge_aa_offset(EDGE_AA_BOTTOM, qi.edge_flags);
            break;
    }

    vec2 local_pos = mix(local_coverage_rect.p0, local_coverage_rect.p1, aPosition);

    VertexInfo vi = write_vertex(
        local_pos,
        z,
        transform,
        task.content_origin,
        task.task_rect,
        task.device_pixel_scale,
        qi.quad_flags
    );

    return PrimitiveInfo(
        vi.local_pos,
        prim.bounds,
        prim.clip,
        qi.edge_flags
    );
}

#endif
