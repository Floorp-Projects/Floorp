/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include shared,prim_shared,ellipse,shared_border

flat varying vec4 vColor0;
flat varying vec4 vColor1;
flat varying vec4 vColorLine;
flat varying int vFeatures;
flat varying vec2 vClipCenter;
flat varying vec4 vClipRadii;
flat varying vec2 vClipSign;

varying vec2 vPos;

#define CLIP_RADII      1
#define MIX_COLOR       2

#ifdef WR_VERTEX_SHADER

in vec2 aTaskOrigin;
in vec4 aRect;
in vec4 aColor0;
in vec4 aColor1;
in int aFlags;
in vec2 aWidths;
in vec2 aRadii;

#define SEGMENT_TOP_LEFT        0
#define SEGMENT_TOP_RIGHT       1
#define SEGMENT_BOTTOM_RIGHT    2
#define SEGMENT_BOTTOM_LEFT     3
#define SEGMENT_LEFT            4
#define SEGMENT_TOP             5
#define SEGMENT_RIGHT           6
#define SEGMENT_BOTTOM          7

vec2 get_outer_corner_scale(int segment) {
    vec2 p;

    switch (segment) {
        case SEGMENT_TOP_LEFT:
            p = vec2(0.0, 0.0);
            break;
        case SEGMENT_TOP_RIGHT:
            p = vec2(1.0, 0.0);
            break;
        case SEGMENT_BOTTOM_RIGHT:
            p = vec2(1.0, 1.0);
            break;
        case SEGMENT_BOTTOM_LEFT:
            p = vec2(0.0, 1.0);
            break;
        default:
            // Should never get hit
            p = vec2(0.0);
            break;
    }

    return p;
}

void main(void) {
    vec2 pos = aRect.xy + aRect.zw * aPosition.xy;

    int segment = aFlags & 0xff;
    int style = (aFlags >> 8) & 0xff;

    vec2 outer_scale = get_outer_corner_scale(segment);
    vec2 outer = aRect.xy + outer_scale * aRect.zw;
    vec2 clip_sign = 1.0 - 2.0 * outer_scale;

    vColor0 = aColor0;
    vColor1 = aColor1;
    vPos = pos;

    vFeatures = 0;
    vClipSign = clip_sign;
    vClipCenter = outer + clip_sign * aRadii;
    vClipRadii = vec4(aRadii, aRadii - aWidths);
    vColorLine = vec4(0.0);

    switch (segment) {
        case SEGMENT_TOP_LEFT:
        case SEGMENT_TOP_RIGHT:
        case SEGMENT_BOTTOM_RIGHT:
        case SEGMENT_BOTTOM_LEFT:
            vFeatures |= (CLIP_RADII | MIX_COLOR);
            vColorLine = vec4(outer, aWidths.y * -clip_sign.y, aWidths.x * clip_sign.x);
            break;
        default:
            break;
    }

    gl_Position = uTransform * vec4(aTaskOrigin + pos, 0.0, 1.0);
}
#endif

#ifdef WR_FRAGMENT_SHADER
void main(void) {
    float aa_range = compute_aa_range(vPos);
    float d = -1.0;

    // Apply color mix
    float mix_factor = 0.0;
    if ((vFeatures & MIX_COLOR) != 0) {
        float d_line = distance_to_line(vColorLine.xy, vColorLine.zw, vPos);
        mix_factor = distance_aa(aa_range, -d_line);
    }

    // Apply clip radii
    if ((vFeatures & CLIP_RADII) != 0) {
        vec2 p = vPos - vClipCenter;
        if (vClipSign.x * p.x < 0.0 && vClipSign.y * p.y < 0.0) {
            float d_radii_a = distance_to_ellipse(p, vClipRadii.xy, aa_range);
            float d_radii_b = distance_to_ellipse(p, vClipRadii.zw, aa_range);
            float d_radii = max(d_radii_a, -d_radii_b);
            d = max(d, d_radii);
        }
    }

    float alpha = distance_aa(aa_range, d);
    vec4 color = mix(vColor0, vColor1, mix_factor);
    oFragColor = color * alpha;
}
#endif
