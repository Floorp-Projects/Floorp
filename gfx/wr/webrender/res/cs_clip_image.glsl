/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include shared,clip_shared

varying vec2 vLocalPos;
varying vec2 vClipMaskImageUv;

flat varying vec4 vClipMaskUvRect;
flat varying vec4 vClipMaskUvInnerRect;
flat varying float vLayer;

#ifdef WR_VERTEX_SHADER
struct ImageMaskData {
    RectWithSize local_mask_rect;
    RectWithSize local_tile_rect;
};

ImageMaskData fetch_mask_data(ivec2 address) {
    vec4 data[2] = fetch_from_gpu_cache_2_direct(address);
    RectWithSize mask_rect = RectWithSize(data[0].xy, data[0].zw);
    RectWithSize tile_rect = RectWithSize(data[1].xy, data[1].zw);
    ImageMaskData mask_data = ImageMaskData(mask_rect, tile_rect);
    return mask_data;
}

void main(void) {
    ClipMaskInstance cmi = fetch_clip_item();
    ClipArea area = fetch_clip_area(cmi.render_task_address);
    Transform clip_transform = fetch_transform(cmi.clip_transform_id);
    Transform prim_transform = fetch_transform(cmi.prim_transform_id);
    ImageMaskData mask = fetch_mask_data(cmi.clip_data_address);
    RectWithSize local_rect = mask.local_mask_rect;
    ImageResource res = fetch_image_resource_direct(cmi.resource_address);

    ClipVertexInfo vi = write_clip_tile_vertex(
        local_rect,
        prim_transform,
        clip_transform,
        area
    );
    vLocalPos = vi.local_pos.xy / vi.local_pos.z;
    vLayer = res.layer;
    vClipMaskImageUv = (vLocalPos - mask.local_tile_rect.p0) / mask.local_tile_rect.size;
    vec2 texture_size = vec2(textureSize(sColor0, 0));
    vClipMaskUvRect = vec4(res.uv_rect.p0, res.uv_rect.p1 - res.uv_rect.p0) / texture_size.xyxy;
    // applying a half-texel offset to the UV boundaries to prevent linear samples from the outside
    vec4 inner_rect = vec4(res.uv_rect.p0, res.uv_rect.p1);
    vClipMaskUvInnerRect = (inner_rect + vec4(0.5, 0.5, -0.5, -0.5)) / texture_size.xyxy;
}
#endif

#ifdef WR_FRAGMENT_SHADER
void main(void) {
    float alpha = init_transform_fs(vLocalPos);

    // TODO: Handle repeating masks?
    vec2 clamped_mask_uv = clamp(vClipMaskImageUv, vec2(0.0, 0.0), vec2(1.0, 1.0));

    // Ensure we don't draw outside of our tile.
    // FIXME(emilio): Can we do this earlier?
    if (clamped_mask_uv != vClipMaskImageUv)
        discard;

    vec2 source_uv = clamp(clamped_mask_uv * vClipMaskUvRect.zw + vClipMaskUvRect.xy,
        vClipMaskUvInnerRect.xy, vClipMaskUvInnerRect.zw);
    float clip_alpha = texture(sColor0, vec3(source_uv, vLayer)).r; //careful: texture has type A8
    oFragColor = vec4(alpha * clip_alpha, 1.0, 1.0, 1.0);
}
#endif
