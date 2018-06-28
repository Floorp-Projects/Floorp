/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define VECS_PER_SPECIFIC_BRUSH 2

#include shared,prim_shared,brush

flat varying int vGradientAddress;
flat varying float vGradientRepeat;

flat varying vec2 vScaledDir;
flat varying vec2 vStartPoint;
// Size of the gradient pattern's rectangle, used to compute horizontal and vertical
// repetitions. Not to be confused with another kind of repetition of the pattern
// which happens along the gradient stops.
flat varying vec2 vRepeatedSize;

varying vec2 vPos;

#ifdef WR_FEATURE_ALPHA_PASS
varying vec2 vLocalPos;
flat varying vec2 vTileRepeat;
#endif

#ifdef WR_VERTEX_SHADER

struct Gradient {
    vec4 start_end_point;
    int extend_mode;
    vec2 stretch_size;
};

Gradient fetch_gradient(int address) {
    vec4 data[2] = fetch_from_resource_cache_2(address);
    return Gradient(
        data[0],
        int(data[1].x),
        data[1].yz
    );
}

void brush_vs(
    VertexInfo vi,
    int prim_address,
    RectWithSize local_rect,
    RectWithSize segment_rect,
    ivec3 user_data,
    mat4 transform,
    PictureTask pic_task,
    int brush_flags,
    vec4 texel_rect
) {
    Gradient gradient = fetch_gradient(prim_address);

    if ((brush_flags & BRUSH_FLAG_SEGMENT_RELATIVE) != 0) {
        vPos = (vi.local_pos - segment_rect.p0) / segment_rect.size;
        vPos = vPos * (texel_rect.zw - texel_rect.xy) + texel_rect.xy;
    } else {
        vPos = vi.local_pos - local_rect.p0;
    }

    vec2 start_point = gradient.start_end_point.xy;
    vec2 end_point = gradient.start_end_point.zw;
    vec2 dir = end_point - start_point;

    vStartPoint = start_point;
    vScaledDir = dir / dot(dir, dir);

    vec2 tile_repeat = local_rect.size / gradient.stretch_size;
    vRepeatedSize = gradient.stretch_size;

    vGradientAddress = user_data.x;

    // Whether to repeat the gradient along the line instead of clamping.
    vGradientRepeat = float(gradient.extend_mode != EXTEND_MODE_CLAMP);

#ifdef WR_FEATURE_ALPHA_PASS
    vTileRepeat = tile_repeat;
    vLocalPos = vi.local_pos;
#endif
}
#endif

#ifdef WR_FRAGMENT_SHADER
Fragment brush_fs() {

#ifdef WR_FEATURE_ALPHA_PASS
    // Handle top and left inflated edges (see brush_image).
    vec2 local_pos = max(vPos, vec2(0.0));

    // Apply potential horizontal and vertical repetitions.
    vec2 pos = mod(local_pos, vRepeatedSize);

    vec2 prim_size = vRepeatedSize * vTileRepeat;
    // Handle bottom and right inflated edges (see brush_image).
    if (local_pos.x >= prim_size.x) {
        pos.x = vRepeatedSize.x;
    }
    if (local_pos.y >= prim_size.y) {
        pos.y = vRepeatedSize.y;
    }
#else
    // Apply potential horizontal and vertical repetitions.
    vec2 pos = mod(vPos, vRepeatedSize);
#endif

    float offset = dot(pos - vStartPoint, vScaledDir);

    vec4 color = sample_gradient(vGradientAddress,
                                 offset,
                                 vGradientRepeat);

#ifdef WR_FEATURE_ALPHA_PASS
    color *= init_transform_fs(vLocalPos);
#endif

    return Fragment(color);
}
#endif
