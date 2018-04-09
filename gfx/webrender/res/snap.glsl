/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef WR_VERTEX_SHADER

vec4 compute_snap_positions(mat4 transform, RectWithSize snap_rect) {
    // Ensure that the snap rect is at *least* one device pixel in size.
    // TODO(gw): It's not clear to me that this is "correct". Specifically,
    //           how should it interact with sub-pixel snap rects when there
    //           is a scroll_node transform with scale present? But it does fix
    //           the test cases we have in Servo that are failing without it
    //           and seem better than not having this at all.
    snap_rect.size = max(snap_rect.size, vec2(1.0 / uDevicePixelRatio));

    // Transform the snap corners to the world space.
    vec4 world_snap_p0 = transform * vec4(snap_rect.p0, 0.0, 1.0);
    vec4 world_snap_p1 = transform * vec4(snap_rect.p0 + snap_rect.size, 0.0, 1.0);
    // Snap bounds in world coordinates, adjusted for pixel ratio. XY = top left, ZW = bottom right
    vec4 world_snap = uDevicePixelRatio * vec4(world_snap_p0.xy, world_snap_p1.xy) /
                                          vec4(world_snap_p0.ww, world_snap_p1.ww);
    return world_snap;
}

vec2 compute_snap_offset_impl(
    vec2 reference_pos,
    mat4 transform,
    RectWithSize snap_rect,
    RectWithSize reference_rect,
    vec4 snap_positions) {

    /// World offsets applied to the corners of the snap rectangle.
    vec4 snap_offsets = floor(snap_positions + 0.5) - snap_positions;

    /// Compute the position of this vertex inside the snap rectangle.
    vec2 normalized_snap_pos = (reference_pos - reference_rect.p0) / reference_rect.size;

    /// Compute the actual world offset for this vertex needed to make it snap.
    return mix(snap_offsets.xy, snap_offsets.zw, normalized_snap_pos);
}

// Compute a snapping offset in world space (adjusted to pixel ratio),
// given local position on the scroll_node and a snap rectangle.
vec2 compute_snap_offset(vec2 local_pos,
                         mat4 transform,
                         RectWithSize snap_rect) {
    vec4 snap_positions = compute_snap_positions(
        transform,
        snap_rect
    );

    vec2 snap_offsets = compute_snap_offset_impl(
        local_pos,
        transform,
        snap_rect,
        snap_rect,
        snap_positions
    );

    return snap_offsets;
}

#endif //WR_VERTEX_SHADER
