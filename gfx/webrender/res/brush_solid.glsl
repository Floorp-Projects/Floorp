/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
    int prim_address,
    vec2 local_pos,
    RectWithSize local_rect,
    ivec2 user_data
) {
    SolidBrush prim = fetch_solid_primitive(prim_address);
    vColor = prim.color;

#ifdef WR_FEATURE_ALPHA_PASS
    vLocalPos = local_pos;
#endif
}
#endif

#ifdef WR_FRAGMENT_SHADER
vec4 brush_fs() {
    vec4 color = vColor;
#ifdef WR_FEATURE_ALPHA_PASS
    color *= init_transform_fs(vLocalPos);
#endif
    return color;
}
#endif
