/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include rect,clip_scroll,render_task,resource_cache,snap,transform

#define EXTEND_MODE_CLAMP  0
#define EXTEND_MODE_REPEAT 1

#define SUBPX_DIR_NONE        0
#define SUBPX_DIR_HORIZONTAL  1
#define SUBPX_DIR_VERTICAL    2

#define RASTER_LOCAL            0
#define RASTER_SCREEN           1

uniform sampler2DArray sCacheA8;
uniform sampler2DArray sCacheRGBA8;

// An A8 target for standalone tasks that is available to all passes.
uniform sampler2DArray sSharedCacheA8;

uniform sampler2D sGradients;

vec2 clamp_rect(vec2 pt, RectWithSize rect) {
    return clamp(pt, rect.p0, rect.p0 + rect.size);
}

float distance_to_line(vec2 p0, vec2 perp_dir, vec2 p) {
    vec2 dir_to_p0 = p0 - p;
    return dot(normalize(perp_dir), dir_to_p0);
}

// TODO: convert back to RectWithEndPoint if driver issues are resolved, if ever.
flat varying vec4 vClipMaskUvBounds;
varying vec3 vClipMaskUv;


#ifdef WR_VERTEX_SHADER

#define VECS_PER_LOCAL_CLIP_RECT    1
#define VECS_PER_PRIM_HEADER        2
#define VECS_PER_TEXT_RUN           3
#define VECS_PER_GRADIENT_STOP      2

uniform HIGHP_SAMPLER_FLOAT sampler2D sLocalClipRects;

// Instanced attributes
in ivec4 aData0;
in ivec4 aData1;

RectWithSize fetch_clip_chain_rect(int index) {
    ivec2 uv = get_fetch_uv(index, VECS_PER_LOCAL_CLIP_RECT);
    vec4 rect = TEXEL_FETCH(sLocalClipRects, uv, 0, ivec2(0, 0));
    return RectWithSize(rect.xy, rect.zw);
}

struct Glyph {
    vec2 offset;
};

Glyph fetch_glyph(int specific_prim_address,
                  int glyph_index,
                  int subpx_dir) {
    // Two glyphs are packed in each texel in the GPU cache.
    int glyph_address = specific_prim_address +
                        VECS_PER_TEXT_RUN +
                        glyph_index / 2;
    vec4 data = fetch_from_resource_cache_1(glyph_address);
    // Select XY or ZW based on glyph index.
    // We use "!= 0" instead of "== 1" here in order to work around a driver
    // bug with equality comparisons on integers.
    vec2 glyph = mix(data.xy, data.zw, bvec2(glyph_index % 2 != 0));

    // In subpixel mode, the subpixel offset has already been
    // accounted for while rasterizing the glyph.
    switch (subpx_dir) {
        case SUBPX_DIR_NONE:
            break;
        case SUBPX_DIR_HORIZONTAL:
            // Glyphs positioned [-0.125, 0.125] get a
            // subpx position of zero. So include that
            // offset in the glyph position to ensure
            // we round to the correct whole position.
            glyph.x = floor(glyph.x + 0.125);
            break;
        case SUBPX_DIR_VERTICAL:
            glyph.y = floor(glyph.y + 0.125);
            break;
        default: break;
    }

    return Glyph(glyph);
}

struct PrimitiveInstance {
    int prim_address;
    int specific_prim_address;
    int render_task_index;
    int clip_task_index;
    int scroll_node_id;
    int clip_chain_rect_index;
    int z;
    int user_data0;
    int user_data1;
    int user_data2;
};

PrimitiveInstance fetch_prim_instance() {
    PrimitiveInstance pi;

    pi.prim_address = aData0.x;
    pi.specific_prim_address = pi.prim_address + VECS_PER_PRIM_HEADER;
    pi.render_task_index = aData0.y;
    pi.clip_task_index = aData0.z;
    pi.clip_chain_rect_index = aData0.w / 65536;
    pi.scroll_node_id = aData0.w % 65536;
    pi.z = aData1.x;
    pi.user_data0 = aData1.y;
    pi.user_data1 = aData1.z;
    pi.user_data2 = aData1.w;

    return pi;
}

struct CompositeInstance {
    int render_task_index;
    int src_task_index;
    int backdrop_task_index;
    int user_data0;
    int user_data1;
    float z;
    int user_data2;
    int user_data3;
};

CompositeInstance fetch_composite_instance() {
    CompositeInstance ci;

    ci.render_task_index = aData0.x;
    ci.src_task_index = aData0.y;
    ci.backdrop_task_index = aData0.z;
    ci.z = float(aData0.w);

    ci.user_data0 = aData1.x;
    ci.user_data1 = aData1.y;
    ci.user_data2 = aData1.z;
    ci.user_data3 = aData1.w;

    return ci;
}

struct Primitive {
    ClipScrollNode scroll_node;
    ClipArea clip_area;
    PictureTask task;
    RectWithSize local_rect;
    RectWithSize local_clip_rect;
    int specific_prim_address;
    int user_data0;
    int user_data1;
    int user_data2;
    float z;
};

