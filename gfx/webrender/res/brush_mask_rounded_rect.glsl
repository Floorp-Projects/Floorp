/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define VECS_PER_SPECIFIC_BRUSH 4

#include shared,prim_shared,ellipse,brush

flat varying float vClipMode;
flat varying vec4 vClipCenter_Radius_TL;
flat varying vec4 vClipCenter_Radius_TR;
flat varying vec4 vClipCenter_Radius_BR;
flat varying vec4 vClipCenter_Radius_BL;
flat varying vec4 vLocalRect;
varying vec2 vLocalPos;

#ifdef WR_VERTEX_SHADER

struct RoundedRectPrimitive {
    float clip_mode;
    vec4 rect;
    vec2 radius_tl;
    vec2 radius_tr;
    vec2 radius_br;
    vec2 radius_bl;
};

RoundedRectPrimitive fetch_rounded_rect_primitive(int address) {
    vec4 data[4] = fetch_from_resource_cache_4(address);
    return RoundedRectPrimitive(
        data[0].x,
        data[1],
        data[2].xy,
        data[2].zw,
        data[3].xy,
        data[3].zw
    );
}

void brush_vs(
    int prim_address,
    vec2 local_pos,
    RectWithSize local_rect,
    ivec2 user_data,
    PictureTask pic_task
) {
    // Load the specific primitive.
    RoundedRectPrimitive prim = fetch_rounded_rect_primitive(prim_address);

    // Write clip parameters
    vClipMode = prim.clip_mode;

    // TODO(gw): In the future, when brush primitives may be segment rects
    //           we need to account for that here, and differentiate between
    //           the segment rect (geometry) amd the primitive rect (which
    //           defines where the clip radii are relative to).
    vec4 clip_rect = vec4(prim.rect.xy, prim.rect.xy + prim.rect.zw);

    vClipCenter_Radius_TL = vec4(clip_rect.xy + prim.radius_tl, prim.radius_tl);
    vClipCenter_Radius_TR = vec4(clip_rect.zy + vec2(-prim.radius_tr.x, prim.radius_tr.y), prim.radius_tr);
    vClipCenter_Radius_BR = vec4(clip_rect.zw - prim.radius_br, prim.radius_br);
    vClipCenter_Radius_BL = vec4(clip_rect.xw + vec2(prim.radius_bl.x, -prim.radius_bl.y), prim.radius_bl);

    vLocalRect = clip_rect;
    vLocalPos = local_pos;
}
#endif

#ifdef WR_FRAGMENT_SHADER
vec4 brush_fs() {
    // TODO(gw): The mask code below is super-inefficient. Once we
    // start using primitive segments in brush shaders, this can
    // be made much faster.

    // NOTE: The AA range must be computed outside the if statement,
    //       since otherwise the results can be undefined if the
    //       input function is not continuous. I have observed this
    //       as flickering behaviour on Intel GPUs.
    float aa_range = compute_aa_range(vLocalPos);

    // Apply ellipse clip on each corner.
    float d = 0.0;

    if (vLocalPos.x > vLocalRect.x &&
        vLocalPos.y > vLocalRect.y &&
        vLocalPos.x <= vLocalRect.z &&
        vLocalPos.y <= vLocalRect.w) {
        d = rounded_rect(vLocalPos,
                         vClipCenter_Radius_TL,
                         vClipCenter_Radius_TR,
                         vClipCenter_Radius_BR,
                         vClipCenter_Radius_BL,
                         aa_range);
    }

    return vec4(mix(d, 1.0 - d, vClipMode));
}
#endif
