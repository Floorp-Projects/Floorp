/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Preprocess the radii for computing the distance approximation. This should
// be used in the vertex shader if possible to avoid doing expensive division
// in the fragment shader. When dealing with a point (zero radii), approximate
// it as an ellipse with very small radii so that we don't need to branch.
vec2 inverse_radii_squared(vec2 radii) {
    return any(lessThanEqual(radii, vec2(0.0)))
        ? vec2(1.0 / (1.0e-3 * 1.0e-3))
        : 1.0 / (radii * radii);
}

#ifdef WR_FRAGMENT_SHADER

// One iteration of Newton's method on the 2D equation of an ellipse:
//
//     E(x, y) = x^2/a^2 + y^2/b^2 - 1
//
// The Jacobian of this equation is:
//
//     J(E(x, y)) = [ 2*x/a^2 2*y/b^2 ]
//
// We approximate the distance with:
//
//     E(x, y) / ||J(E(x, y))||
//
// See G. Taubin, "Distance Approximations for Rasterizing Implicit
// Curves", section 3.
float distance_to_ellipse_approx(vec2 p, vec2 inv_radii_sq) {
    float g = dot(p * p, inv_radii_sq) - 1.0;
    vec2 dG = 2.0 * p * inv_radii_sq;
    return g * inversesqrt(dot(dG, dG));
}

// Slower but more accurate version that uses the exact distance when dealing
// with a 0-radius point distance and otherwise uses the faster approximation
// when dealing with non-zero radii.
float distance_to_ellipse(vec2 p, vec2 radii) {
    return any(lessThanEqual(radii, vec2(0.0)))
        ? length(p)
        : distance_to_ellipse_approx(p, 1.0 / (radii * radii));
}

float clip_against_ellipse_if_needed(
    vec2 pos,
    vec4 ellipse_center_radius,
    vec2 sign_modifier
) {
    vec2 p = pos - ellipse_center_radius.xy;
    // If we're not within the distance bounds of both of the radii (in the
    // corner), then return a large magnitude negative value to cause us to
    // clamp off the anti-aliasing.
    return all(lessThan(sign_modifier * p, vec2(0.0)))
        ? distance_to_ellipse_approx(p, ellipse_center_radius.zw)
        : -1.0e6;
}

float rounded_rect(vec2 pos,
                   vec4 clip_center_radius_tl,
                   vec4 clip_center_radius_tr,
                   vec4 clip_center_radius_br,
                   vec4 clip_center_radius_bl,
                   float aa_range) {
    // Clip against each ellipse. If the fragment is in a corner, one of the
    // clip_against_ellipse_if_needed calls below will update it. If outside
    // any ellipse, the clip routine will return a negative value so that max()
    // will choose the greatest amount of applicable anti-aliasing.
    float current_distance = 
        max(max(clip_against_ellipse_if_needed(pos, clip_center_radius_tl, vec2(1.0)),
                clip_against_ellipse_if_needed(pos, clip_center_radius_tr, vec2(-1.0, 1.0))),
            max(clip_against_ellipse_if_needed(pos, clip_center_radius_br, vec2(-1.0)),
                clip_against_ellipse_if_needed(pos, clip_center_radius_bl, vec2(1.0, -1.0))));

    // Apply AA
    // See comment in ps_border_corner about the choice of constants.

    return distance_aa(aa_range, current_distance);
}
#endif
