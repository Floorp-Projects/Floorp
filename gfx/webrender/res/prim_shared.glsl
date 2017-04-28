#line 1
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if defined(GL_ES)
    #if GL_ES == 1
        #ifdef GL_FRAGMENT_PRECISION_HIGH
        precision highp sampler2DArray;
        #else
        precision mediump sampler2DArray;
        #endif
    #endif
#endif

#define PST_TOP_LEFT     0
#define PST_TOP          1
#define PST_TOP_RIGHT    2
#define PST_RIGHT        3
#define PST_BOTTOM_RIGHT 4
#define PST_BOTTOM       5
#define PST_BOTTOM_LEFT  6
#define PST_LEFT         7

#define BORDER_LEFT      0
#define BORDER_TOP       1
#define BORDER_RIGHT     2
#define BORDER_BOTTOM    3

// Border styles as defined in webrender_traits/types.rs
#define BORDER_STYLE_NONE         0
#define BORDER_STYLE_SOLID        1
#define BORDER_STYLE_DOUBLE       2
#define BORDER_STYLE_DOTTED       3
#define BORDER_STYLE_DASHED       4
#define BORDER_STYLE_HIDDEN       5
#define BORDER_STYLE_GROOVE       6
#define BORDER_STYLE_RIDGE        7
#define BORDER_STYLE_INSET        8
#define BORDER_STYLE_OUTSET       9

#define UV_NORMALIZED    uint(0)
#define UV_PIXEL         uint(1)

#define EXTEND_MODE_CLAMP  0
#define EXTEND_MODE_REPEAT 1

uniform sampler2DArray sCacheA8;
uniform sampler2DArray sCacheRGBA8;

struct RectWithSize {
    vec2 p0;
    vec2 size;
};

struct RectWithEndpoint {
    vec2 p0;
    vec2 p1;
};

RectWithEndpoint to_rect_with_endpoint(RectWithSize rect) {
    RectWithEndpoint result;
    result.p0 = rect.p0;
    result.p1 = rect.p0 + rect.size;

    return result;
}

RectWithSize to_rect_with_size(RectWithEndpoint rect) {
    RectWithSize result;
    result.p0 = rect.p0;
    result.size = rect.p1 - rect.p0;

    return result;
}

vec2 clamp_rect(vec2 point, RectWithSize rect) {
    return clamp(point, rect.p0, rect.p0 + rect.size);
}

vec2 clamp_rect(vec2 point, RectWithEndpoint rect) {
    return clamp(point, rect.p0, rect.p1);
}

// Clamp 2 points at once.
vec4 clamp_rect(vec4 points, RectWithSize rect) {
    return clamp(points, rect.p0.xyxy, rect.p0.xyxy + rect.size.xyxy);
}

vec4 clamp_rect(vec4 points, RectWithEndpoint rect) {
    return clamp(points, rect.p0.xyxy, rect.p1.xyxy);
}

RectWithSize intersect_rect(RectWithSize a, RectWithSize b) {
    vec4 p = clamp_rect(vec4(a.p0, a.p0 + a.size), b);
    return RectWithSize(p.xy, max(vec2(0.0), p.zw - p.xy));
}

RectWithEndpoint intersect_rect(RectWithEndpoint a, RectWithEndpoint b) {
    vec4 p = clamp_rect(vec4(a.p0, a.p1), b);
    return RectWithEndpoint(p.xy, max(p.xy, p.zw));
}


// TODO: convert back to RectWithEndPoint if driver issues are resolved, if ever.
flat varying vec4 vClipMaskUvBounds;
varying vec3 vClipMaskUv;

#ifdef WR_VERTEX_SHADER

#define VECS_PER_LAYER             13
#define VECS_PER_RENDER_TASK        3
#define VECS_PER_PRIM_GEOM          2

uniform sampler2D sLayers;
uniform sampler2D sRenderTasks;
uniform sampler2D sPrimGeometry;

