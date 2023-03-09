/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define WR_FEATURE_TEXTURE_2D

#include shared,rect,transform,render_task,gpu_buffer

flat varying vec4 v_color;
flat varying ivec2 v_flags;

#ifndef SWGL_ANTIALIAS
varying vec2 vLocalPos;
#endif

#ifdef WR_VERTEX_SHADER

#define EDGE_AA_LEFT    1
#define EDGE_AA_TOP     2
#define EDGE_AA_RIGHT   4
#define EDGE_AA_BOTTOM  8

#define AA_PIXEL_RADIUS 2.0

PER_INSTANCE in ivec4 aData;

struct QuadPrimitive {
    RectWithEndpoint bounds;
    RectWithEndpoint clip;
    vec4 uv_rect;
    vec4 color;
};

QuadPrimitive fetch_primitive(int index) {
    QuadPrimitive prim;

    vec4 texels[4] = fetch_from_gpu_buffer_4(index);

    prim.bounds = RectWithEndpoint(texels[0].xy, texels[0].zw);
    prim.clip = RectWithEndpoint(texels[1].xy, texels[1].zw);
    prim.uv_rect = texels[2];
    prim.color = texels[3];

    return prim;
}

struct QuadInstance {
    // x
    int prim_address;

    // y
    ivec2 tile_count;
    int picture_task_address;

    // z
    int edge_flags;
    int z_id;

    // w
    ivec2 tile_index;
    int transform_id;
};

