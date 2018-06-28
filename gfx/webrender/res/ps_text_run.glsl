/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include shared,prim_shared

flat varying vec4 vColor;
varying vec3 vUv;
flat varying vec4 vUvBorder;
flat varying vec2 vMaskSwizzle;

#ifdef WR_FEATURE_GLYPH_TRANSFORM
varying vec4 vUvClip;
#endif

#ifdef WR_VERTEX_SHADER

struct Glyph {
    vec2 offset;
};

Glyph fetch_glyph(int specific_prim_address,
                  int glyph_index) {
    // Two glyphs are packed in each texel in the GPU cache.
    int glyph_address = specific_prim_address +
                        VECS_PER_TEXT_RUN +
                        glyph_index / 2;
    vec4 data = fetch_from_resource_cache_1(glyph_address);
    // Select XY or ZW based on glyph index.
    // We use "!= 0" instead of "== 1" here in order to work around a driver
    // bug with equality comparisons on integers.
    vec2 glyph = mix(data.xy, data.zw, bvec2(glyph_index % 2 != 0));

    return Glyph(glyph);
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
    pi.render_task_index = aData0.y % 0x10000;
    pi.clip_task_index = aData0.y / 0x10000;
    pi.clip_chain_rect_index = aData0.z;
    pi.scroll_node_id = aData0.w;
    pi.z = aData1.x;
    pi.user_data0 = aData1.y;
    pi.user_data1 = aData1.z;
    pi.user_data2 = aData1.w;

    return pi;
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

VertexInfo write_text_vertex(vec2 clamped_local_pos,
                             RectWithSize local_clip_rect,
                             float z,
                             ClipScrollNode scroll_node,
                             PictureTask task,
                             vec2 text_offset,
                             RectWithSize snap_rect,
                             vec2 snap_bias) {
    // Transform the current vertex to world space.
    vec4 world_pos = scroll_node.transform * vec4(clamped_local_pos, 0.0, 1.0);

    // Convert the world positions to device pixel space.
    float device_scale = uDevicePixelRatio / world_pos.w;
    vec2 device_pos = world_pos.xy * device_scale;

    // Apply offsets for the render task to get correct screen location.
    vec2 final_pos = device_pos -
                     task.content_origin +
                     task.common_data.task_rect.p0;

#if defined(WR_FEATURE_GLYPH_TRANSFORM)
    bool remove_subpx_offset = true;
#else
    // Compute the snapping offset only if the scroll node transform is axis-aligned.
    bool remove_subpx_offset = scroll_node.is_axis_aligned;
#endif
    if (remove_subpx_offset) {
        // Ensure the transformed text offset does not contain a subpixel translation
        // such that glyph snapping is stable for equivalent glyph subpixel positions.
        vec2 world_text_offset = mat2(scroll_node.transform) * text_offset;
        vec2 device_text_pos = (scroll_node.transform[3].xy + world_text_offset) * device_scale;
        final_pos += floor(device_text_pos + 0.5) - device_text_pos;

#ifdef WR_FEATURE_GLYPH_TRANSFORM
        // For transformed subpixels, we just need to align the glyph origin to a device pixel.
        // The transformed text offset has already been snapped, so remove it from the glyph
        // origin when snapping the glyph.
        vec2 snap_offset = snap_rect.p0 - world_text_offset * device_scale;
        final_pos += floor(snap_offset + snap_bias) - snap_offset;
#else
        // The transformed text offset has already been snapped, so remove it from the transform
        // when snapping the glyph.
        mat4 snap_transform = scroll_node.transform;
        snap_transform[3].xy = -world_text_offset;
        final_pos += compute_snap_offset(
            clamped_local_pos,
            snap_transform,
            snap_rect,
            snap_bias
        );
#endif
    }

    gl_Position = uTransform * vec4(final_pos, z, 1.0);

    VertexInfo vi = VertexInfo(
        clamped_local_pos,
        device_pos,
        world_pos.w,
        final_pos
    );

    return vi;
}

void main(void) {
    Primitive prim = load_primitive();
    TextRun text = fetch_text_run(prim.specific_prim_address);

    int glyph_index = prim.user_data0;
    int resource_address = prim.user_data1;
    int subpx_dir = prim.user_data2 >> 16;
    int color_mode = prim.user_data2 & 0xffff;

    if (color_mode == COLOR_MODE_FROM_PASS) {
        color_mode = uMode;
    }

    Glyph glyph = fetch_glyph(prim.specific_prim_address, glyph_index);
    GlyphResource res = fetch_glyph_resource(resource_address);

#ifdef WR_FEATURE_GLYPH_TRANSFORM
    // Transform from local space to glyph space.
    mat2 transform = mat2(prim.scroll_node.transform) * uDevicePixelRatio;

    // Compute the glyph rect in glyph space.
    RectWithSize glyph_rect = RectWithSize(res.offset + transform * (text.offset + glyph.offset),
                                           res.uv_rect.zw - res.uv_rect.xy);

    // Transform the glyph rect back to local space.
    mat2 inv = inverse(transform);
    RectWithSize local_rect = transform_rect(glyph_rect, inv);

    // Select the corner of the glyph's local space rect that we are processing.
    vec2 local_pos = local_rect.p0 + local_rect.size * aPosition.xy;

    // If the glyph's local rect would fit inside the local clip rect, then select a corner from
    // the device space glyph rect to reduce overdraw of clipped pixels in the fragment shader.
    // Otherwise, fall back to clamping the glyph's local rect to the local clip rect.
    local_pos = rect_inside_rect(local_rect, prim.local_clip_rect) ?
                    inv * (glyph_rect.p0 + glyph_rect.size * aPosition.xy) :
                    clamp_rect(local_pos, prim.local_clip_rect);
#else
    // Scale from glyph space to local space.
    float scale = res.scale / uDevicePixelRatio;

    // Compute the glyph rect in local space.
    RectWithSize glyph_rect = RectWithSize(scale * res.offset + text.offset + glyph.offset,
                                           scale * (res.uv_rect.zw - res.uv_rect.xy));

    // Select the corner of the glyph rect that we are processing.
    vec2 local_pos = glyph_rect.p0 + glyph_rect.size * aPosition.xy;

    // Clamp to the local clip rect.
    local_pos = clamp_rect(local_pos, prim.local_clip_rect);
#endif

    vec2 snap_bias;
    // In subpixel mode, the subpixel offset has already been
    // accounted for while rasterizing the glyph. However, we
    // must still round with a subpixel bias rather than rounding
    // to the nearest whole pixel, depending on subpixel direciton.
    switch (subpx_dir) {
        case SUBPX_DIR_NONE:
        default:
            snap_bias = vec2(0.5);
            break;
        case SUBPX_DIR_HORIZONTAL:
            // Glyphs positioned [-0.125, 0.125] get a
            // subpx position of zero. So include that
            // offset in the glyph position to ensure
            // we round to the correct whole position.
            snap_bias = vec2(0.125, 0.5);
            break;
        case SUBPX_DIR_VERTICAL:
            snap_bias = vec2(0.5, 0.125);
            break;
        case SUBPX_DIR_MIXED:
            snap_bias = vec2(0.125);
            break;
    }

    VertexInfo vi = write_text_vertex(local_pos,
                                      prim.local_clip_rect,
                                      prim.z,
                                      prim.scroll_node,
                                      prim.task,
                                      text.offset,
                                      glyph_rect,
                                      snap_bias);

#ifdef WR_FEATURE_GLYPH_TRANSFORM
    vec2 f = (transform * vi.local_pos - glyph_rect.p0) / glyph_rect.size;
    vUvClip = vec4(f, 1.0 - f);
#else
    vec2 f = (vi.local_pos - glyph_rect.p0) / glyph_rect.size;
#endif

    write_clip(vi.screen_pos, prim.clip_area);

    switch (color_mode) {
        case COLOR_MODE_ALPHA:
        case COLOR_MODE_BITMAP:
            vMaskSwizzle = vec2(0.0, 1.0);
            vColor = text.color;
            break;
        case COLOR_MODE_SUBPX_BG_PASS2:
        case COLOR_MODE_SUBPX_DUAL_SOURCE:
            vMaskSwizzle = vec2(1.0, 0.0);
            vColor = text.color;
            break;
        case COLOR_MODE_SUBPX_CONST_COLOR:
        case COLOR_MODE_SUBPX_BG_PASS0:
        case COLOR_MODE_COLOR_BITMAP:
            vMaskSwizzle = vec2(1.0, 0.0);
            vColor = vec4(text.color.a);
            break;
        case COLOR_MODE_SUBPX_BG_PASS1:
            vMaskSwizzle = vec2(-1.0, 1.0);
            vColor = vec4(text.color.a) * text.bg_color;
            break;
        default:
            vMaskSwizzle = vec2(0.0);
            vColor = vec4(1.0);
    }

    vec2 texture_size = vec2(textureSize(sColor0, 0));
    vec2 st0 = res.uv_rect.xy / texture_size;
    vec2 st1 = res.uv_rect.zw / texture_size;

    vUv = vec3(mix(st0, st1, f), res.layer);
    vUvBorder = (res.uv_rect + vec4(0.5, 0.5, -0.5, -0.5)) / texture_size.xyxy;
}
#endif

#ifdef WR_FRAGMENT_SHADER
void main(void) {
    vec3 tc = vec3(clamp(vUv.xy, vUvBorder.xy, vUvBorder.zw), vUv.z);
    vec4 mask = texture(sColor0, tc);
    mask.rgb = mask.rgb * vMaskSwizzle.x + mask.aaa * vMaskSwizzle.y;

    float alpha = do_clip();
#ifdef WR_FEATURE_GLYPH_TRANSFORM
    alpha *= float(all(greaterThanEqual(vUvClip, vec4(0.0))));
#endif

#ifdef WR_FEATURE_DUAL_SOURCE_BLENDING
    vec4 alpha_mask = mask * alpha;
    oFragColor = vColor * alpha_mask;
    oFragBlend = alpha_mask * vColor.a;
#else
    oFragColor = vColor * mask * alpha;
#endif
}
#endif