uniform sampler2D sData16;
uniform sampler2D sData32;
uniform sampler2D sData64;
uniform sampler2D sData128;
uniform sampler2D sResourceRects;

// Instanced attributes
in int aGlobalPrimId;
in int aPrimitiveAddress;
in int aTaskIndex;
in int aClipTaskIndex;
in int aLayerIndex;
in int aElementIndex;
in ivec2 aUserData;
in int aZIndex;

// get_fetch_uv is a macro to work around a macOS Intel driver parsing bug.
// TODO: convert back to a function once the driver issues are resolved, if ever.
// https://github.com/servo/webrender/pull/623
// https://github.com/servo/servo/issues/13953
#define get_fetch_uv(i, vpi)  ivec2(vpi * (i % (WR_MAX_VERTEX_TEXTURE_WIDTH/vpi)), i / (WR_MAX_VERTEX_TEXTURE_WIDTH/vpi))

vec4 fetch_data_1(int index) {
    ivec2 uv = get_fetch_uv(index, 1);
    return texelFetch(sData16, uv, 0);
}

vec4[2] fetch_data_2(int index) {
    ivec2 uv = get_fetch_uv(index, 2);
    return vec4[2](
        texelFetchOffset(sData32, uv, 0, ivec2(0, 0)),
        texelFetchOffset(sData32, uv, 0, ivec2(1, 0))
    );
}

vec4[4] fetch_data_4(int index) {
    ivec2 uv = get_fetch_uv(index, 4);
    return vec4[4](
        texelFetchOffset(sData64, uv, 0, ivec2(0, 0)),
        texelFetchOffset(sData64, uv, 0, ivec2(1, 0)),
        texelFetchOffset(sData64, uv, 0, ivec2(2, 0)),
        texelFetchOffset(sData64, uv, 0, ivec2(3, 0))
    );
}

vec4[8] fetch_data_8(int index) {
    ivec2 uv = get_fetch_uv(index, 8);
    return vec4[8](
        texelFetchOffset(sData128, uv, 0, ivec2(0, 0)),
        texelFetchOffset(sData128, uv, 0, ivec2(1, 0)),
        texelFetchOffset(sData128, uv, 0, ivec2(2, 0)),
        texelFetchOffset(sData128, uv, 0, ivec2(3, 0)),
        texelFetchOffset(sData128, uv, 0, ivec2(4, 0)),
        texelFetchOffset(sData128, uv, 0, ivec2(5, 0)),
        texelFetchOffset(sData128, uv, 0, ivec2(6, 0)),
        texelFetchOffset(sData128, uv, 0, ivec2(7, 0))
    );
}


struct Layer {
    mat4 transform;
    mat4 inv_transform;
    RectWithSize local_clip_rect;
    vec4 screen_vertices[4];
};

Layer fetch_layer(int index) {
    Layer layer;

    // Create a UV base coord for each 8 texels.
    // This is required because trying to use an offset
    // of more than 8 texels doesn't work on some versions
    // of OSX.
    ivec2 uv = get_fetch_uv(index, VECS_PER_LAYER);
    ivec2 uv0 = ivec2(uv.x + 0, uv.y);
    ivec2 uv1 = ivec2(uv.x + 8, uv.y);

    layer.transform[0] = texelFetchOffset(sLayers, uv0, 0, ivec2(0, 0));
    layer.transform[1] = texelFetchOffset(sLayers, uv0, 0, ivec2(1, 0));
    layer.transform[2] = texelFetchOffset(sLayers, uv0, 0, ivec2(2, 0));
    layer.transform[3] = texelFetchOffset(sLayers, uv0, 0, ivec2(3, 0));

    layer.inv_transform[0] = texelFetchOffset(sLayers, uv0, 0, ivec2(4, 0));
    layer.inv_transform[1] = texelFetchOffset(sLayers, uv0, 0, ivec2(5, 0));
    layer.inv_transform[2] = texelFetchOffset(sLayers, uv0, 0, ivec2(6, 0));
    layer.inv_transform[3] = texelFetchOffset(sLayers, uv0, 0, ivec2(7, 0));

    vec4 clip_rect = texelFetchOffset(sLayers, uv1, 0, ivec2(0, 0));
    layer.local_clip_rect = RectWithSize(clip_rect.xy, clip_rect.zw);

    layer.screen_vertices[0] = texelFetchOffset(sLayers, uv1, 0, ivec2(1, 0));
    layer.screen_vertices[1] = texelFetchOffset(sLayers, uv1, 0, ivec2(2, 0));
    layer.screen_vertices[2] = texelFetchOffset(sLayers, uv1, 0, ivec2(3, 0));
    layer.screen_vertices[3] = texelFetchOffset(sLayers, uv1, 0, ivec2(4, 0));

    return layer;
}

