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

#define MODE_ALPHA              0
#define MODE_SUBPX_CONST_COLOR  1
#define MODE_SUBPX_PASS0        2
#define MODE_SUBPX_PASS1        3
#define MODE_SUBPX_BG_PASS0     4
#define MODE_SUBPX_BG_PASS1     5
#define MODE_SUBPX_BG_PASS2     6
#define MODE_SUBPX_DUAL_SOURCE  7
#define MODE_BITMAP             8
#define MODE_COLOR_BITMAP       9

VertexInfo write_text_vertex(vec2 clamped_local_pos,
                             RectWithSize local_clip_rect,
                             float z,
                             ClipScrollNode scroll_node,
                             PictureTask task,
                             RectWithSize snap_rect) {
    // Transform the current vertex to world space.
    vec4 world_pos = scroll_node.transform * vec4(clamped_local_pos, 0.0, 1.0);

    // Convert the world positions to device pixel space.
    vec2 device_pos = world_pos.xy / world_pos.w * uDevicePixelRatio;

    // Apply offsets for the render task to get correct screen location.
    vec2 final_pos = device_pos -
                     task.content_origin +
                     task.common_data.task_rect.p0;

#ifdef WR_FEATURE_GLYPH_TRANSFORM
    // For transformed subpixels, we just need to align the glyph origin to a device pixel.
    // Only check the scroll node transform's translation since the scales and axes match.
    vec2 world_snap_p0 = snap_rect.p0 + scroll_node.transform[3].xy * uDevicePixelRatio;
    final_pos += floor(world_snap_p0 + 0.5) - world_snap_p0;
#elif !defined(WR_FEATURE_TRANSFORM)
    // Compute the snapping offset only if the scroll node transform is axis-aligned.
    final_pos += compute_snap_offset(clamped_local_pos, scroll_node, snap_rect);
#endif

    gl_Position = uTransform * vec4(final_pos, z, 1.0);

    VertexInfo vi = VertexInfo(clamped_local_pos, device_pos);
    return vi;
}

void main(void) {
    Primitive prim = load_primitive();
    TextRun text = fetch_text_run(prim.specific_prim_address);

    int glyph_index = prim.user_data0;
    int resource_address = prim.user_data1;
    int subpx_dir = prim.user_data2;

    Glyph glyph = fetch_glyph(prim.specific_prim_address,
                              glyph_index,
                              subpx_dir);
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

    VertexInfo vi = write_text_vertex(local_pos,
                                      prim.local_clip_rect,
                                      prim.z,
                                      prim.scroll_node,
                                      prim.task,
                                      glyph_rect);

#ifdef WR_FEATURE_GLYPH_TRANSFORM
    vec2 f = (transform * vi.local_pos - glyph_rect.p0) / glyph_rect.size;
    vUvClip = vec4(f, 1.0 - f);
#else
    vec2 f = (vi.local_pos - glyph_rect.p0) / glyph_rect.size;
#endif

    write_clip(vi.screen_pos, prim.clip_area);

    switch (uMode) {
        case MODE_ALPHA:
        case MODE_BITMAP:
            vMaskSwizzle = vec2(0.0, 1.0);
            vColor = text.color;
            break;
        case MODE_SUBPX_PASS1:
        case MODE_SUBPX_BG_PASS2:
        case MODE_SUBPX_DUAL_SOURCE:
            vMaskSwizzle = vec2(1.0, 0.0);
            vColor = text.color;
            break;
        case MODE_SUBPX_CONST_COLOR:
        case MODE_SUBPX_PASS0:
        case MODE_SUBPX_BG_PASS0:
        case MODE_COLOR_BITMAP:
            vMaskSwizzle = vec2(1.0, 0.0);
            vColor = vec4(text.color.a);
            break;
        case MODE_SUBPX_BG_PASS1:
            vMaskSwizzle = vec2(-1.0, 1.0);
            vColor = vec4(text.color.a) * text.bg_color;
            break;
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
