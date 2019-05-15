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

#define VECS_PER_TEXT_RUN           2
#define GLYPHS_PER_GPU_BLOCK        2U

struct Glyph {
    vec2 offset;
};

Glyph fetch_glyph(int specific_prim_address,
                  int glyph_index) {
    // Two glyphs are packed in each texel in the GPU cache.
    int glyph_address = specific_prim_address +
                        VECS_PER_TEXT_RUN +
                        int(uint(glyph_index) / GLYPHS_PER_GPU_BLOCK);
    vec4 data = fetch_from_gpu_cache_1(glyph_address);
    // Select XY or ZW based on glyph index.
    // We use "!= 0" instead of "== 1" here in order to work around a driver
    // bug with equality comparisons on integers.
    vec2 glyph = mix(data.xy, data.zw,
                     bvec2(uint(glyph_index) % GLYPHS_PER_GPU_BLOCK != 0U));

    return Glyph(glyph);
}

struct GlyphResource {
    vec4 uv_rect;
    float layer;
    vec2 offset;
    float scale;
};

GlyphResource fetch_glyph_resource(int address) {
    vec4 data[2] = fetch_from_gpu_cache_2(address);
    return GlyphResource(data[0], data[1].x, data[1].yz, data[1].w);
}

struct TextRun {
    vec4 color;
    vec4 bg_color;
};

TextRun fetch_text_run(int address) {
    vec4 data[2] = fetch_from_gpu_cache_2(address);
    return TextRun(data[0], data[1]);
}

VertexInfo write_text_vertex(RectWithSize local_clip_rect,
                             float z,
                             int raster_space,
                             Transform transform,
                             PictureTask task,
                             vec2 text_offset,
                             vec2 glyph_offset,
                             RectWithSize glyph_rect,
                             vec2 snap_bias) {
    // The offset to snap the glyph rect to a device pixel
    vec2 snap_offset = vec2(0.0);
    mat2 local_transform = mat2(1.0);

#ifdef WR_FEATURE_GLYPH_TRANSFORM
    bool remove_subpx_offset = true;
#else
    bool remove_subpx_offset = transform.is_axis_aligned;
#endif
    // Compute the snapping offset only if the scroll node transform is axis-aligned.
    if (remove_subpx_offset) {
        // Be careful to only snap with the transform when in screen raster space.
        switch (raster_space) {
            case RASTER_SCREEN: {
                // Transform from local space to device space.
                float device_scale = task.device_pixel_scale / transform.m[3].w;
                mat2 device_transform = mat2(transform.m) * device_scale;

                // Ensure the transformed text offset does not contain a subpixel translation
                // such that glyph snapping is stable for equivalent glyph subpixel positions.
                vec2 device_text_pos = device_transform * text_offset + transform.m[3].xy * device_scale;
                snap_offset = floor(device_text_pos + 0.5) - device_text_pos;

                // Snap the glyph offset to a device pixel, using an appropriate bias depending
                // on whether subpixel positioning is required.
                vec2 device_glyph_offset = device_transform * glyph_offset;
                snap_offset += floor(device_glyph_offset + snap_bias) - device_glyph_offset;

                // Transform from device space back to local space.
                local_transform = inverse(device_transform);

#ifndef WR_FEATURE_GLYPH_TRANSFORM
                // If not using transformed subpixels, the glyph rect is actually in local space.
                // So convert the snap offset back to local space.
                snap_offset = local_transform * snap_offset;
#endif
                break;
            }
            default: {
                // Otherwise, when in local raster space, the transform may be animated, so avoid
                // snapping with the transform to avoid oscillation.
                snap_offset = floor(text_offset + 0.5) - text_offset;
                snap_offset += floor(glyph_offset + snap_bias) - glyph_offset;
                break;
            }
        }
    }

    // Actually translate the glyph rect to a device pixel using the snap offset.
    glyph_rect.p0 += snap_offset;

#ifdef WR_FEATURE_GLYPH_TRANSFORM
    // The glyph rect is in device space, so transform it back to local space.
    RectWithSize local_rect = transform_rect(glyph_rect, local_transform);

    // Select the corner of the glyph's local space rect that we are processing.
    vec2 local_pos = local_rect.p0 + local_rect.size * aPosition.xy;

    // If the glyph's local rect would fit inside the local clip rect, then select a corner from
    // the device space glyph rect to reduce overdraw of clipped pixels in the fragment shader.
    // Otherwise, fall back to clamping the glyph's local rect to the local clip rect.
    if (rect_inside_rect(local_rect, local_clip_rect)) {
        local_pos = local_transform * (glyph_rect.p0 + glyph_rect.size * aPosition.xy);
    }
#else
    // Select the corner of the glyph rect that we are processing.
    vec2 local_pos = glyph_rect.p0 + glyph_rect.size * aPosition.xy;
#endif

    // Clamp to the local clip rect.
    local_pos = clamp_rect(local_pos, local_clip_rect);

    // Map the clamped local space corner into device space.
    vec4 world_pos = transform.m * vec4(local_pos, 0.0, 1.0);
    vec2 device_pos = world_pos.xy * task.device_pixel_scale;

    // Apply offsets for the render task to get correct screen location.
    vec2 final_offset = -task.content_origin + task.common_data.task_rect.p0;

    gl_Position = uTransform * vec4(device_pos + final_offset * world_pos.w, z * world_pos.w, world_pos.w);

    VertexInfo vi = VertexInfo(
        local_pos,
        snap_offset,
        world_pos
    );

    return vi;
}