struct RenderTaskData {
    vec4 data0;
    vec4 data1;
    vec4 data2;
};

RenderTaskData fetch_render_task(int index) {
    RenderTaskData task;

    ivec2 uv = get_fetch_uv(index, VECS_PER_RENDER_TASK);

    task.data0 = texelFetchOffset(sRenderTasks, uv, 0, ivec2(0, 0));
    task.data1 = texelFetchOffset(sRenderTasks, uv, 0, ivec2(1, 0));
    task.data2 = texelFetchOffset(sRenderTasks, uv, 0, ivec2(2, 0));

    return task;
}

struct AlphaBatchTask {
    vec2 screen_space_origin;
    vec2 render_target_origin;
    vec2 size;
    float render_target_layer_index;
};

AlphaBatchTask fetch_alpha_batch_task(int index) {
    RenderTaskData data = fetch_render_task(index);

    AlphaBatchTask task;
    task.render_target_origin = data.data0.xy;
    task.size = data.data0.zw;
    task.screen_space_origin = data.data1.xy;
    task.render_target_layer_index = data.data1.z;

    return task;
}

struct ClipArea {
    vec4 task_bounds;
    vec4 screen_origin_target_index;
    vec4 inner_rect;
};

ClipArea fetch_clip_area(int index) {
    ClipArea area;

    if (index == 0x7FFFFFFF) { //special sentinel task index
        area.task_bounds = vec4(0.0, 0.0, 0.0, 0.0);
        area.screen_origin_target_index = vec4(0.0, 0.0, 0.0, 0.0);
        area.inner_rect = vec4(0.0);
    } else {
        RenderTaskData task = fetch_render_task(index);
        area.task_bounds = task.data0;
        area.screen_origin_target_index = task.data1;
        area.inner_rect = task.data2;
    }

    return area;
}

struct Gradient {
    vec4 start_end_point;
    vec4 tile_size_repeat;
    vec4 extend_mode;
};

Gradient fetch_gradient(int index) {
    vec4 data[4] = fetch_data_4(index);
    return Gradient(data[0], data[1], data[2]);
}

struct GradientStop {
    vec4 color;
    vec4 offset;
};

GradientStop fetch_gradient_stop(int index) {
    vec4 data[2] = fetch_data_2(index);
    return GradientStop(data[0], data[1]);
}

struct RadialGradient {
    vec4 start_end_center;
    vec4 start_end_radius_ratio_xy_extend_mode;
    vec4 tile_size_repeat;
};

RadialGradient fetch_radial_gradient(int index) {
    vec4 data[4] = fetch_data_4(index);
    return RadialGradient(data[0], data[1], data[2]);
}

struct Border {
    vec4 style;
    vec4 widths;
    vec4 colors[4];
    vec4 radii[2];
};

