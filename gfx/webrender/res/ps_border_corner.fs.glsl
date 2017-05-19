#line 1
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

void main(void) {
    float alpha = 1.0;
#ifdef WR_FEATURE_TRANSFORM
    alpha = 0.0;
    vec2 local_pos = init_transform_fs(vLocalPos, alpha);
#else
    vec2 local_pos = vLocalPos;
#endif

    alpha = min(alpha, do_clip());

    // Find the appropriate distance to apply the AA smoothstep over.
    vec2 fw = fwidth(local_pos);
    float afwidth = length(fw);
    float distance_for_color;
    float color_mix_factor;

    // Only apply the clip AA if inside the clip region. This is
    // necessary for correctness when the border width is greater
    // than the border radius.
    if (all(lessThan(local_pos * vClipSign, vClipCenter * vClipSign))) {
        vec2 p = local_pos - vClipCenter;

        // Get signed distance from the inner/outer clips.
        float d0 = distance_to_ellipse(p, vRadii0.xy);
        float d1 = distance_to_ellipse(p, vRadii0.zw);
        float d2 = distance_to_ellipse(p, vRadii1.xy);
        float d3 = distance_to_ellipse(p, vRadii1.zw);

        // SDF subtract main radii
        float d_main = max(d0, 0.5 * afwidth - d1);

        // SDF subtract inner radii (double style borders)
        float d_inner = max(d2 - 0.5 * afwidth, -d3);

        // Select how to combine the SDF based on border style.
        float d = mix(max(d_main, -d_inner), d_main, vSDFSelect);

        // Only apply AA to fragments outside the signed distance field.
        alpha = min(alpha, 1.0 - smoothstep(0.0, 0.5 * afwidth, d));

        // Get the groove/ridge mix factor.
        color_mix_factor = smoothstep(-0.5 * afwidth,
                                      0.5 * afwidth,
                                      -d2);
    } else {
        // Handle the case where the fragment is outside the clip
        // region in a corner. This occurs when border width is
        // greater than border radius.

        // Get linear distances along horizontal and vertical edges.
        vec2 d0 = vClipSign.xx * (local_pos.xx - vEdgeDistance.xz);
        vec2 d1 = vClipSign.yy * (local_pos.yy - vEdgeDistance.yw);
        // Apply union to get the outer edge signed distance.
        float da = min(d0.x, d1.x);
        // Apply intersection to get the inner edge signed distance.
        float db = max(-d0.y, -d1.y);
        // Apply union to get both edges.
        float d = min(da, db);
        // Select fragment on/off based on signed distance.
        // No AA here, since we know we're on a straight edge
        // and the width is rounded to a whole CSS pixel.
        alpha = min(alpha, mix(vAlphaSelect, 1.0, d < 0.0));

        // Get the groove/ridge mix factor.
        // TODO(gw): Support AA for groove/ridge border edge with transforms.
        color_mix_factor = mix(0.0, 1.0, da > 0.0);
    }

    // Mix inner/outer color.
    vec4 color0 = mix(vColor00, vColor01, color_mix_factor);
    vec4 color1 = mix(vColor10, vColor11, color_mix_factor);

    // Select color based on side of line. Get distance from the
    // reference line, and then apply AA along the edge.
    float ld = distance_to_line(vColorEdgeLine.xy, vColorEdgeLine.zw, local_pos);
    float m = smoothstep(-0.5 * afwidth, 0.5 * afwidth, ld);
    vec4 color = mix(color0, color1, m);

    oFragColor = color * vec4(1.0, 1.0, 1.0, alpha);
}
