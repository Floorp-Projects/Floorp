/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define VECS_PER_SPECIFIC_BRUSH 1

#include shared,prim_shared,brush

flat varying vec4 vColor;

#ifdef WR_FEATURE_ALPHA_PASS
varying vec2 vLocalPos;
#endif

#ifdef WR_VERTEX_SHADER

struct SolidBrush {
    vec4 color;
};

SolidBrush fetch_solid_primitive(int address) {
    vec4 data = fetch_from_resource_cache_1(address);
    return SolidBrush(data);
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
    vec4 unused
) {
    SolidBrush prim = fetch_solid_primitive(prim_address);
    vColor = prim.color;

#ifdef WR_FEATURE_ALPHA_PASS
    vLocalPos = vi.local_pos;
#endif
}
#endif

#ifdef WR_FRAGMENT_SHADER
Fragment brush_fs() {
    vec4 color = vColor;
#ifdef WR_FEATURE_ALPHA_PASS
    color *= init_transform_fs(vLocalPos);
#endif
    return Fragment(color);
}
#endif