vec4 get_effective_border_widths(Border border) {
    switch (int(border.style.x)) {
        case BORDER_STYLE_DOUBLE:
            // Calculate the width of a border segment in a style: double
            // border. Round to the nearest CSS pixel.

            // The CSS spec doesn't define what width each of the segments
            // in a style: double border should be. It only says that the
            // sum of the segments should be equal to the total border
            // width. We pick to make the segments (almost) equal thirds
            // for now - we can adjust this if we find other browsers pick
            // different values in some cases.
            // SEE: https://drafts.csswg.org/css-backgrounds-3/#double
            return floor(0.5 + border.widths / 3.0);
        case BORDER_STYLE_GROOVE:
        case BORDER_STYLE_RIDGE:
            return floor(0.5 + border.widths * 0.5);
        default:
            return border.widths;
    }
}

Border fetch_border(int index) {
    vec4 data[8] = fetch_data_8(index);
    return Border(data[0], data[1],
                  vec4[4](data[2], data[3], data[4], data[5]),
                  vec4[2](data[6], data[7]));
}

struct BorderCorners {
    vec2 tl_outer;
    vec2 tl_inner;
    vec2 tr_outer;
    vec2 tr_inner;
    vec2 br_outer;
    vec2 br_inner;
    vec2 bl_outer;
    vec2 bl_inner;
};

BorderCorners get_border_corners(Border border, RectWithSize local_rect) {
    vec2 tl_outer = local_rect.p0;
    vec2 tl_inner = tl_outer + vec2(max(border.radii[0].x, border.widths.x),
                                    max(border.radii[0].y, border.widths.y));

    vec2 tr_outer = vec2(local_rect.p0.x + local_rect.size.x,
                         local_rect.p0.y);
    vec2 tr_inner = tr_outer + vec2(-max(border.radii[0].z, border.widths.z),
                                    max(border.radii[0].w, border.widths.y));

    vec2 br_outer = vec2(local_rect.p0.x + local_rect.size.x,
                         local_rect.p0.y + local_rect.size.y);
    vec2 br_inner = br_outer - vec2(max(border.radii[1].x, border.widths.z),
                                    max(border.radii[1].y, border.widths.w));

    vec2 bl_outer = vec2(local_rect.p0.x,
                         local_rect.p0.y + local_rect.size.y);
    vec2 bl_inner = bl_outer + vec2(max(border.radii[1].z, border.widths.x),
                                    -max(border.radii[1].w, border.widths.w));

    return BorderCorners(
        tl_outer,
        tl_inner,
        tr_outer,
        tr_inner,
        br_outer,
        br_inner,
        bl_outer,
        bl_inner
    );
}

struct Glyph {
    vec4 offset;
};

Glyph fetch_glyph(int index) {
    vec4 data = fetch_data_1(index);
    return Glyph(data);
}

RectWithSize fetch_instance_geometry(int index) {
    vec4 data = fetch_data_1(index);
    return RectWithSize(data.xy, data.zw);
}

struct PrimitiveGeometry {
    RectWithSize local_rect;
    RectWithSize local_clip_rect;
};

PrimitiveGeometry fetch_prim_geometry(int index) {
    PrimitiveGeometry pg;

    ivec2 uv = get_fetch_uv(index, VECS_PER_PRIM_GEOM);

    vec4 local_rect = texelFetchOffset(sPrimGeometry, uv, 0, ivec2(0, 0));
    pg.local_rect = RectWithSize(local_rect.xy, local_rect.zw);
    vec4 local_clip_rect = texelFetchOffset(sPrimGeometry, uv, 0, ivec2(1, 0));
    pg.local_clip_rect = RectWithSize(local_clip_rect.xy, local_clip_rect.zw);

    return pg;
}

struct PrimitiveInstance {
    int global_prim_index;
    int specific_prim_index;
    int render_task_index;
    int clip_task_index;
    int layer_index;
    int sub_index;
    int z;
    ivec2 user_data;
};

