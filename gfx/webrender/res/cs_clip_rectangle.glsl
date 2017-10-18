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

ClipCorner fetch_clip_corner(ivec2 address) {
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

    // Read the corners in groups of two texels, and adjust the read address
    // before every read.
    // The address adjustment is done inside this function, and not by passing
    // the corner index to fetch_clip_corner and computing the correct address
    // there, because doing so was hitting a driver bug on certain Intel macOS
    // drivers which creates wrong results when doing arithmetic with integer
    // variables (under certain, unknown, circumstances).
    address.x += 2;
    clip.top_left = fetch_clip_corner(address);
    address.x += 2;
    clip.top_right = fetch_clip_corner(address);
    address.x += 2;
    clip.bottom_left = fetch_clip_corner(address);
    address.x += 2;
    clip.bottom_right = fetch_clip_corner(address);

    return clip;
}

void main(void) {
    ClipMaskInstance cmi = fetch_clip_item();
    ClipArea area = fetch_clip_area(cmi.render_task_address);
    Layer layer = fetch_layer(cmi.layer_address);
    ClipData clip = fetch_clip(cmi.clip_data_address);
    RectWithSize local_rect = clip.rect.rect;

    ClipVertexInfo vi = write_clip_tile_vertex(local_rect,
                                               layer,
                                               area,
                                               cmi.segment);
    vPos = vi.local_pos;

    vClipMode = clip.rect.mode.x;

    RectWithEndpoint clip_rect = to_rect_with_endpoint(local_rect);

    vClipCenter_Radius_TL = vec4(clip_rect.p0 + clip.top_left.outer_inner_radius.xy,
                                 clip.top_left.outer_inner_radius.xy);

    vClipCenter_Radius_TR = vec4(clip_rect.p1.x - clip.top_right.outer_inner_radius.x,
                                 clip_rect.p0.y + clip.top_right.outer_inner_radius.y,
                                 clip.top_right.outer_inner_radius.xy);

    vClipCenter_Radius_BR = vec4(clip_rect.p1 - clip.bottom_right.outer_inner_radius.xy,
                                 clip.bottom_right.outer_inner_radius.xy);

    vClipCenter_Radius_BL = vec4(clip_rect.p0.x + clip.bottom_left.outer_inner_radius.x,
                                 clip_rect.p1.y - clip.bottom_left.outer_inner_radius.y,
                                 clip.bottom_left.outer_inner_radius.xy);
}
#endif

#ifdef WR_FRAGMENT_SHADER
float clip_against_ellipse_if_needed(vec2 pos,
                                     float current_distance,
                                     vec4 ellipse_center_radius,
                                     vec2 sign_modifier,
                                     float afwidth) {
    float ellipse_distance = distance_to_ellipse(pos - ellipse_center_radius.xy,
                                                 ellipse_center_radius.zw);

    return mix(current_distance,
               ellipse_distance + afwidth,
               all(lessThan(sign_modifier * pos, sign_modifier * ellipse_center_radius.xy)));
}

float rounded_rect(vec2 pos) {
    float current_distance = 0.0;

    // Apply AA
    float afwidth = 0.5 * length(fwidth(pos));

    // Clip against each ellipse.
    current_distance = clip_against_ellipse_if_needed(pos,
                                                      current_distance,
                                                      vClipCenter_Radius_TL,
                                                      vec2(1.0),
                                                      afwidth);

    current_distance = clip_against_ellipse_if_needed(pos,
                                                      current_distance,
                                                      vClipCenter_Radius_TR,
                                                      vec2(-1.0, 1.0),
                                                      afwidth);

    current_distance = clip_against_ellipse_if_needed(pos,
                                                      current_distance,
                                                      vClipCenter_Radius_BR,
                                                      vec2(-1.0),
                                                      afwidth);

    current_distance = clip_against_ellipse_if_needed(pos,
                                                      current_distance,
                                                      vClipCenter_Radius_BL,
                                                      vec2(1.0, -1.0),
                                                      afwidth);

    return smoothstep(0.0, afwidth, 1.0 - current_distance);
}


void main(void) {
    float alpha = 1.f;
    vec2 local_pos = init_transform_fs(vPos, alpha);

    float clip_alpha = rounded_rect(local_pos);

    float combined_alpha = min(alpha, clip_alpha);

    // Select alpha or inverse alpha depending on clip in/out.
    float final_alpha = mix(combined_alpha, 1.0 - combined_alpha, vClipMode);

    oFragColor = vec4(final_alpha, 0.0, 0.0, 1.0);
}
#endif
