/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

void main(void) {
    vec2 tc = clamp(vUv, vUvBorder.xy, vUvBorder.zw);
#ifdef WR_FEATURE_SUBPIXEL_AA
    //note: the blend mode is not compatible with clipping
    oFragColor = texture(sColor0, tc);
#else
    float alpha = texture(sColor0, tc).a;
#ifdef WR_FEATURE_TRANSFORM
    float a = 0.0;
    init_transform_fs(vLocalPos, a);
    alpha *= a;
#endif
    vec4 color = vColor;
    alpha = min(alpha, do_clip());
    oFragColor = vec4(vColor.rgb, vColor.a * alpha);
#endif
}