PrimitiveInstance fetch_prim_instance() {
    PrimitiveInstance pi;

    pi.global_prim_index = aGlobalPrimId;
    pi.specific_prim_index = aPrimitiveAddress;
    pi.render_task_index = aTaskIndex;
    pi.clip_task_index = aClipTaskIndex;
    pi.layer_index = aLayerIndex;
    pi.sub_index = aElementIndex;
    pi.user_data = aUserData;
    pi.z = aZIndex;

    return pi;
}

struct CachePrimitiveInstance {
    int global_prim_index;
    int specific_prim_index;
    int render_task_index;
    int sub_index;
    ivec2 user_data;
};

CachePrimitiveInstance fetch_cache_instance() {
    CachePrimitiveInstance cpi;

    PrimitiveInstance pi = fetch_prim_instance();

    cpi.global_prim_index = pi.global_prim_index;
    cpi.specific_prim_index = pi.specific_prim_index;
    cpi.render_task_index = pi.render_task_index;
    cpi.sub_index = pi.sub_index;
    cpi.user_data = pi.user_data;

    return cpi;
}

struct Primitive {
    Layer layer;
    ClipArea clip_area;
    AlphaBatchTask task;
    RectWithSize local_rect;
    RectWithSize local_clip_rect;
    int prim_index;
    // when sending multiple primitives of the same type (e.g. border segments)
    // this index allows the vertex shader to recognize the difference
    int sub_index;
    ivec2 user_data;
    float z;
};

Primitive load_primitive_custom(PrimitiveInstance pi) {
    Primitive prim;

    prim.layer = fetch_layer(pi.layer_index);
    prim.clip_area = fetch_clip_area(pi.clip_task_index);
    prim.task = fetch_alpha_batch_task(pi.render_task_index);

    PrimitiveGeometry pg = fetch_prim_geometry(pi.global_prim_index);
    prim.local_rect = pg.local_rect;
    prim.local_clip_rect = pg.local_clip_rect;

    prim.prim_index = pi.specific_prim_index;
    prim.sub_index = pi.sub_index;
    prim.user_data = pi.user_data;
    prim.z = float(pi.z);

    return prim;
}

Primitive load_primitive() {
    PrimitiveInstance pi = fetch_prim_instance();

    return load_primitive_custom(pi);
}


// Return the intersection of the plane (set up by "normal" and "point")
// with the ray (set up by "ray_origin" and "ray_dir"),
// writing the resulting scaler into "t".
bool ray_plane(vec3 normal, vec3 point, vec3 ray_origin, vec3 ray_dir, out float t)
{
    float denom = dot(normal, ray_dir);
    if (denom > 1e-6) {
        vec3 d = point - ray_origin;
        t = dot(d, normal) / denom;
        return t >= 0.0;
    }

    return false;
}

// Apply the inverse transform "inv_transform"
// to the reference point "ref" in CSS space,
// producing a local point on a layer plane,
// set by a base point "a" and a normal "n".
vec4 untransform(vec2 ref, vec3 n, vec3 a, mat4 inv_transform) {
    vec3 p = vec3(ref, -10000.0);
    vec3 d = vec3(0, 0, 1.0);

    float t = 0.0;
    // get an intersection of the layer plane with Z axis vector,
    // originated from the "ref" point
    ray_plane(n, a, p, d, t);
    float z = p.z + d.z * t; // Z of the visible point on the layer

    vec4 r = inv_transform * vec4(ref, z, 1.0);
    return r;
}

// Given a CSS space position, transform it back into the layer space.
vec4 get_layer_pos(vec2 pos, Layer layer) {
    // get 3 of the layer corners in CSS space
    vec3 a = layer.screen_vertices[0].xyz / layer.screen_vertices[0].w;
    vec3 b = layer.screen_vertices[3].xyz / layer.screen_vertices[3].w;
    vec3 c = layer.screen_vertices[2].xyz / layer.screen_vertices[2].w;
    // get the normal to the layer plane
    vec3 n = normalize(cross(b-a, c-a));
    return untransform(pos, n, a, layer.inv_transform);
}