struct PrimitiveGeometry {
    RectWithSize local_rect;
    RectWithSize local_clip_rect;
};

PrimitiveGeometry fetch_primitive_geometry(int address) {
    vec4 geom[2] = fetch_from_resource_cache_2(address);
    return PrimitiveGeometry(RectWithSize(geom[0].xy, geom[0].zw),
                             RectWithSize(geom[1].xy, geom[1].zw));
}

Primitive load_primitive() {
    PrimitiveInstance pi = fetch_prim_instance();

    Primitive prim;

    prim.scroll_node = fetch_clip_scroll_node(pi.scroll_node_id);
    prim.clip_area = fetch_clip_area(pi.clip_task_index);
    prim.task = fetch_picture_task(pi.render_task_index);

    RectWithSize clip_chain_rect = fetch_clip_chain_rect(pi.clip_chain_rect_index);

    PrimitiveGeometry geom = fetch_primitive_geometry(pi.prim_address);
    prim.local_rect = geom.local_rect;
    prim.local_clip_rect = intersect_rects(clip_chain_rect, geom.local_clip_rect);

    prim.specific_prim_address = pi.specific_prim_address;
    prim.user_data0 = pi.user_data0;
    prim.user_data1 = pi.user_data1;
    prim.user_data2 = pi.user_data2;
    prim.z = float(pi.z);

    return prim;
}


struct VertexInfo {
    vec2 local_pos;
    vec2 screen_pos;
    float w;
    vec2 snapped_device_pos;
};

VertexInfo write_vertex(RectWithSize instance_rect,
                        RectWithSize local_clip_rect,
                        float z,
                        ClipScrollNode scroll_node,
                        PictureTask task,
                        RectWithSize snap_rect) {

    // Select the corner of the local rect that we are processing.
    vec2 local_pos = instance_rect.p0 + instance_rect.size * aPosition.xy;

    // Clamp to the two local clip rects.
    vec2 clamped_local_pos = clamp_rect(local_pos, local_clip_rect);

    /// Compute the snapping offset.
    vec2 snap_offset = compute_snap_offset(
        clamped_local_pos,
        scroll_node.transform,
        snap_rect
    );

    // Transform the current vertex to world space.
    vec4 world_pos = scroll_node.transform * vec4(clamped_local_pos, 0.0, 1.0);

    // Convert the world positions to device pixel space.
    vec2 device_pos = world_pos.xy / world_pos.w * uDevicePixelRatio;

    // Apply offsets for the render task to get correct screen location.
    vec2 snapped_device_pos = device_pos + snap_offset;
    vec2 final_pos = snapped_device_pos -
                     task.content_origin +
                     task.common_data.task_rect.p0;

    gl_Position = uTransform * vec4(final_pos, z, 1.0);

    VertexInfo vi = VertexInfo(
        clamped_local_pos,
        device_pos,
        world_pos.w,
        snapped_device_pos
    );

    return vi;
}

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

VertexInfo write_transform_vertex(RectWithSize local_segment_rect,
                                  RectWithSize local_prim_rect,
                                  RectWithSize local_clip_rect,
                                  vec4 clip_edge_mask,
                                  float z,
                                  ClipScrollNode scroll_node,
                                  PictureTask task,
                                  bool do_perspective_interpolation) {
    // Calculate a clip rect from local_rect + local clip
    RectWithEndpoint clip_rect = to_rect_with_endpoint(local_clip_rect);
    RectWithEndpoint segment_rect = to_rect_with_endpoint(local_segment_rect);
    segment_rect.p0 = clamp(segment_rect.p0, clip_rect.p0, clip_rect.p1);
    segment_rect.p1 = clamp(segment_rect.p1, clip_rect.p0, clip_rect.p1);

    // Calculate a clip rect from local_rect + local clip
    RectWithEndpoint prim_rect = to_rect_with_endpoint(local_prim_rect);
    prim_rect.p0 = clamp(prim_rect.p0, clip_rect.p0, clip_rect.p1);
    prim_rect.p1 = clamp(prim_rect.p1, clip_rect.p0, clip_rect.p1);

    // As this is a transform shader, extrude by 2 (local space) pixels
    // in each direction. This gives enough space around the edge to
    // apply distance anti-aliasing. Technically, it:
    // (a) slightly over-estimates the number of required pixels in the simple case.
    // (b) might not provide enough edge in edge case perspective projections.
    // However, it's fast and simple. If / when we ever run into issues, we
    // can do some math on the projection matrix to work out a variable
    // amount to extrude.

    // Only extrude along edges where we are going to apply AA.
    float extrude_amount = 2.0;
    vec4 extrude_distance = vec4(extrude_amount) * clip_edge_mask;
    local_segment_rect.p0 -= extrude_distance.xy;
    local_segment_rect.size += extrude_distance.xy + extrude_distance.zw;

    // Select the corner of the local rect that we are processing.
    vec2 local_pos = local_segment_rect.p0 + local_segment_rect.size * aPosition.xy;

    // Transform the current vertex to the world cpace.
    vec4 world_pos = scroll_node.transform * vec4(local_pos, 0.0, 1.0);

    // Convert the world positions to device pixel space.
    vec2 device_pos = world_pos.xy / world_pos.w * uDevicePixelRatio;
    vec2 task_offset = task.common_data.task_rect.p0 - task.content_origin;

    // Force w = 1, if we don't want perspective interpolation (for
    // example, drawing a screen-space quad on an element with a
    // perspective transform).
    world_pos.w = mix(1.0, world_pos.w, do_perspective_interpolation);

    // We want the world space coords to be perspective divided by W.
    // We also want that to apply to any interpolators. However, we
    // want a constant Z across the primitive, since we're using it
    // for draw ordering - so scale by the W coord to ensure this.
    vec4 final_pos = vec4(device_pos + task_offset, z, 1.0) * world_pos.w;
    gl_Position = uTransform * final_pos;

    init_transform_vs(mix(
        vec4(prim_rect.p0, prim_rect.p1),
        vec4(segment_rect.p0, segment_rect.p1),
        clip_edge_mask
    ));

    VertexInfo vi = VertexInfo(
        local_pos,
        device_pos,
        world_pos.w,
        device_pos
    );

    return vi;
}

