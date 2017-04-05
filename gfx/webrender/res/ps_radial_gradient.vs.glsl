#line 1
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

void main(void) {
    Primitive prim = load_primitive();
    RadialGradient gradient = fetch_radial_gradient(prim.prim_index);

    VertexInfo vi = write_vertex(prim.local_rect,
                                 prim.local_clip_rect,
                                 prim.z,
                                 prim.layer,
                                 prim.task);

    vPos = vi.local_pos;

    // Snap the start/end points to device pixel units.
    // I'm not sure this is entirely correct, but the
    // old render path does this, and it is needed to
    // make the angle gradient ref tests pass. It might
    // be better to fix this higher up in DL construction
    // and not snap here?
    vStartCenter = floor(0.5 + gradient.start_end_center.xy * uDevicePixelRatio) / uDevicePixelRatio;
    vEndCenter = floor(0.5 + gradient.start_end_center.zw * uDevicePixelRatio) / uDevicePixelRatio;
    vStartRadius = gradient.start_end_radius_ratio_xy_extend_mode.x;
    vEndRadius = gradient.start_end_radius_ratio_xy_extend_mode.y;

    // Transform all coordinates by the y scale so the
    // fragment shader can work with circles
    float ratio_xy = gradient.start_end_radius_ratio_xy_extend_mode.z;
    vPos.y *= ratio_xy;
    vStartCenter.y *= ratio_xy;
    vEndCenter.y *= ratio_xy;

    // V coordinate of gradient row in lookup texture.
    vGradientIndex = float(prim.sub_index);

    // Whether to repeat the gradient instead of clamping.
    vGradientRepeat = float(int(gradient.start_end_radius_ratio_xy_extend_mode.w) == EXTEND_MODE_REPEAT);
}
