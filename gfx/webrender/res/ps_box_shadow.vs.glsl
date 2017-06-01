#line 1
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

void main(void) {
    Primitive prim = load_primitive();
    BoxShadow bs = fetch_boxshadow(prim.prim_index);
    RectWithSize segment_rect = fetch_instance_geometry(prim.user_data0);

    VertexInfo vi = write_vertex(segment_rect,
                                 prim.local_clip_rect,
                                 prim.z,
                                 prim.layer,
                                 prim.task,
                                 prim.local_rect);

    RenderTaskData child_task = fetch_render_task(prim.user_data1);
    vUv.z = child_task.data1.x;

    // Constant offsets to inset from bilinear filtering border.
    vec2 patch_origin = child_task.data0.xy + vec2(1.0);
    vec2 patch_size_device_pixels = child_task.data0.zw - vec2(2.0);
    vec2 patch_size = patch_size_device_pixels / uDevicePixelRatio;

    vUv.xy = (vi.local_pos - prim.local_rect.p0) / patch_size;
    vMirrorPoint = 0.5 * prim.local_rect.size / patch_size;

    vec2 texture_size = vec2(textureSize(sCacheRGBA8, 0));
    vCacheUvRectCoords = vec4(patch_origin, patch_origin + patch_size_device_pixels) / texture_size.xyxy;

    vColor = bs.color;

    write_clip(vi.screen_pos, prim.clip_area);
}