struct VertexInfo {
    vec2 local_pos;
    vec2 screen_pos;
};

VertexInfo write_vertex(RectWithSize instance_rect,
                        RectWithSize local_clip_rect,
                        float z,
                        Layer layer,
                        AlphaBatchTask task,
                        vec2 snap_ref) {
    // Select the corner of the local rect that we are processing.
    vec2 local_pos = instance_rect.p0 + instance_rect.size * aPosition.xy;

    // xy = top left corner of the local rect, zw = position of current vertex.
    vec4 local_p0_pos = vec4(snap_ref, local_pos);

    // Clamp to the two local clip rects.
    local_p0_pos = clamp_rect(local_p0_pos, local_clip_rect);
    local_p0_pos = clamp_rect(local_p0_pos, layer.local_clip_rect);

    // Transform the top corner and current vertex to world space.
    vec4 world_p0 = layer.transform * vec4(local_p0_pos.xy, 0.0, 1.0);
    world_p0.xyz /= world_p0.w;
    vec4 world_pos = layer.transform * vec4(local_p0_pos.zw, 0.0, 1.0);
    world_pos.xyz /= world_pos.w;

    // Convert the world positions to device pixel space. xy=top left corner. zw=current vertex.
    vec4 device_p0_pos = vec4(world_p0.xy, world_pos.xy) * uDevicePixelRatio;

    // Calculate the distance to snap the vertex by (snap top left corner).
    vec2 snap_delta = device_p0_pos.xy - floor(device_p0_pos.xy + 0.5);

    // Apply offsets for the render task to get correct screen location.
    vec2 final_pos = device_p0_pos.zw -
                     snap_delta -
                     task.screen_space_origin +
                     task.render_target_origin;

    gl_Position = uTransform * vec4(final_pos, z, 1.0);

    VertexInfo vi = VertexInfo(local_p0_pos.zw, device_p0_pos.zw);
    return vi;
}

#ifdef WR_FEATURE_TRANSFORM

struct TransformVertexInfo {
    vec3 local_pos;
    vec2 screen_pos;
};

float cross2(vec2 v0, vec2 v1) {
    return v0.x * v1.y - v0.y * v1.x;
}

// Return intersection of line (p0,p1) and line (p2,p3)
vec2 intersect_lines(vec2 p0, vec2 p1, vec2 p2, vec2 p3) {
    vec2 d0 = p0 - p1;
    vec2 d1 = p2 - p3;

    float s0 = cross2(p0, p1);
    float s1 = cross2(p2, p3);

    float d = cross2(d0, d1);
    float nx = s0 * d1.x - d0.x * s1;
    float ny = s0 * d1.y - d0.y * s1;

    return vec2(nx / d, ny / d);
}

