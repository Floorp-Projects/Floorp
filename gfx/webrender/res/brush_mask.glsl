/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include shared,prim_shared,ellipse

flat varying float vClipMode;
flat varying vec4 vClipCenter_Radius_TL;
flat varying vec4 vClipCenter_Radius_TR;
flat varying vec4 vClipCenter_Radius_BR;
flat varying vec4 vClipCenter_Radius_BL;

#ifdef WR_VERTEX_SHADER

struct BrushPrimitive {
    float clip_mode;
    float radius;
};

BrushPrimitive fetch_brush_primitive(int address) {
    vec4 data = fetch_from_resource_cache_1(address);
    return BrushPrimitive(data.x, data.y);
}

void brush_vs(int prim_address, vec4 prim_rect) {
    // Load the specific primitive.
    BrushPrimitive prim = fetch_brush_primitive(prim_address + 2);

    // Write clip parameters
    vClipMode = prim.clip_mode;

    vec2 r = vec2(prim.radius);
    vClipCenter_Radius_TL = vec4(prim_rect.xy + vec2(r.x, r.y), r);
    vClipCenter_Radius_TR = vec4(prim_rect.zy + vec2(-r.x, r.y), r);
    vClipCenter_Radius_BR = vec4(prim_rect.zw + vec2(-r.x, -r.y), r);
    vClipCenter_Radius_BL = vec4(prim_rect.xw + vec2(r.x, -r.y), r);
}
#endif

#ifdef WR_FRAGMENT_SHADER
vec4 brush_fs(vec2 local_pos, vec4 local_rect) {
    // TODO(gw): The mask code below is super-inefficient. Once we
    // start using primitive segments in brush shaders, this can
    // be made much faster.
    float d = 0.0;
    // Check if in valid clip region.
    if (local_pos.x >= local_rect.x && local_pos.x < local_rect.z &&
        local_pos.y >= local_rect.y && local_pos.y < local_rect.w) {
        // Apply ellipse clip on each corner.
        d = rounded_rect(local_pos,
                         vClipCenter_Radius_TL,
                         vClipCenter_Radius_TR,
                         vClipCenter_Radius_BR,
                         vClipCenter_Radius_BL);
    }

    return vec4(mix(d, 1.0 - d, vClipMode));
}
#endif

#include brush
