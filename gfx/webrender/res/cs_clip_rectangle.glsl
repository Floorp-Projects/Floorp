/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include shared,prim_shared,clip_shared,ellipse

varying vec3 vPos;
flat varying float vClipMode;
flat varying vec4 vClipCenter_Radius_TL;
flat varying vec4 vClipCenter_Radius_TR;
flat varying vec4 vClipCenter_Radius_BL;
flat varying vec4 vClipCenter_Radius_BR;

#ifdef WR_VERTEX_SHADER
struct ClipRect {
    RectWithSize rect;
    vec4 mode;
};

ClipRect fetch_clip_rect(ivec2 address) {
    vec4 data[2] = fetch_from_resource_cache_2_direct(address);
    return ClipRect(RectWithSize(data[0].xy, data[0].zw), data[1]);
}

struct ClipCorner {
    RectWithSize rect;
    vec4 outer_inner_radius;
};

// index is of type float instead of int because using an int led to shader
// miscompilations with a macOS 10.12 Intel driver.
ClipCorner fetch_clip_corner(ivec2 address, float index) {
    address += ivec2(2 + 2 * int(index), 0);
    vec4 data[2] = fetch_from_resource_cache_2_direct(address);
    return ClipCorner(RectWithSize(data[0].xy, data[0].zw), data[1]);
}

struct ClipData {
    ClipRect rect;
    ClipCorner top_left;
    ClipCorner top_right;
    ClipCorner bottom_left;
    ClipCorner bottom_right;
};

ClipData fetch_clip(ivec2 address) {
    ClipData clip;

    clip.rect = fetch_clip_rect(address);
    clip.top_left = fetch_clip_corner(address, 0.0);
    clip.top_right = fetch_clip_corner(address, 1.0);
    clip.bottom_left = fetch_clip_corner(address, 2.0);
    clip.bottom_right = fetch_clip_corner(address, 3.0);

    return clip;
}

void main(void) {
    ClipMaskInstance cmi = fetch_clip_item();
    ClipArea area = fetch_clip_area(cmi.render_task_address);
    ClipScrollNode scroll_node = fetch_clip_scroll_node(cmi.scroll_node_id);
    ClipData clip = fetch_clip(cmi.clip_data_address);
    RectWithSize local_rect = clip.rect.rect;

    ClipVertexInfo vi = write_clip_tile_vertex(local_rect, scroll_node, area);
    vPos = vi.local_pos;

    vClipMode = clip.rect.mode.x;

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
}
#endif

#ifdef WR_FRAGMENT_SHADER
void main(void) {
    vec2 local_pos = vPos.xy / vPos.z;
    float alpha = init_transform_fs(local_pos);

    float aa_range = compute_aa_range(local_pos);

    float clip_alpha = rounded_rect(local_pos,
                                    vClipCenter_Radius_TL,
                                    vClipCenter_Radius_TR,
                                    vClipCenter_Radius_BR,
                                    vClipCenter_Radius_BL,
                                    aa_range);

    float combined_alpha = alpha * clip_alpha;

    // Select alpha or inverse alpha depending on clip in/out.
    float final_alpha = mix(combined_alpha, 1.0 - combined_alpha, vClipMode);

    oFragColor = vec4(final_alpha, 0.0, 0.0, 1.0);
}
#endif
