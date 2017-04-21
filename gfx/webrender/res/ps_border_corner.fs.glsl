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

    // Only apply the clip AA if inside the clip region. This is
    // necessary for correctness when the border width is greater
    // than the border radius.
    if (all(lessThan(local_pos * vClipSign, vClipCenter * vClipSign))) {
        vec2 p = local_pos - vClipCenter;

        // Get signed distance from the inner/outer clips.
        float d0 = distance_to_ellipse(p, vOuterRadii);
        float d1 = distance_to_ellipse(p, vInnerRadii);

        // Signed distance field subtract
        float d = max(d0, 0.5 * afwidth - d1);

        // Only apply AA to fragments outside the signed distance field.
        alpha = min(alpha, 1.0 - smoothstep(0.0, afwidth, d));
    }

    // Select color based on side of line. Get distance from the
    // reference line, and then apply AA along the edge.
    float ld = distance_to_line(vColorEdgeLine.xy, vColorEdgeLine.zw, local_pos);
    float m = smoothstep(-0.5 * afwidth, 0.5 * afwidth, ld);
    vec4 color = mix(vColor0, vColor1, m);

    oFragColor = color * vec4(1.0, 1.0, 1.0, alpha);
}
