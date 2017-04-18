#line 1
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

struct ImageMaskData {
    RectWithSize uv_rect;
    RectWithSize local_rect;
};

ImageMaskData fetch_mask_data(int index) {
    vec4 data[2] = fetch_data_2(index);
    return ImageMaskData(RectWithSize(data[0].xy, data[0].zw),
                         RectWithSize(data[1].xy, data[1].zw));
}

void main(void) {
    CacheClipInstance cci = fetch_clip_item(gl_InstanceID);
    ClipArea area = fetch_clip_area(cci.render_task_index);
    Layer layer = fetch_layer(cci.layer_index);
    ImageMaskData mask = fetch_mask_data(cci.data_index);
    RectWithSize local_rect = mask.local_rect;

    ClipVertexInfo vi = write_clip_tile_vertex(local_rect,
                                               layer,
                                               area,
                                               cci.segment_index);

    vLocalRect = vi.clipped_local_rect;
    vPos = vi.local_pos;

    vClipMaskUv = vec3((vPos.xy / vPos.z - local_rect.p0) / local_rect.size, 0.0);
    vec2 texture_size = vec2(textureSize(sMask, 0));
    vClipMaskUvRect = vec4(mask.uv_rect.p0, mask.uv_rect.size) / texture_size.xyxy;
    // applying a half-texel offset to the UV boundaries to prevent linear samples from the outside
    vec4 inner_rect = vec4(mask.uv_rect.p0, mask.uv_rect.p0 + mask.uv_rect.size);
    vClipMaskUvInnerRect = (inner_rect + vec4(0.5, 0.5, -0.5, -0.5)) / texture_size.xyxy;
}