TransformVertexInfo write_transform_vertex(RectWithSize instance_rect,
                                           RectWithSize local_clip_rect,
                                           float z,
                                           Layer layer,
                                           AlphaBatchTask task,
                                           vec2 snap_ref) {
    RectWithEndpoint local_rect = to_rect_with_endpoint(instance_rect);

    vec2 current_local_pos, prev_local_pos, next_local_pos;

    // Select the current vertex and the previous/next vertices,
    // based on the vertex ID that is known based on the instance rect.
    switch (gl_VertexID) {
        case 0:
            current_local_pos = vec2(local_rect.p0.x, local_rect.p0.y);
            next_local_pos = vec2(local_rect.p0.x, local_rect.p1.y);
            prev_local_pos = vec2(local_rect.p1.x, local_rect.p0.y);
            break;
        case 1:
            current_local_pos = vec2(local_rect.p1.x, local_rect.p0.y);
            next_local_pos = vec2(local_rect.p0.x, local_rect.p0.y);
            prev_local_pos = vec2(local_rect.p1.x, local_rect.p1.y);
            break;
        case 2:
            current_local_pos = vec2(local_rect.p0.x, local_rect.p1.y);
            prev_local_pos = vec2(local_rect.p0.x, local_rect.p0.y);
            next_local_pos = vec2(local_rect.p1.x, local_rect.p1.y);
            break;
        case 3:
            current_local_pos = vec2(local_rect.p1.x, local_rect.p1.y);
            prev_local_pos = vec2(local_rect.p0.x, local_rect.p1.y);
            next_local_pos = vec2(local_rect.p1.x, local_rect.p0.y);
            break;
    }

    // Transform them to world space
    vec4 current_world_pos = layer.transform * vec4(current_local_pos, 0.0, 1.0);
    vec4 prev_world_pos = layer.transform * vec4(prev_local_pos, 0.0, 1.0);
    vec4 next_world_pos = layer.transform * vec4(next_local_pos, 0.0, 1.0);

    // Convert to device space
    vec2 current_device_pos = uDevicePixelRatio * current_world_pos.xy / current_world_pos.w;
    vec2 prev_device_pos = uDevicePixelRatio * prev_world_pos.xy / prev_world_pos.w;
    vec2 next_device_pos = uDevicePixelRatio * next_world_pos.xy / next_world_pos.w;

    // Get the normals of each of the vectors between the current and next/prev vertices.
    const float amount = 2.0;
    vec2 dir_prev = normalize(current_device_pos - prev_device_pos);
    vec2 dir_next = normalize(current_device_pos - next_device_pos);
    vec2 norm_prev = vec2(-dir_prev.y,  dir_prev.x);
    vec2 norm_next = vec2( dir_next.y, -dir_next.x);

    // Push those lines out along the normal by a specific amount of device pixels.
    vec2 adjusted_prev_p0 = current_device_pos + norm_prev * amount;
    vec2 adjusted_prev_p1 = prev_device_pos + norm_prev * amount;
    vec2 adjusted_next_p0 = current_device_pos + norm_next * amount;
    vec2 adjusted_next_p1 = next_device_pos + norm_next * amount;

    // Intersect those adjusted lines to find the actual vertex position.
    vec2 device_pos = intersect_lines(adjusted_prev_p0,
                                      adjusted_prev_p1,
                                      adjusted_next_p0,
                                      adjusted_next_p1);

    // Calculate the snap amount based on the first vertex as a reference point.
    vec4 world_p0 = layer.transform * vec4(snap_ref, 0.0, 1.0);
    vec2 device_p0 = uDevicePixelRatio * world_p0.xy / world_p0.w;
    vec2 snap_delta = device_p0 - floor(device_p0 + 0.5);

    // Apply offsets for the render task to get correct screen location.
    vec2 final_pos = device_pos -
                     snap_delta -
                     task.screen_space_origin +
                     task.render_target_origin;

    gl_Position = uTransform * vec4(final_pos, z, 1.0);

    vec4 layer_pos = get_layer_pos(device_pos / uDevicePixelRatio, layer);

    return TransformVertexInfo(layer_pos.xyw, device_pos);
}

#endif //WR_FEATURE_TRANSFORM

struct ResourceRect {
    vec4 uv_rect;
};

ResourceRect fetch_resource_rect(int index) {
    ResourceRect rect;

    ivec2 uv = get_fetch_uv(index, 1);

    rect.uv_rect = texelFetchOffset(sResourceRects, uv, 0, ivec2(0, 0));

    return rect;
}

struct Rectangle {
    vec4 color;
};

Rectangle fetch_rectangle(int index) {
    vec4 data = fetch_data_1(index);
    return Rectangle(data);
}

struct TextRun {
    vec4 color;
};

TextRun fetch_text_run(int index) {
    vec4 data = fetch_data_1(index);
    return TextRun(data);
}