void main(void) {
    int prim_header_address = aData.x;
    int glyph_index = aData.y & 0xffff;
    int render_task_index = aData.y >> 16;
    int resource_address = aData.z;
    int raster_space = aData.w >> 16;
    int subpx_dir = (aData.w >> 8) & 0xff;
    int color_mode = aData.w & 0xff;

    PrimitiveHeader ph = fetch_prim_header(prim_header_address);
    Transform transform = fetch_transform(ph.transform_id);
    ClipArea clip_area = fetch_clip_area(ph.user_data.w);
    PictureTask task = fetch_picture_task(render_task_index);

    TextRun text = fetch_text_run(ph.specific_prim_address);
    vec2 text_offset = vec2(ph.user_data.xy) / 256.0;

    if (color_mode == COLOR_MODE_FROM_PASS) {
        color_mode = uMode;
    }

    Glyph glyph = fetch_glyph(ph.specific_prim_address, glyph_index);
    glyph.offset += ph.local_rect.p0 - text_offset;

    GlyphResource res = fetch_glyph_resource(resource_address);

#ifdef WR_FEATURE_GLYPH_TRANSFORM
    // Transform from local space to glyph space.
    mat2 glyph_transform = mat2(transform.m) * task.device_pixel_scale;

    // Compute the glyph rect in glyph space.
    RectWithSize glyph_rect = RectWithSize(res.offset + glyph_transform * (text_offset + glyph.offset),
                                           res.uv_rect.zw - res.uv_rect.xy);
#else
    float raster_scale = float(ph.user_data.z) / 65535.0;

    // Scale from glyph space to local space.
    float scale = res.scale / (raster_scale * task.device_pixel_scale);

    // Compute the glyph rect in local space.
    RectWithSize glyph_rect = RectWithSize(scale * res.offset + text_offset + glyph.offset,
                                           scale * (res.uv_rect.zw - res.uv_rect.xy));
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

    VertexInfo vi = write_text_vertex(ph.local_clip_rect,
                                      ph.z,
                                      raster_space,
                                      transform,
                                      task,
                                      text_offset,
                                      glyph.offset,
                                      glyph_rect,
                                      snap_bias);
    glyph_rect.p0 += vi.snap_offset;

#ifdef WR_FEATURE_GLYPH_TRANSFORM
    vec2 f = (glyph_transform * vi.local_pos - glyph_rect.p0) / glyph_rect.size;
    vUvClip = vec4(f, 1.0 - f);
#else
    vec2 f = (vi.local_pos - glyph_rect.p0) / glyph_rect.size;
#endif

    write_clip(vi.world_pos, vi.snap_offset, clip_area);

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

#if defined(WR_FEATURE_DEBUG_OVERDRAW)
    oFragColor = WR_DEBUG_OVERDRAW_COLOR;
#elif defined(WR_FEATURE_DUAL_SOURCE_BLENDING)
    vec4 alpha_mask = mask * alpha;
    oFragColor = vColor * alpha_mask;
    oFragBlend = alpha_mask * vColor.a;
#else
    write_output(vColor * mask * alpha);
#endif
}
#endif
