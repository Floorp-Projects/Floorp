#line 1
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

void main(void) {
    Primitive prim = load_primitive();
    Gradient gradient = fetch_gradient(prim.prim_index);

    VertexInfo vi = write_vertex(prim.local_rect,
                                 prim.local_clip_rect,
                                 prim.z,
                                 prim.layer,
                                 prim.task);

    // Snap the start/end points to device pixel units.
    // I'm not sure this is entirely correct, but the
    // old render path does this, and it is needed to
    // make the angle gradient ref tests pass. It might
    // be better to fix this higher up in DL construction
    // and not snap here?
    vec2 start_point = floor(0.5 + gradient.start_end_point.xy * uDevicePixelRatio) / uDevicePixelRatio;
    vec2 end_point = floor(0.5 + gradient.start_end_point.zw * uDevicePixelRatio) / uDevicePixelRatio;

    vec2 dir = end_point - start_point;
    // Normalized offset of this vertex within the gradient, before clamp/repeat.
    vOffset = dot(vi.local_pos - start_point, dir) / dot(dir, dir);

    // V coordinate of gradient row in lookup texture.
    vGradientIndex = float(prim.sub_index);

    // Whether to repeat the gradient instead of clamping.
    vGradientRepeat = float(int(gradient.extend_mode.x) == EXTEND_MODE_REPEAT);
}
