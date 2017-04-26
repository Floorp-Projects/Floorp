#line 1
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//
// Signed distance to an ellipse.
// Taken from http://www.iquilezles.org/www/articles/ellipsedist/ellipsedist.htm
// Note that this fails for exact circles.
//
float sdEllipse( vec2 p, in vec2 ab ) {
    p = abs( p ); if( p.x > p.y ){ p=p.yx; ab=ab.yx; }
    float l = ab.y*ab.y - ab.x*ab.x;

    float m = ab.x*p.x/l;
    float n = ab.y*p.y/l;
    float m2 = m*m;
    float n2 = n*n;

    float c = (m2 + n2 - 1.0)/3.0;
    float c3 = c*c*c;

    float q = c3 + m2*n2*2.0;
    float d = c3 + m2*n2;
    float g = m + m*n2;

    float co;

    if( d<0.0 )
    {
        float p = acos(q/c3)/3.0;
        float s = cos(p);
        float t = sin(p)*sqrt(3.0);
        float rx = sqrt( -c*(s + t + 2.0) + m2 );
        float ry = sqrt( -c*(s - t + 2.0) + m2 );
        co = ( ry + sign(l)*rx + abs(g)/(rx*ry) - m)/2.0;
    }
    else
    {
        float h = 2.0*m*n*sqrt( d );
        float s = sign(q+h)*pow( abs(q+h), 1.0/3.0 );
        float u = sign(q-h)*pow( abs(q-h), 1.0/3.0 );
        float rx = -s - u - c*4.0 + 2.0*m2;
        float ry = (s - u)*sqrt(3.0);
        float rm = sqrt( rx*rx + ry*ry );
        float p = ry/sqrt(rm-rx);
        co = (p + 2.0*g/rm - m)/2.0;
    }

    float si = sqrt( 1.0 - co*co );

    vec2 r = vec2( ab.x*co, ab.y*si );

    return length(r - p ) * sign(p.y-r.y);
}

float distance_to_line(vec2 p0, vec2 perp_dir, vec2 p) {
    vec2 dir_to_p0 = p0 - p;
    return dot(normalize(perp_dir), dir_to_p0);
}

float distance_to_ellipse(vec2 p, vec2 radii) {
    // sdEllipse fails on exact circles, so handle equal
    // radii here. The branch coherency should make this
    // a performance win for the circle case too.
    if (radii.x == radii.y) {
        return length(p) - radii.x;
    } else {
        return sdEllipse(p, radii);
    }
}

void main(void) {
    float alpha = 1.0;
#ifdef WR_FEATURE_TRANSFORM
    alpha = 0.0;
    vec2 local_pos = init_transform_fs(vLocalPos, vLocalRect, alpha);
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