VertexInfo write_transform_vertex_primitive(Primitive prim) {
    return write_transform_vertex(
        prim.local_rect,
        prim.local_rect,
        prim.local_clip_rect,
        vec4(1.0),
        prim.z,
        prim.scroll_node,
        prim.task,
        true
    );
}

struct GlyphResource {
    vec4 uv_rect;
    float layer;
    vec2 offset;
    float scale;
};

GlyphResource fetch_glyph_resource(int address) {
    vec4 data[2] = fetch_from_resource_cache_2(address);
    return GlyphResource(data[0], data[1].x, data[1].yz, data[1].w);
}

struct TextRun {
    vec4 color;
    vec4 bg_color;
    vec2 offset;
};

TextRun fetch_text_run(int address) {
    vec4 data[3] = fetch_from_resource_cache_3(address);
    return TextRun(data[0], data[1], data[2].xy);
}

struct Image {
    vec4 stretch_size_and_tile_spacing;  // Size of the actual image and amount of space between
                                         //     tiled instances of this image.
};

Image fetch_image(int address) {
    vec4 data = fetch_from_resource_cache_1(address);
    return Image(data);
}

void write_clip(vec2 global_pos, ClipArea area) {
    vec2 uv = global_pos +
              area.common_data.task_rect.p0 -
              area.screen_origin;
    vClipMaskUvBounds = vec4(
        area.common_data.task_rect.p0,
        area.common_data.task_rect.p0 + area.common_data.task_rect.size
    );
    vClipMaskUv = vec3(uv, area.common_data.texture_layer_index);
}
#endif //WR_VERTEX_SHADER

#ifdef WR_FRAGMENT_SHADER

float do_clip() {
    // anything outside of the mask is considered transparent
    bvec4 inside = lessThanEqual(
        vec4(vClipMaskUvBounds.xy, vClipMaskUv.xy),
        vec4(vClipMaskUv.xy, vClipMaskUvBounds.zw));
    // check for the dummy bounds, which are given to the opaque objects
    return vClipMaskUvBounds.xy == vClipMaskUvBounds.zw ? 1.0:
        all(inside) ? texelFetch(sCacheA8, ivec3(vClipMaskUv), 0).r : 0.0;
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
#endif //WR_FEATURE_DITHERING

vec4 sample_gradient(int address, float offset, float gradient_repeat) {
    // Modulo the offset if the gradient repeats.
    float x = mix(offset, fract(offset), gradient_repeat);

    // Calculate the color entry index to use for this offset:
    //     offsets < 0 use the first color entry, 0
    //     offsets from [0, 1) use the color entries in the range of [1, N-1)
    //     offsets >= 1 use the last color entry, N-1
    //     so transform the range [0, 1) -> [1, N-1)

    // TODO(gw): In the future we might consider making the size of the
    // LUT vary based on number / distribution of stops in the gradient.
    const int GRADIENT_ENTRIES = 128;
    x = 1.0 + x * float(GRADIENT_ENTRIES);

    // Calculate the texel to index into the gradient color entries:
    //     floor(x) is the gradient color entry index
    //     fract(x) is the linear filtering factor between start and end
    int lut_offset = 2 * int(floor(x));     // There is a [start, end] color per entry.

    // Ensure we don't fetch outside the valid range of the LUT.
    lut_offset = clamp(lut_offset, 0, 2 * (GRADIENT_ENTRIES + 1));

    // Fetch the start and end color.
    vec4 texels[2] = fetch_from_resource_cache_2(address + lut_offset);

    // Finally interpolate and apply dithering
    return dither(mix(texels[0], texels[1], fract(x)));
}

#endif //WR_FRAGMENT_SHADER
