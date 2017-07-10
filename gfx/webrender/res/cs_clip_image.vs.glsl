#line 1
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

struct ImageMaskData {
    RectWithSize local_rect;
};

ImageMaskData fetch_mask_data(int index) {
    vec4 data = fetch_from_resource_cache_1(index);
    return ImageMaskData(RectWithSize(data.xy, data.zw));
}

void main(void) {
    CacheClipInstance cci = fetch_clip_item(gl_InstanceID);
    ClipArea area = fetch_clip_area(cci.render_task_index);
    Layer layer = fetch_layer(cci.layer_index);
    ImageMaskData mask = fetch_mask_data(cci.data_index);
    RectWithSize local_rect = mask.local_rect;
    ImageResource res = fetch_image_resource(cci.resource_address);

    ClipVertexInfo vi = write_clip_tile_vertex(local_rect,
                                               layer,
                                               area,
                                               cci.segment_index);

    vPos = vi.local_pos;

    vClipMaskUv = vec3((vPos.xy / vPos.z - local_rect.p0) / local_rect.size, 0.0);
    vec2 texture_size = vec2(textureSize(sColor0, 0));
    vClipMaskUvRect = vec4(res.uv_rect.xy, res.uv_rect.zw - res.uv_rect.xy) / texture_size.xyxy;
    // applying a half-texel offset to the UV boundaries to prevent linear samples from the outside
    vec4 inner_rect = vec4(res.uv_rect.xy, res.uv_rect.zw);
    vClipMaskUvInnerRect = (inner_rect + vec4(0.5, 0.5, -0.5, -0.5)) / texture_size.xyxy;
}
