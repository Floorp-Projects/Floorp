/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include shared,clip_shared

varying vec3 vPos;
varying vec3 vClipMaskImageUv;

flat varying vec4 vClipMaskUvRect;
flat varying vec4 vClipMaskUvInnerRect;
flat varying float vLayer;

#ifdef WR_VERTEX_SHADER
struct ImageMaskData {
    RectWithSize local_rect;
};

ImageMaskData fetch_mask_data(ivec2 address) {
    vec4 data = fetch_from_resource_cache_1_direct(address);
    RectWithSize local_rect = RectWithSize(data.xy, data.zw);
    ImageMaskData mask_data = ImageMaskData(local_rect);
    return mask_data;
}

void main(void) {
    ClipMaskInstance cmi = fetch_clip_item();
    ClipArea area = fetch_clip_area(cmi.render_task_address);
    ClipScrollNode scroll_node = fetch_clip_scroll_node(cmi.scroll_node_id);
    ImageMaskData mask = fetch_mask_data(cmi.clip_data_address);
    RectWithSize local_rect = mask.local_rect;
    ImageResource res = fetch_image_resource_direct(cmi.resource_address);

    ClipVertexInfo vi = write_clip_tile_vertex(local_rect,
                                               scroll_node,
                                               area);

    vPos = vi.local_pos;
    vLayer = res.layer;

    vClipMaskImageUv = vec3((vPos.xy / vPos.z - local_rect.p0) / local_rect.size, 0.0);
    vec2 texture_size = vec2(textureSize(sColor0, 0));
    vClipMaskUvRect = vec4(res.uv_rect.p0, res.uv_rect.p1 - res.uv_rect.p0) / texture_size.xyxy;
    // applying a half-texel offset to the UV boundaries to prevent linear samples from the outside
    vec4 inner_rect = vec4(res.uv_rect.p0, res.uv_rect.p1);
    vClipMaskUvInnerRect = (inner_rect + vec4(0.5, 0.5, -0.5, -0.5)) / texture_size.xyxy;
}
#endif

#ifdef WR_FRAGMENT_SHADER
void main(void) {
    float alpha = init_transform_fs(vPos.xy / vPos.z);

    bool repeat_mask = false; //TODO
    vec2 clamped_mask_uv = repeat_mask ? fract(vClipMaskImageUv.xy) :
        clamp(vClipMaskImageUv.xy, vec2(0.0, 0.0), vec2(1.0, 1.0));
    vec2 source_uv = clamp(clamped_mask_uv * vClipMaskUvRect.zw + vClipMaskUvRect.xy,
        vClipMaskUvInnerRect.xy, vClipMaskUvInnerRect.zw);
    float clip_alpha = texture(sColor0, vec3(source_uv, vLayer)).r; //careful: texture has type A8

    oFragColor = vec4(alpha * clip_alpha, 1.0, 1.0, 1.0);
}
#endif
