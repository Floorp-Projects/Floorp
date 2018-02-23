/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define VECS_PER_SPECIFIC_BRUSH 0

#include shared,prim_shared,brush

#ifdef WR_FEATURE_ALPHA_PASS
varying vec2 vLocalPos;
#endif

varying vec3 vUv;
flat varying vec4 vUvBounds;

#ifdef WR_VERTEX_SHADER

void brush_vs(
    VertexInfo vi,
    int prim_address,
    RectWithSize local_rect,
    ivec3 user_data,
    PictureTask pic_task
) {
    // If this is in WR_FEATURE_TEXTURE_RECT mode, the rect and size use
    // non-normalized texture coordinates.
#ifdef WR_FEATURE_TEXTURE_RECT
    vec2 texture_size = vec2(1, 1);
#else
    vec2 texture_size = vec2(textureSize(sColor0, 0));
#endif

    ImageResource res = fetch_image_resource(user_data.x);
    vec2 uv0 = res.uv_rect.p0;
    vec2 uv1 = res.uv_rect.p1;

    vUv.z = res.layer;

    vec2 f = (vi.local_pos - local_rect.p0) / local_rect.size;
    vUv.xy = mix(uv0, uv1, f);
    vUv.xy /= texture_size;

    // Handle case where the UV coords are inverted (e.g. from an
    // external image).
    vUvBounds = vec4(
        min(uv0, uv1) + vec2(0.5),
        max(uv0, uv1) - vec2(0.5)
    ) / texture_size.xyxy;

#ifdef WR_FEATURE_ALPHA_PASS
    vLocalPos = vi.local_pos;
#endif
}
#endif

#ifdef WR_FRAGMENT_SHADER
vec4 brush_fs() {
    vec2 uv = clamp(vUv.xy, vUvBounds.xy, vUvBounds.zw);

    vec4 color = TEX_SAMPLE(sColor0, vec3(uv, vUv.z));

#ifdef WR_FEATURE_ALPHA_PASS
    color *= init_transform_fs(vLocalPos);
#endif

    return color;
}
#endif