QuadInstance decode_instance() {
    QuadInstance qi = QuadInstance(
        aData.x,

        ivec2(
            (aData.y >> 24) & 0xff,
            (aData.y >> 16) & 0xff
        ),
        aData.y & 0xffff,

        aData.z >> 24,
        aData.z & 0xffffff,

        ivec2(
            (aData.w >> 28) & 0xf,
            (aData.w >> 24) & 0xf
        ),
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
                        vec2 task_offset,
                        float device_pixel_scale) {
    vec2 clamped_local_pos = local_pos;

    // Transform the current vertex to world space.
    vec4 world_pos = transform.m * vec4(clamped_local_pos, 0.0, 1.0);

    // Convert the world positions to device pixel space.
    vec2 device_pos = world_pos.xy * device_pixel_scale;

    // Apply offsets for the render task to get correct screen location.
    vec2 final_offset = -content_origin + task_offset;

    vec4 clip_pos = uTransform * vec4(device_pos + final_offset * world_pos.w, z * world_pos.w, world_pos.w);

    gl_Position = clip_pos;

    VertexInfo vi = VertexInfo(
        clamped_local_pos
    );

    return vi;
}

#ifndef SWGL_ANTIALIAS
float edge_aa_offset(int edge, int flags) {
    return ((flags & edge) != 0) ? AA_PIXEL_RADIUS : 0.0;
}
#endif

// Given the edge AA flags for an axis, and the tile count for an axis,
// calculate the size of the first tile (in size.x), size of the inner tiles
// (in size.y), and the size of the last tile (size.z). For now, we don't
// use size.z, instead explicitly selecting the outer edge to avoid float
// accuracy issues. In future, it may be useful, or we may remove that field.
vec3 get_tile_sizes(
    int edge_flags,
    int edge1,
    int edge2,
    int tile_count,
    float prim_size
) {
    vec3 size = vec3(0.0);
    int auto_tile_count = tile_count;

    float inv_tile_count = 1.0 / float(tile_count);
    float norm_aa_radius = AA_PIXEL_RADIUS / prim_size;

    if ((edge_flags & edge1) != 0) {
        size.x = min(norm_aa_radius, inv_tile_count);
        auto_tile_count -= 1;
    }
    if ((edge_flags & edge2) != 0) {
        size.z = min(norm_aa_radius, inv_tile_count);
        auto_tile_count -= 1;
    }

    size.y = (1.0 - size.x - size.z) / float(auto_tile_count);

    if (size.x == 0.0) {
        size.x = size.y;
    }
    if (size.z == 0.0) {
        size.z = size.y;
    }

    return size;
}

// For a given tile index and tile size config, retrieve a normalized (0-1)
// fraction of the primitive size that this edge should be placed at. We're
// careful here to use math in a way that we won't get cracks between the
// tiles when there are weird scales / rotations involved.
float get_size_for_tile_index(
    int tile_index,
    int tile_count,
    vec3 sizes
) {
    if (tile_index == 0) {
        return 0.0;
    } else if (tile_index == tile_count) {
        return 1.0;
    } else {
        return sizes.x + float(tile_index-1) * sizes.y;
    }
}

void main(void) {
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
    RectWithEndpoint local_coverage_rect;

    vec3 tile_sizes_x = get_tile_sizes(
        qi.edge_flags,
        EDGE_AA_LEFT,
        EDGE_AA_RIGHT,
        qi.tile_count.x,
        prim.bounds.p1.x - prim.bounds.p0.x
    );

    float f0 = get_size_for_tile_index(
        qi.tile_index.x,
        qi.tile_count.x,
        tile_sizes_x
    );
    float f1 = get_size_for_tile_index(
        qi.tile_index.x + 1,
        qi.tile_count.x,
        tile_sizes_x
    );
    local_coverage_rect.p0.x = mix(prim.bounds.p0.x, prim.bounds.p1.x, f0);
    local_coverage_rect.p1.x = mix(prim.bounds.p0.x, prim.bounds.p1.x, f1);

    vec3 tile_sizes_y = get_tile_sizes(
        qi.edge_flags,
        EDGE_AA_TOP,
        EDGE_AA_BOTTOM,
        qi.tile_count.y,
        prim.bounds.p1.y - prim.bounds.p0.y
    );

    f0 = get_size_for_tile_index(
        qi.tile_index.y,
        qi.tile_count.y,
        tile_sizes_y
    );
    f1 = get_size_for_tile_index(
        qi.tile_index.y + 1,
        qi.tile_count.y,
        tile_sizes_y
    );
    local_coverage_rect.p0.y = mix(prim.bounds.p0.y, prim.bounds.p1.y, f0);
    local_coverage_rect.p1.y = mix(prim.bounds.p0.y, prim.bounds.p1.y, f1);

    // Apply local clip rect
    local_coverage_rect.p0 = max(local_coverage_rect.p0, prim.clip.p0);
    local_coverage_rect.p1 = min(local_coverage_rect.p1, prim.clip.p1);
    local_coverage_rect.p1 = max(local_coverage_rect.p0, local_coverage_rect.p1);

#ifndef SWGL_ANTIALIAS
    if (qi.tile_index.x == 0) {
        local_coverage_rect.p0.x -= edge_aa_offset(EDGE_AA_LEFT, qi.edge_flags);
    } else if (qi.tile_index.x == qi.tile_count.x-1) {
        local_coverage_rect.p1.x += edge_aa_offset(EDGE_AA_RIGHT, qi.edge_flags);
    }

    if (qi.tile_index.y == 0) {
        local_coverage_rect.p0.y -= edge_aa_offset(EDGE_AA_TOP, qi.edge_flags);
    } else if (qi.tile_index.y == qi.tile_count.y-1) {
        local_coverage_rect.p1.y += edge_aa_offset(EDGE_AA_BOTTOM, qi.edge_flags);
    }
#endif

    vec2 local_pos = mix(local_coverage_rect.p0, local_coverage_rect.p1, aPosition);

    VertexInfo vi = write_vertex(
        local_pos,
        z,
        transform,
        task.content_origin,
        task.task_rect.p0,
        task.device_pixel_scale
    );

#ifdef SWGL_ANTIALIAS
    if (qi.tile_index.x != 0) {
        qi.edge_flags &= ~EDGE_AA_LEFT;
    }
    if (qi.tile_index.x != qi.tile_count.x-1) {
        qi.edge_flags &= ~EDGE_AA_RIGHT;
    }
    if (qi.tile_index.y != 0) {
        qi.edge_flags &= ~EDGE_AA_TOP;
    }
    if (qi.tile_index.y != qi.tile_count.y-1) {
        qi.edge_flags &= ~EDGE_AA_BOTTOM;
    }
    swgl_antiAlias(qi.edge_flags);
#else
    RectWithEndpoint xf_bounds = RectWithEndpoint(
        max(prim.bounds.p0, prim.clip.p0),
        min(prim.bounds.p1, prim.clip.p1)
    );
    vTransformBounds = vec4(xf_bounds.p0, xf_bounds.p1);

    vLocalPos = vi.local_pos;

    if (qi.edge_flags == 0) {
        v_flags.x = 0;
    } else {
        v_flags.x = 1;
    }
#endif
}
#endif

#ifdef WR_FRAGMENT_SHADER
void main(void) {
    vec4 color = v_color;

#ifndef SWGL_ANTIALIAS
    if (v_flags.x != 0) {
        float alpha = init_transform_fs(vLocalPos);
        color *= alpha;
    }
#endif

    oFragColor = color;
}

#if defined(SWGL_DRAW_SPAN)
void swgl_drawSpanRGBA8() {
    swgl_commitSolidRGBA8(v_color);
}

void swgl_drawSpanR8() {
    swgl_commitSolidR8(v_color.x);
}
#endif

#endif
