/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include shared,prim_shared,brush

#ifdef WR_FEATURE_ALPHA_PASS
varying vec2 vLocalPos;
#endif

varying vec3 vUv;
flat varying int vImageKind;
flat varying vec4 vUvBounds;
flat varying vec4 vUvBounds_NoClamp;
flat varying vec4 vParams;

#if defined WR_FEATURE_ALPHA_TARGET || defined WR_FEATURE_COLOR_TARGET_ALPHA_MASK
flat varying vec4 vColor;
#endif

#define BRUSH_IMAGE_SIMPLE      0
#define BRUSH_IMAGE_NINEPATCH   1
#define BRUSH_IMAGE_MIRROR      2

#ifdef WR_VERTEX_SHADER
void brush_vs(
    int prim_address,
    vec2 local_pos,
    RectWithSize local_rect,
    ivec2 user_data
) {
    // TODO(gw): For now, this brush_image shader is only
    //           being used to draw items from the intermediate
    //           surface cache (render tasks). In the future
    //           we can expand this to support items from
    //           the normal texture cache and unify this
    //           with the normal image shader.
    BlurTask blur_task = fetch_blur_task(user_data.x);
    vUv.z = blur_task.common_data.texture_layer_index;
    vImageKind = user_data.y;

#if defined WR_FEATURE_COLOR_TARGET
    vec2 texture_size = vec2(textureSize(sColor0, 0).xy);
#elif defined WR_FEATURE_COLOR_TARGET_ALPHA_MASK
    vec2 texture_size = vec2(textureSize(sColor0, 0).xy);
    vColor = blur_task.color;
#else
    vec2 texture_size = vec2(textureSize(sColor1, 0).xy);
    vColor = blur_task.color;
#endif

    vec2 uv0 = blur_task.common_data.task_rect.p0;
    vec2 src_size = blur_task.common_data.task_rect.size * blur_task.scale_factor;
    vec2 uv1 = uv0 + blur_task.common_data.task_rect.size;

    // TODO(gw): In the future we'll probably draw these as segments
    //           with the brush shader. When that occurs, we can
    //           modify the UVs for each segment in the VS, and the
    //           FS can become a simple shader that doesn't need
    //           to adjust the UVs.

    switch (vImageKind) {
        case BRUSH_IMAGE_SIMPLE: {
            vec2 f = (local_pos - local_rect.p0) / local_rect.size;
            vUv.xy = mix(uv0, uv1, f);
            vUv.xy /= texture_size;
            break;
        }
        case BRUSH_IMAGE_NINEPATCH: {
            vec2 local_src_size = src_size / uDevicePixelRatio;
            vUv.xy = (local_pos - local_rect.p0) / local_src_size;
            vParams.xy = vec2(0.5);
            vParams.zw = (local_rect.size / local_src_size - 0.5);
            break;
        }
        case BRUSH_IMAGE_MIRROR: {
            vec2 local_src_size = src_size / uDevicePixelRatio;
            vUv.xy = (local_pos - local_rect.p0) / local_src_size;
            vParams.xy = 0.5 * local_rect.size / local_src_size;
            break;
        }
    }

    vUvBounds = vec4(uv0 + vec2(0.5), uv1 - vec2(0.5)) / texture_size.xyxy;
    vUvBounds_NoClamp = vec4(uv0, uv1) / texture_size.xyxy;

#ifdef WR_FEATURE_ALPHA_PASS
    vLocalPos = local_pos;
#endif
}
#endif

#ifdef WR_FRAGMENT_SHADER
vec4 brush_fs() {
    vec2 uv;

    switch (vImageKind) {
        case BRUSH_IMAGE_SIMPLE: {
            uv = clamp(vUv.xy, vUvBounds.xy, vUvBounds.zw);
            break;
        }
        case BRUSH_IMAGE_NINEPATCH: {
            uv = clamp(vUv.xy, vec2(0.0), vParams.xy);
            uv += max(vec2(0.0), vUv.xy - vParams.zw);
            uv = mix(vUvBounds_NoClamp.xy, vUvBounds_NoClamp.zw, uv);
            uv = clamp(uv, vUvBounds.xy, vUvBounds.zw);
            break;
        }
        case BRUSH_IMAGE_MIRROR: {
            // Mirror and stretch the box shadow corner over the entire
            // primitives.
            uv = vParams.xy - abs(vUv.xy - vParams.xy);

            // Ensure that we don't fetch texels outside the box
            // shadow corner. This can happen, for example, when
            // drawing the outer parts of an inset box shadow.
            uv = clamp(uv, vec2(0.0), vec2(1.0));
            uv = mix(vUvBounds_NoClamp.xy, vUvBounds_NoClamp.zw, uv);
            uv = clamp(uv, vUvBounds.xy, vUvBounds.zw);
            break;
        }
    }

#if defined WR_FEATURE_COLOR_TARGET
    vec4 color = texture(sColor0, vec3(uv, vUv.z));
#elif defined WR_FEATURE_COLOR_TARGET_ALPHA_MASK
    vec4 color = vColor * texture(sColor0, vec3(uv, vUv.z)).a;
#else
    vec4 color = vColor * texture(sColor1, vec3(uv, vUv.z)).r;
#endif

#ifdef WR_FEATURE_ALPHA_PASS
    color *= init_transform_fs(vLocalPos);
#endif

    return color;
}
#endif