struct Image {
    vec4 stretch_size_and_tile_spacing;  // Size of the actual image and amount of space between
                                         //     tiled instances of this image.
};

Image fetch_image(int index) {
    vec4 data = fetch_data_1(index);
    return Image(data);
}

struct YuvImage {
    vec2 size;
};

YuvImage fetch_yuv_image(int index) {
    vec4 data = fetch_data_1(index);
    return YuvImage(data.xy);
}

struct BoxShadow {
    vec4 src_rect;
    vec4 bs_rect;
    vec4 color;
    vec4 border_radius_edge_size_blur_radius_inverted;
};

BoxShadow fetch_boxshadow(int index) {
    vec4 data[4] = fetch_data_4(index);
    return BoxShadow(data[0], data[1], data[2], data[3]);
}

void write_clip(vec2 global_pos, ClipArea area) {
    vec2 texture_size = vec2(textureSize(sCacheA8, 0).xy);
    vec2 uv = global_pos + area.task_bounds.xy - area.screen_origin_target_index.xy;
    vClipMaskUvBounds = area.task_bounds / texture_size.xyxy;
    vClipMaskUv = vec3(uv / texture_size, area.screen_origin_target_index.z);
}
#endif //WR_VERTEX_SHADER

#ifdef WR_FRAGMENT_SHADER
float signed_distance_rect(vec2 pos, vec2 p0, vec2 p1) {
    vec2 d = max(p0 - pos, pos - p1);
    return length(max(vec2(0.0), d)) + min(0.0, max(d.x, d.y));
}

vec2 init_transform_fs(vec3 local_pos, RectWithSize local_rect, out float fragment_alpha) {
    fragment_alpha = 1.0;
    vec2 pos = local_pos.xy / local_pos.z;

    // Because the local rect is placed on whole coordinates, but the interpolation
    // occurs at pixel centers, we need to offset the signed distance by that amount.
    // In the simple case of no zoom, and no transform, this is 0.5. However, we
    // need to scale this by the amount that the local rect is changing by per
    // fragment, based on the current zoom and transform.
    vec2 fw = fwidth(pos.xy);
    vec2 dxdy = 0.5 * fw;

    // Now get the actual signed distance. Inset the local rect by the offset amount
    // above to get correct distance values. This ensures that we only apply
    // anti-aliasing when the fragment has partial coverage.
    float d = signed_distance_rect(pos,
                                   local_rect.p0 + dxdy,
                                   local_rect.p0 + local_rect.size - dxdy);

    // Find the appropriate distance to apply the AA smoothstep over.
    float afwidth = 0.5 / length(fw);

    // Only apply AA to fragments outside the signed distance field.
    fragment_alpha = 1.0 - smoothstep(0.0, afwidth, d);

    return pos;
}

float do_clip() {
    // anything outside of the mask is considered transparent
    bvec4 inside = lessThanEqual(
        vec4(vClipMaskUvBounds.xy, vClipMaskUv.xy),
        vec4(vClipMaskUv.xy, vClipMaskUvBounds.zw));
    // check for the dummy bounds, which are given to the opaque objects
    return vClipMaskUvBounds.xy == vClipMaskUvBounds.zw ? 1.0:
        all(inside) ? textureLod(sCacheA8, vClipMaskUv, 0.0).r : 0.0;
}

#ifdef WR_FEATURE_DITHERING
vec4 dither(vec4 color) {
    const int matrix_mask = 7;

    ivec2 pos = ivec2(gl_FragCoord.xy) & ivec2(matrix_mask);
    float noise_normalized = (texelFetch(sDither, pos, 0).r * 255.0 + 0.5) / 64.0;
    float noise = (noise_normalized - 0.5) / 256.0; // scale down to the unit length

    return color + vec4(noise, noise, noise, 0);
}
#else
vec4 dither(vec4 color) {
    return color;
}
#endif

#endif //WR_FRAGMENT_SHADER
