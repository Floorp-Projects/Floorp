#line 1
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

float det(vec2 a, vec2 b) {
    return a.x * b.y - b.x * a.y;
}

// From: http://research.microsoft.com/en-us/um/people/hoppe/ravg.pdf
vec2 get_distance_vector(vec2 b0, vec2 b1, vec2 b2) {
    float a = det(b0, b2);
    float b = 2.0 * det(b1, b0);
    float d = 2.0 * det(b2, b1);

    float f = b * d - a * a;
    vec2 d21 = b2 - b1;
    vec2 d10 = b1 - b0;
    vec2 d20 = b2 - b0;

    vec2 gf = 2.0 * (b *d21 + d * d10 + a * d20);
    gf = vec2(gf.y,-gf.x);
    vec2 pp = -f * gf / dot(gf, gf);
    vec2 d0p = b0 - pp;
    float ap = det(d0p, d20);
    float bp = 2.0 * det(d10, d0p);

    float t = clamp((ap + bp) / (2.0 * a + b + d), 0.0, 1.0);
    return mix(mix(b0, b1, t), mix(b1,b2,t), t);
}

// Approximate distance from point to quadratic bezier.
float approx_distance(vec2 p, vec2 b0, vec2 b1, vec2 b2) {
    return length(get_distance_vector(b0 - p, b1 - p, b2 - p));
}

void main(void) {
    float alpha = 1.0;

#ifdef WR_FEATURE_CACHE
    vec2 local_pos = vLocalPos;
#else
    #ifdef WR_FEATURE_TRANSFORM
        alpha = 0.0;
        vec2 local_pos = init_transform_fs(vLocalPos, alpha);
    #else
        vec2 local_pos = vLocalPos;
    #endif

        alpha = min(alpha, do_clip());
#endif

    // Find the appropriate distance to apply the step over.
    vec2 fw = fwidth(local_pos);
    float afwidth = length(fw);

    // Select the x/y coord, depending on which axis this edge is.
    vec2 pos = mix(local_pos.xy, local_pos.yx, vAxisSelect);

    switch (vStyle) {
        case LINE_STYLE_SOLID: {
            break;
        }
        case LINE_STYLE_DASHED: {
            // Get the main-axis position relative to closest dot or dash.
            float x = mod(pos.x - vLocalOrigin.x, vParams.x);

            // Calculate dash alpha (on/off) based on dash length
            alpha = min(alpha, step(x, vParams.y));
            break;
        }
        case LINE_STYLE_DOTTED: {
            // Get the main-axis position relative to closest dot or dash.
            float x = mod(pos.x - vLocalOrigin.x, vParams.x);

            // Get the dot alpha
            vec2 dot_relative_pos = vec2(x, pos.y) - vParams.yz;
            float dot_distance = length(dot_relative_pos) - vParams.y;
            alpha = min(alpha, 1.0 - smoothstep(-0.5 * afwidth,
                                                0.5 * afwidth,
                                                dot_distance));
            break;
        }
        case LINE_STYLE_WAVY: {
            vec2 normalized_local_pos = pos - vLocalOrigin.xy;

            float y0 = vParams.y;
            float dy = vParams.z;
            float dx = vParams.w;

            // Flip the position of the bezier center points each
            // wave period.
            dy *= step(mod(normalized_local_pos.x, 4.0 * dx), 2.0 * dx) * 2.0 - 1.0;

            // Convert pos to a local position within one wave period.
            normalized_local_pos.x = dx + mod(normalized_local_pos.x, 2.0 * dx);

            // Evaluate SDF to the first bezier.
            vec2 b0_0 = vec2(0.0 * dx,  y0);
            vec2 b1_0 = vec2(1.0 * dx,  y0 - dy);
            vec2 b2_0 = vec2(2.0 * dx,  y0);
            float d1 = approx_distance(normalized_local_pos, b0_0, b1_0, b2_0);

            // Evaluate SDF to the second bezier.
            vec2 b0_1 = vec2(2.0 * dx,  y0);
            vec2 b1_1 = vec2(3.0 * dx,  y0 + dy);
            vec2 b2_1 = vec2(4.0 * dx,  y0);
            float d2 = approx_distance(normalized_local_pos, b0_1, b1_1, b2_1);

            // SDF union - this is needed to avoid artifacts where the
            // bezier curves join.
            float d = min(d1, d2);

            // Apply AA based on the thickness of the wave.
            alpha = 1.0 - smoothstep(vParams.x - 0.5 * afwidth,
                                     vParams.x + 0.5 * afwidth,
                                     d);
            break;
        }
    }

    oFragColor = vColor * vec4(1.0, 1.0, 1.0, alpha);
}
