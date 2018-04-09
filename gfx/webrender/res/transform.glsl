/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define EPSILON     0.0001

flat varying vec4 vTransformBounds;

#ifdef WR_VERTEX_SHADER

void init_transform_vs(vec4 local_bounds) {
    vTransformBounds = local_bounds;
}

#endif //WR_VERTEX_SHADER

#ifdef WR_FRAGMENT_SHADER

/// Find the appropriate half range to apply the AA approximation over.
/// This range represents a coefficient to go from one CSS pixel to half a device pixel.
float compute_aa_range(vec2 position) {
    // The constant factor is chosen to compensate for the fact that length(fw) is equal
    // to sqrt(2) times the device pixel ratio in the typical case. 0.5/sqrt(2) = 0.35355.
    //
    // This coefficient is chosen to ensure that any sample 0.5 pixels or more inside of
    // the shape has no anti-aliasing applied to it (since pixels are sampled at their center,
    // such a pixel (axis aligned) is fully inside the border). We need this so that antialiased
    // curves properly connect with non-antialiased vertical or horizontal lines, among other things.
    //
    // Lines over a half-pixel away from the pixel center *can* intersect with the pixel square;
    // indeed, unless they are horizontal or vertical, they are guaranteed to. However, choosing
    // a nonzero area for such pixels causes noticeable artifacts at the junction between an anti-
    // aliased corner and a straight edge.
    //
    // We may want to adjust this constant in specific scenarios (for example keep the principled
    // value for straight edges where we want pixel-perfect equivalence with non antialiased lines
    // when axis aligned, while selecting a larger and smoother aa range on curves).
    return 0.35355 * length(fwidth(position));
}

/// Return the blending coefficient for distance antialiasing.
///
/// 0.0 means inside the shape, 1.0 means outside.
///
/// This cubic polynomial approximates the area of a 1x1 pixel square under a
/// line, given the signed Euclidean distance from the center of the square to
/// that line. Calculating the *exact* area would require taking into account
/// not only this distance but also the angle of the line. However, in
/// practice, this complexity is not required, as the area is roughly the same
/// regardless of the angle.
///
/// The coefficients of this polynomial were determined through least-squares
/// regression and are accurate to within 2.16% of the total area of the pixel
/// square 95% of the time, with a maximum error of 3.53%.
///
/// See the comments in `compute_aa_range()` for more information on the
/// cutoff values of -0.5 and 0.5.
float distance_aa(float aa_range, float signed_distance) {
    float dist = 0.5 * signed_distance / aa_range;
    if (dist <= -0.5 + EPSILON)
        return 1.0;
    if (dist >= 0.5 - EPSILON)
        return 0.0;
    return 0.5 + dist * (0.8431027 * dist * dist - 1.14453603);
}

float signed_distance_rect(vec2 pos, vec2 p0, vec2 p1) {
    vec2 d = max(p0 - pos, pos - p1);
    return length(max(vec2(0.0), d)) + min(0.0, max(d.x, d.y));
}

float init_transform_fs(vec2 local_pos) {
    // Get signed distance from local rect bounds.
    float d = signed_distance_rect(
        local_pos,
        vTransformBounds.xy,
        vTransformBounds.zw
    );

    // Find the appropriate distance to apply the AA smoothstep over.
    float aa_range = compute_aa_range(local_pos);

    // Only apply AA to fragments outside the signed distance field.
    return distance_aa(aa_range, d);
}

float init_transform_rough_fs(vec2 local_pos) {
    return point_inside_rect(
        local_pos,
        vTransformBounds.xy,
        vTransformBounds.zw
    );
}

#endif //WR_FRAGMENT_SHADER
