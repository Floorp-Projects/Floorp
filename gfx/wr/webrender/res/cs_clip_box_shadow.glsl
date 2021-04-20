/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include shared,clip_shared

varying vec4 vLocalPos;
varying vec2 vUv;
flat varying vec4 vUvBounds;
flat varying vec4 vEdge;
flat varying vec4 vUvBounds_NoClamp;
#if defined(PLATFORM_ANDROID) && !defined(SWGL)
// Work around Adreno 3xx driver bug. See the v_perspective comment in
// brush_image or bug 1630356 for details.
flat varying vec2 vClipModeVec;
#define vClipMode vClipModeVec.x
#else
flat varying float vClipMode;
#endif

#define MODE_STRETCH        0
#define MODE_SIMPLE         1

#ifdef WR_VERTEX_SHADER

PER_INSTANCE in ivec2 aClipDataResourceAddress;
PER_INSTANCE in vec2 aClipSrcRectSize;
PER_INSTANCE in int aClipMode;
PER_INSTANCE in ivec2 aStretchMode;
PER_INSTANCE in vec4 aClipDestRect;

struct ClipMaskInstanceBoxShadow {
    ClipMaskInstanceCommon base;
    ivec2 resource_address;
};

ClipMaskInstanceBoxShadow fetch_clip_item() {
    ClipMaskInstanceBoxShadow cmi;

    cmi.base = fetch_clip_item_common();
    cmi.resource_address = aClipDataResourceAddress;

    return cmi;
}

struct BoxShadowData {
    vec2 src_rect_size;
    int clip_mode;
    int stretch_mode_x;
    int stretch_mode_y;
    RectWithSize dest_rect;
};

BoxShadowData fetch_data() {
    BoxShadowData bs_data = BoxShadowData(
        aClipSrcRectSize,
        aClipMode,
        aStretchMode.x,
        aStretchMode.y,
        RectWithSize(aClipDestRect.xy, aClipDestRect.zw)
    );
    return bs_data;
}

void main(void) {
    ClipMaskInstanceBoxShadow cmi = fetch_clip_item();
    Transform clip_transform = fetch_transform(cmi.base.clip_transform_id);
    Transform prim_transform = fetch_transform(cmi.base.prim_transform_id);
    BoxShadowData bs_data = fetch_data();
    ImageSource res = fetch_image_source_direct(cmi.resource_address);

    RectWithSize dest_rect = bs_data.dest_rect;

    ClipVertexInfo vi = write_clip_tile_vertex(
        dest_rect,
        prim_transform,
        clip_transform,
        cmi.base.sub_rect,
        cmi.base.task_origin,
        cmi.base.screen_origin,
        cmi.base.device_pixel_scale
    );
    vClipMode = float(bs_data.clip_mode);

    vec2 texture_size = vec2(TEX_SIZE(sColor0));
    vec2 local_pos = vi.local_pos.xy / vi.local_pos.w;
    vLocalPos = vi.local_pos;

    switch (bs_data.stretch_mode_x) {
        case MODE_STRETCH: {
            vEdge.x = 0.5;
            vEdge.z = (dest_rect.size.x / bs_data.src_rect_size.x) - 0.5;
            vUv.x = (local_pos.x - dest_rect.p0.x) / bs_data.src_rect_size.x;
            break;
        }
        case MODE_SIMPLE:
        default: {
            vEdge.xz = vec2(1.0);
            vUv.x = (local_pos.x - dest_rect.p0.x) / dest_rect.size.x;
            break;
        }
    }

    switch (bs_data.stretch_mode_y) {
        case MODE_STRETCH: {
            vEdge.y = 0.5;
            vEdge.w = (dest_rect.size.y / bs_data.src_rect_size.y) - 0.5;
            vUv.y = (local_pos.y - dest_rect.p0.y) / bs_data.src_rect_size.y;
            break;
        }
        case MODE_SIMPLE:
        default: {
            vEdge.yw = vec2(1.0);
            vUv.y = (local_pos.y - dest_rect.p0.y) / dest_rect.size.y;
            break;
        }
    }

    vUv *= vi.local_pos.w;
    vec2 uv0 = res.uv_rect.p0;
    vec2 uv1 = res.uv_rect.p1;
    vUvBounds = vec4(uv0 + vec2(0.5), uv1 - vec2(0.5)) / texture_size.xyxy;
    vUvBounds_NoClamp = vec4(uv0, uv1) / texture_size.xyxy;
}
#endif

#ifdef WR_FRAGMENT_SHADER
void main(void) {
    vec2 uv_linear = vUv / vLocalPos.w;
    vec2 uv = clamp(uv_linear, vec2(0.0), vEdge.xy);
    uv += max(vec2(0.0), uv_linear - vEdge.zw);
    uv = mix(vUvBounds_NoClamp.xy, vUvBounds_NoClamp.zw, uv);
    uv = clamp(uv, vUvBounds.xy, vUvBounds.zw);

    float in_shadow_rect = init_transform_rough_fs(vLocalPos.xy / vLocalPos.w);

    float texel = TEX_SAMPLE(sColor0, uv).r;

    float alpha = mix(texel, 1.0 - texel, vClipMode);
    float result = vLocalPos.w > 0.0 ? mix(vClipMode, alpha, in_shadow_rect) : 0.0;

    oFragColor = vec4(result);
}
#endif
