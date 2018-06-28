/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include shared,clip_shared

varying vec3 vPos;
varying vec2 vUv;
flat varying vec4 vUvBounds;
flat varying float vLayer;
flat varying vec4 vEdge;
flat varying vec4 vUvBounds_NoClamp;
flat varying float vClipMode;

#define MODE_STRETCH        0
#define MODE_SIMPLE         1

#ifdef WR_VERTEX_SHADER

struct BoxShadowData {
    vec2 src_rect_size;
    float clip_mode;
    int stretch_mode_x;
    int stretch_mode_y;
    RectWithSize dest_rect;
};

BoxShadowData fetch_data(ivec2 address) {
    vec4 data[3] = fetch_from_resource_cache_3_direct(address);
    RectWithSize dest_rect = RectWithSize(data[2].xy, data[2].zw);
    BoxShadowData bs_data = BoxShadowData(
        data[0].xy,
        data[0].z,
        int(data[1].x),
        int(data[1].y),
        dest_rect
    );
    return bs_data;
}

void main(void) {
    ClipMaskInstance cmi = fetch_clip_item();
    ClipArea area = fetch_clip_area(cmi.render_task_address);
    Transform transform = fetch_transform(cmi.transform_id);
    BoxShadowData bs_data = fetch_data(cmi.clip_data_address);
    ImageResource res = fetch_image_resource_direct(cmi.resource_address);

    ClipVertexInfo vi = write_clip_tile_vertex(bs_data.dest_rect,
                                               transform,
                                               area);

    vLayer = res.layer;
    vPos = vi.local_pos;
    vClipMode = bs_data.clip_mode;

    vec2 uv0 = res.uv_rect.p0;
    vec2 uv1 = res.uv_rect.p1;

    vec2 texture_size = vec2(textureSize(sColor0, 0));
    vec2 local_pos = vPos.xy / vPos.z;

    switch (bs_data.stretch_mode_x) {
        case MODE_STRETCH: {
            vEdge.x = 0.5;
            vEdge.z = (bs_data.dest_rect.size.x / bs_data.src_rect_size.x) - 0.5;
            vUv.x = (local_pos.x - bs_data.dest_rect.p0.x) / bs_data.src_rect_size.x;
            break;
        }
        case MODE_SIMPLE:
        default: {
            vEdge.xz = vec2(1.0);
            vUv.x = (local_pos.x - bs_data.dest_rect.p0.x) / bs_data.dest_rect.size.x;
            break;
        }
    }

    switch (bs_data.stretch_mode_y) {
        case MODE_STRETCH: {
            vEdge.y = 0.5;
            vEdge.w = (bs_data.dest_rect.size.y / bs_data.src_rect_size.y) - 0.5;
            vUv.y = (local_pos.y - bs_data.dest_rect.p0.y) / bs_data.src_rect_size.y;
            break;
        }
        case MODE_SIMPLE:
        default: {
            vEdge.yw = vec2(1.0);
            vUv.y = (local_pos.y - bs_data.dest_rect.p0.y) / bs_data.dest_rect.size.y;
            break;
        }
    }

    vUvBounds = vec4(uv0 + vec2(0.5), uv1 - vec2(0.5)) / texture_size.xyxy;
    vUvBounds_NoClamp = vec4(uv0, uv1) / texture_size.xyxy;
}
#endif

#ifdef WR_FRAGMENT_SHADER
void main(void) {
    vec2 local_pos = vPos.xy / vPos.z;

    vec2 uv = clamp(vUv.xy, vec2(0.0), vEdge.xy);
    uv += max(vec2(0.0), vUv.xy - vEdge.zw);
    uv = mix(vUvBounds_NoClamp.xy, vUvBounds_NoClamp.zw, uv);
    uv = clamp(uv, vUvBounds.xy, vUvBounds.zw);

    float in_shadow_rect = init_transform_rough_fs(local_pos);

    float texel = TEX_SAMPLE(sColor0, vec3(uv, vLayer)).r;

    float alpha = mix(texel, 1.0 - texel, vClipMode);

    oFragColor = vec4(mix(vClipMode, alpha, in_shadow_rect));
}
#endif
