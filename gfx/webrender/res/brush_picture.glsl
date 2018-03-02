/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define VECS_PER_SPECIFIC_BRUSH 1

#include shared,prim_shared,brush

#ifdef WR_FEATURE_ALPHA_PASS
varying vec2 vLocalPos;
#endif

varying vec3 vUv;
flat varying int vImageKind;
flat varying vec4 vUvBounds;
flat varying vec4 vUvBounds_NoClamp;
flat varying vec4 vParams;
flat varying vec4 vColor;

#define BRUSH_PICTURE_SIMPLE      0
#define BRUSH_PICTURE_NINEPATCH   1

#ifdef WR_VERTEX_SHADER

struct Picture {
    vec4 color;
};

Picture fetch_picture(int address) {
    vec4 data = fetch_from_resource_cache_1(address);
    return Picture(data);
}

void brush_vs(
    VertexInfo vi,
    int prim_address,
    RectWithSize local_rect,
    ivec3 user_data,
    PictureTask pic_task
) {
    vImageKind = user_data.y;

    Picture pic = fetch_picture(prim_address);
    ImageResource res = fetch_image_resource(user_data.x);
    vec2 texture_size = vec2(textureSize(sColor1, 0).xy);
    vColor = pic.color;
    vec2 uv0 = res.uv_rect.p0;
    vec2 uv1 = res.uv_rect.p1;
    vec2 src_size = (uv1 - uv0) * res.user_data.x;
    vUv.z = res.layer;

    // TODO(gw): In the future we'll probably draw these as segments
    //           with the brush shader. When that occurs, we can
    //           modify the UVs for each segment in the VS, and the
    //           FS can become a simple shader that doesn't need
    //           to adjust the UVs.

    switch (vImageKind) {
        case BRUSH_PICTURE_SIMPLE: {
            vec2 f = (vi.local_pos - local_rect.p0) / local_rect.size;
            vUv.xy = mix(uv0, uv1, f);
            vUv.xy /= texture_size;
            break;
        }
        case BRUSH_PICTURE_NINEPATCH: {
            vec2 local_src_size = src_size / uDevicePixelRatio;
            vUv.xy = (vi.local_pos - local_rect.p0) / local_src_size;
            vParams.xy = vec2(0.5);
            vParams.zw = (local_rect.size / local_src_size - 0.5);
            break;
        }
        default:
            vUv.xy = vec2(0.0);
            vParams = vec4(0.0);
    }

    vUvBounds = vec4(uv0 + vec2(0.5), uv1 - vec2(0.5)) / texture_size.xyxy;
    vUvBounds_NoClamp = vec4(uv0, uv1) / texture_size.xyxy;

#ifdef WR_FEATURE_ALPHA_PASS
    vLocalPos = vi.local_pos;
#endif
}
#endif

#ifdef WR_FRAGMENT_SHADER
vec4 brush_fs() {
    vec2 uv;

    switch (vImageKind) {
        case BRUSH_PICTURE_SIMPLE: {
            uv = clamp(vUv.xy, vUvBounds.xy, vUvBounds.zw);
            break;
        }
        case BRUSH_PICTURE_NINEPATCH: {
            uv = clamp(vUv.xy, vec2(0.0), vParams.xy);
            uv += max(vec2(0.0), vUv.xy - vParams.zw);
            uv = mix(vUvBounds_NoClamp.xy, vUvBounds_NoClamp.zw, uv);
            uv = clamp(uv, vUvBounds.xy, vUvBounds.zw);
            break;
        }
        default:
            uv = vec2(0.0);
    }

    vec4 color = vColor * texture(sColor1, vec3(uv, vUv.z)).r;

#ifdef WR_FEATURE_ALPHA_PASS
    color *= init_transform_fs(vLocalPos);
#endif

    return color;
}
#endif
