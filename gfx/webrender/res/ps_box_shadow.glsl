/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include shared,prim_shared

flat varying vec4 vColor;

varying vec3 vUv;
flat varying vec2 vMirrorPoint;
flat varying vec4 vCacheUvRectCoords;

#ifdef WR_VERTEX_SHADER
#define BS_HEADER_VECS 4

RectWithSize fetch_instance_geometry(int address) {
    vec4 data = fetch_from_resource_cache_1(address);
    return RectWithSize(data.xy, data.zw);
}

void main(void) {
    Primitive prim = load_primitive();
    BoxShadow bs = fetch_boxshadow(prim.specific_prim_address);
    RectWithSize segment_rect = fetch_instance_geometry(prim.specific_prim_address + BS_HEADER_VECS + prim.user_data0);

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

    vec2 texture_size = vec2(textureSize(sSharedCacheA8, 0));
    vCacheUvRectCoords = vec4(patch_origin, patch_origin + patch_size_device_pixels) / texture_size.xyxy;

    vColor = bs.color;

    write_clip(vi.screen_pos, prim.clip_area);
}
#endif

#ifdef WR_FRAGMENT_SHADER
void main(void) {
    vec4 clip_scale = vec4(1.0, 1.0, 1.0, do_clip());

    // Mirror and stretch the box shadow corner over the entire
    // primitives.
    vec2 uv = vMirrorPoint - abs(vUv.xy - vMirrorPoint);

    // Ensure that we don't fetch texels outside the box
    // shadow corner. This can happen, for example, when
    // drawing the outer parts of an inset box shadow.
    uv = clamp(uv, vec2(0.0), vec2(1.0));

    // Map the unit UV to the actual UV rect in the cache.
    uv = mix(vCacheUvRectCoords.xy, vCacheUvRectCoords.zw, uv);

    // Modulate the box shadow by the color.
    float mask = texture(sSharedCacheA8, vec3(uv, vUv.z)).r;
    oFragColor = clip_scale * dither(vColor * vec4(1.0, 1.0, 1.0, mask));
}
#endif
