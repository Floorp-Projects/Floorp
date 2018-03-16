/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define VECS_PER_SPECIFIC_BRUSH 2

#include shared,prim_shared,brush

#ifdef WR_FEATURE_ALPHA_PASS
varying vec2 vLocalPos;
#endif

varying vec3 vUv;
flat varying vec4 vUvBounds;

#ifdef WR_FEATURE_ALPHA_PASS
flat varying vec2 vSelect;
flat varying vec4 vUvClipBounds;
flat varying vec4 vColor;
#endif

#ifdef WR_VERTEX_SHADER

#ifdef WR_FEATURE_ALPHA_PASS
    #define IMAGE_SOURCE_COLOR              0
    #define IMAGE_SOURCE_ALPHA              1
    #define IMAGE_SOURCE_MASK_FROM_COLOR    2
#endif

struct ImageBrush {
    RectWithSize rendered_task_rect;
    vec4 color;
};

ImageBrush fetch_image_primitive(int address) {
    vec4[2] data = fetch_from_resource_cache_2(address);
    RectWithSize rendered_task_rect = RectWithSize(data[0].xy, data[0].zw);
    ImageBrush brush = ImageBrush(rendered_task_rect, data[1]);
    return brush;
}

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

    // Handle case where the UV coords are inverted (e.g. from an
    // external image).
    vec2 min_uv = min(uv0, uv1);
    vec2 max_uv = max(uv0, uv1);

    vUvBounds = vec4(
        min_uv + vec2(0.5),
        max_uv - vec2(0.5)
    ) / texture_size.xyxy;

    vec2 f;

#ifdef WR_FEATURE_ALPHA_PASS
    ImageBrush image = fetch_image_primitive(prim_address);
    vColor = image.color;

    // Derive the texture coordinates for this image, based on
    // whether the source image is a local-space or screen-space
    // image.
    switch (user_data.z) {
        case RASTER_SCREEN:
            f = (vi.snapped_device_pos - image.rendered_task_rect.p0) / image.rendered_task_rect.size;

            vUvClipBounds = vec4(
                min_uv,
                max_uv
            ) / texture_size.xyxy;
            break;
        case RASTER_LOCAL:
        default: {
            f = (vi.local_pos - local_rect.p0) / local_rect.size;

            // Set the clip bounds to a value that won't have any
            // effect for local space images.
            vUvClipBounds = vec4(0.0, 0.0, 1.0, 1.0);
            break;
        }
    }
#else
    f = (vi.local_pos - local_rect.p0) / local_rect.size;
#endif

    vUv.xy = mix(uv0, uv1, f);
    vUv.xy /= texture_size;

#ifdef WR_FEATURE_ALPHA_PASS
    switch (user_data.y) {
        case IMAGE_SOURCE_COLOR:
            vSelect = vec2(0.0, 0.0);
            break;
        case IMAGE_SOURCE_ALPHA:
            vSelect = vec2(0.0, 1.0);
            break;
        case IMAGE_SOURCE_MASK_FROM_COLOR:
            vSelect = vec2(1.0, 1.0);
            break;
    }

    vLocalPos = vi.local_pos;
#endif
}
#endif

#ifdef WR_FRAGMENT_SHADER
vec4 brush_fs() {
    vec2 uv = clamp(vUv.xy, vUvBounds.xy, vUvBounds.zw);

    vec4 texel = TEX_SAMPLE(sColor0, vec3(uv, vUv.z));

#ifdef WR_FEATURE_ALPHA_PASS
    vec4 mask = mix(texel.rrrr, texel.aaaa, vSelect.x);
    vec4 color = mix(texel, vColor * mask, vSelect.y) * init_transform_fs(vLocalPos);

    // Fail-safe to ensure that we don't sample outside the rendered
    // portion of a picture source.
    color.a *= point_inside_rect(vUv.xy, vUvClipBounds.xy, vUvClipBounds.zw);
#else
    vec4 color = texel;
#endif

    return color;
}
#endif
