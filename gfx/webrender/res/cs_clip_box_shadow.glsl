/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include shared,prim_shared,clip_shared

varying vec3 vPos;
varying vec2 vUv;
flat varying vec4 vUvBounds;
flat varying float vLayer;
flat varying vec4 vEdge;
flat varying vec4 vUvBounds_NoClamp;
flat varying float vClipMode;
flat varying int vStretchMode;

#define MODE_STRETCH        0
#define MODE_SIMPLE         1

#ifdef WR_VERTEX_SHADER

struct BoxShadowData {
    vec2 src_rect_size;
    float clip_mode;
    int stretch_mode;
    RectWithSize dest_rect;
};

BoxShadowData fetch_data(ivec2 address) {
    vec4 data[2] = fetch_from_resource_cache_2_direct(address);
    RectWithSize dest_rect = RectWithSize(data[1].xy, data[1].zw);
    BoxShadowData bs_data = BoxShadowData(data[0].xy, data[0].z, int(data[0].w), dest_rect);
    return bs_data;
}

void main(void) {
    ClipMaskInstance cmi = fetch_clip_item();
    ClipArea area = fetch_clip_area(cmi.render_task_address);
    ClipScrollNode scroll_node = fetch_clip_scroll_node(cmi.scroll_node_id);
    BoxShadowData bs_data = fetch_data(cmi.clip_data_address);
    ImageResource res = fetch_image_resource_direct(cmi.resource_address);

    ClipVertexInfo vi = write_clip_tile_vertex(bs_data.dest_rect,
                                               scroll_node,
                                               area);

    vLayer = res.layer;
    vPos = vi.local_pos;
    vClipMode = bs_data.clip_mode;
    vStretchMode = bs_data.stretch_mode;

    vec2 uv0 = res.uv_rect.p0;
    vec2 uv1 = res.uv_rect.p1;

    vec2 texture_size = vec2(textureSize(sColor0, 0));
    vec2 local_pos = vPos.xy / vPos.z;

    switch (bs_data.stretch_mode) {
        case MODE_STRETCH: {
            vEdge.xy = vec2(0.5);
            vEdge.zw = (bs_data.dest_rect.size / bs_data.src_rect_size) - vec2(0.5);
            vUv = (local_pos - bs_data.dest_rect.p0) / bs_data.src_rect_size;
            break;
        }
        case MODE_SIMPLE:
        default: {
            vec2 f = (local_pos - bs_data.dest_rect.p0) / bs_data.dest_rect.size;
            vUv = mix(uv0, uv1, f) / texture_size;
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
    vec2 uv;

    switch (vStretchMode) {
        case MODE_STRETCH: {
            uv = clamp(vUv.xy, vec2(0.0), vEdge.xy);
            uv += max(vec2(0.0), vUv.xy - vEdge.zw);
            uv = mix(vUvBounds_NoClamp.xy, vUvBounds_NoClamp.zw, uv);
            break;
        }
        case MODE_SIMPLE:
        default: {
            uv = vUv.xy;
            break;
        }
    }

    uv = clamp(uv, vUvBounds.xy, vUvBounds.zw);

    float in_shadow_rect = point_inside_rect(
        local_pos,
        vLocalBounds.xy,
        vLocalBounds.zw
    );

    float texel = TEX_SAMPLE(sColor0, vec3(uv, vLayer)).r;

    float alpha = mix(texel, 1.0 - texel, vClipMode);

    oFragColor = vec4(mix(vClipMode, alpha, in_shadow_rect));
}
#endif
