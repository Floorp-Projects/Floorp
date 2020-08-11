/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include shared,clip_shared,ellipse

varying vec4 vLocalPos;
#ifdef WR_FEATURE_FAST_PATH
flat varying vec3 vClipParams;      // xy = box size, z = radius
#else
flat varying vec4 vClipCenter_Radius_TL;
flat varying vec4 vClipCenter_Radius_TR;
flat varying vec4 vClipCenter_Radius_BL;
flat varying vec4 vClipCenter_Radius_BR;
#endif

flat varying float vClipMode;

#ifdef WR_VERTEX_SHADER

PER_INSTANCE in vec2 aClipLocalPos;
PER_INSTANCE in vec4 aClipLocalRect;
PER_INSTANCE in float aClipMode;
PER_INSTANCE in vec4 aClipRect_TL;
PER_INSTANCE in vec4 aClipRadii_TL;
PER_INSTANCE in vec4 aClipRect_TR;
PER_INSTANCE in vec4 aClipRadii_TR;
PER_INSTANCE in vec4 aClipRect_BL;
PER_INSTANCE in vec4 aClipRadii_BL;
PER_INSTANCE in vec4 aClipRect_BR;
PER_INSTANCE in vec4 aClipRadii_BR;

struct ClipMaskInstanceRect {
    ClipMaskInstanceCommon shared;
    vec2 local_pos;
};

ClipMaskInstanceRect fetch_clip_item() {
    ClipMaskInstanceRect cmi;

    cmi.shared = fetch_clip_item_common();
    cmi.local_pos = aClipLocalPos;

    return cmi;
}

struct ClipRect {
    RectWithSize rect;
    float mode;
};

struct ClipCorner {
    RectWithSize rect;
    vec4 outer_inner_radius;
};

struct ClipData {
    ClipRect rect;
    ClipCorner top_left;
    ClipCorner top_right;
    ClipCorner bottom_left;
    ClipCorner bottom_right;
};

ClipData fetch_clip() {
    ClipData clip;

    clip.rect = ClipRect(RectWithSize(aClipLocalRect.xy, aClipLocalRect.zw), aClipMode);
    clip.top_left = ClipCorner(RectWithSize(aClipRect_TL.xy, aClipRect_TL.zw), aClipRadii_TL);
    clip.top_right = ClipCorner(RectWithSize(aClipRect_TR.xy, aClipRect_TR.zw), aClipRadii_TR);
    clip.bottom_left = ClipCorner(RectWithSize(aClipRect_BL.xy, aClipRect_BL.zw), aClipRadii_BL);
    clip.bottom_right = ClipCorner(RectWithSize(aClipRect_BR.xy, aClipRect_BR.zw), aClipRadii_BR);

    return clip;
}

void main(void) {
    ClipMaskInstanceRect cmi = fetch_clip_item();
    Transform clip_transform = fetch_transform(cmi.shared.clip_transform_id);
    Transform prim_transform = fetch_transform(cmi.shared.prim_transform_id);
    ClipData clip = fetch_clip();

    RectWithSize local_rect = clip.rect.rect;
    local_rect.p0 = cmi.local_pos;

    ClipVertexInfo vi = write_clip_tile_vertex(
        local_rect,
        prim_transform,
        clip_transform,
        cmi.shared.sub_rect,
        cmi.shared.task_origin,
        cmi.shared.screen_origin,
        cmi.shared.device_pixel_scale
    );

    vClipMode = clip.rect.mode;
    vLocalPos = vi.local_pos;

#ifdef WR_FEATURE_FAST_PATH
    // If the radii are all uniform, we can use a much simpler 2d
    // signed distance function to get a rounded rect clip.
    vec2 half_size = 0.5 * local_rect.size;
    float radius = clip.top_left.outer_inner_radius.x;
    vLocalPos.xy -= (half_size + cmi.local_pos) * vi.local_pos.w;
    vClipParams = vec3(half_size - vec2(radius), radius);
#else
    RectWithEndpoint clip_rect = to_rect_with_endpoint(local_rect);

    vec2 r_tl = clip.top_left.outer_inner_radius.xy;
    vec2 r_tr = clip.top_right.outer_inner_radius.xy;
    vec2 r_br = clip.bottom_right.outer_inner_radius.xy;
    vec2 r_bl = clip.bottom_left.outer_inner_radius.xy;

    vClipCenter_Radius_TL = vec4(clip_rect.p0 + r_tl, r_tl);

    vClipCenter_Radius_TR = vec4(clip_rect.p1.x - r_tr.x,
                                 clip_rect.p0.y + r_tr.y,
                                 r_tr);

    vClipCenter_Radius_BR = vec4(clip_rect.p1 - r_br, r_br);

    vClipCenter_Radius_BL = vec4(clip_rect.p0.x + r_bl.x,
                                 clip_rect.p1.y - r_bl.y,
                                 r_bl);
#endif
}
#endif

#ifdef WR_FRAGMENT_SHADER

#ifdef WR_FEATURE_FAST_PATH
// See http://www.iquilezles.org/www/articles/distfunctions2d/distfunctions2d.htm
float sd_box(in vec2 pos, in vec2 box_size) {
    vec2 d = abs(pos) - box_size;
    return length(max(d, vec2(0.0))) + min(max(d.x,d.y), 0.0);
}

float sd_rounded_box(in vec2 pos, in vec2 box_size, in float radius) {
    return sd_box(pos, box_size) - radius;
}
#endif

void main(void) {
    vec2 local_pos = vLocalPos.xy / vLocalPos.w;
    float aa_range = compute_aa_range(local_pos);

#ifdef WR_FEATURE_FAST_PATH
    float d = sd_rounded_box(local_pos, vClipParams.xy, vClipParams.z);
    float f = distance_aa(aa_range, d);
    float final_alpha = mix(f, 1.0 - f, vClipMode);
#else
    float alpha = init_transform_fs(local_pos);

    float clip_alpha = rounded_rect(local_pos,
                                    vClipCenter_Radius_TL,
                                    vClipCenter_Radius_TR,
                                    vClipCenter_Radius_BR,
                                    vClipCenter_Radius_BL,
                                    aa_range);

    float combined_alpha = alpha * clip_alpha;

    // Select alpha or inverse alpha depending on clip in/out.
    float final_alpha = mix(combined_alpha, 1.0 - combined_alpha, vClipMode);
#endif

    float final_final_alpha = vLocalPos.w > 0.0 ? final_alpha : 0.0;
    oFragColor = vec4(final_final_alpha, 0.0, 0.0, 1.0);
}
#endif
