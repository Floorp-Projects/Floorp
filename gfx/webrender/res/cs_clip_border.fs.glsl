#line 1
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

void main(void) {
    vec2 local_pos = vPos.xy / vPos.z;

    // Get local space position relative to the clip center.
    vec2 clip_relative_pos = local_pos - vClipCenter;

    // Get the signed distances to the two clip lines.
    float d0 = distance_to_line(vPoint_Tangent0.xy,
                                vPoint_Tangent0.zw,
                                clip_relative_pos);
    float d1 = distance_to_line(vPoint_Tangent1.xy,
                                vPoint_Tangent1.zw,
                                clip_relative_pos);

    // Get AA widths based on zoom / scale etc.
    vec2 fw = fwidth(local_pos);
    float afwidth = length(fw);

    // Apply AA over half a device pixel for the clip.
    float d = smoothstep(-0.5 * afwidth,
                         0.5 * afwidth,
                         max(d0, -d1));

    oFragColor = vec4(d, 0.0, 0.0, 1.0);
}
