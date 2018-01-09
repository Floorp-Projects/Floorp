/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define VECS_PER_SPECIFIC_BRUSH 1

#include shared,prim_shared,ellipse,brush

flat varying float vClipMode;
flat varying vec4 vClipCenter_Radius;
flat varying vec4 vLocalRect;
varying vec2 vLocalPos;

#ifdef WR_VERTEX_SHADER

struct BrushMaskCornerPrimitive {
    vec2 radius;
    float clip_mode;
};

BrushMaskCornerPrimitive fetch_primitive(int address) {
    vec4 data = fetch_from_resource_cache_1(address);
    return BrushMaskCornerPrimitive(data.xy, data.z);
}

void brush_vs(
    int prim_address,
    vec2 local_pos,
    RectWithSize local_rect,
    ivec2 user_data,
    PictureTask pic_task
) {
    // Load the specific primitive.
    BrushMaskCornerPrimitive prim = fetch_primitive(prim_address);

    // Write clip parameters
    vClipMode = prim.clip_mode;
    vClipCenter_Radius = vec4(local_rect.p0 + prim.radius, prim.radius);

    vLocalRect = vec4(local_rect.p0, local_rect.p0 + local_rect.size);
    vLocalPos = local_pos;
}
#endif

#ifdef WR_FRAGMENT_SHADER
vec4 brush_fs() {
    float d = 1.0;
    // NOTE: The AA range must be computed outside the if statement,
    //       since otherwise the results can be undefined if the
    //       input function is not continuous. I have observed this
    //       as flickering behaviour on Intel GPUs.
    float aa_range = compute_aa_range(vLocalPos);
    // Check if in valid clip region.
    if (vLocalPos.x < vClipCenter_Radius.x && vLocalPos.y < vClipCenter_Radius.y) {
        // Apply ellipse clip on corner.
        d = distance_to_ellipse(vLocalPos - vClipCenter_Radius.xy,
                                vClipCenter_Radius.zw,
                                aa_range);
        d = distance_aa(aa_range, d);
    }

    return vec4(mix(d, 1.0 - d, vClipMode));
}
#endif
