/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef WR_VERTEX_SHADER

/// Given a point within a local rectangle, and the device space corners
/// offsets for the unsnapped primitive, return the snap offsets. This *must*
/// exactly match the logic in the Rust compute_snap_offset_impl function.
vec2 compute_snap_offset(
    vec2 reference_pos,
    RectWithSize reference_rect,
    vec4 snap_positions
) {
    /// Compute the position of this vertex inside the snap rectangle.
    vec2 normalized_snap_pos = (reference_pos - reference_rect.p0) / reference_rect.size;

    /// Compute the actual world offset for this vertex needed to make it snap.
    return mix(snap_positions.xy, snap_positions.zw, normalized_snap_pos);
}

#endif //WR_VERTEX_SHADER
