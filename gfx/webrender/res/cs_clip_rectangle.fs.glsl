/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
