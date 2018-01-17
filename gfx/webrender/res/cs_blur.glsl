/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include shared,prim_shared

varying vec3 vUv;
flat varying vec4 vUvRect;
flat varying vec2 vOffsetScale;
flat varying float vSigma;
flat varying int vBlurRadius;

#ifdef WR_VERTEX_SHADER
// Applies a separable gaussian blur in one direction, as specified
// by the dir field in the blur command.

#define DIR_HORIZONTAL  0
#define DIR_VERTICAL    1

in int aBlurRenderTaskAddress;
in int aBlurSourceTaskAddress;
in int aBlurDirection;

void main(void) {
    BlurTask blur_task = fetch_blur_task(aBlurRenderTaskAddress);
    RenderTaskCommonData src_task = fetch_render_task_common_data(aBlurSourceTaskAddress);

    RectWithSize src_rect = src_task.task_rect;
    RectWithSize target_rect = blur_task.common_data.task_rect;

#if defined WR_FEATURE_COLOR_TARGET
    vec2 texture_size = vec2(textureSize(sCacheRGBA8, 0).xy);
#else
    vec2 texture_size = vec2(textureSize(sCacheA8, 0).xy);
#endif
    vUv.z = src_task.texture_layer_index;
    vBlurRadius = int(3.0 * blur_task.blur_radius);
    vSigma = blur_task.blur_radius;

    switch (aBlurDirection) {
        case DIR_HORIZONTAL:
            vOffsetScale = vec2(1.0 / texture_size.x, 0.0);
            break;
        case DIR_VERTICAL:
            vOffsetScale = vec2(0.0, 1.0 / texture_size.y);
            break;
    }

    vUvRect = vec4(src_rect.p0 + vec2(0.5),
                   src_rect.p0 + src_rect.size - vec2(0.5));
    vUvRect /= texture_size.xyxy;

    vec2 pos = target_rect.p0 + target_rect.size * aPosition.xy;

    vec2 uv0 = src_rect.p0 / texture_size;
    vec2 uv1 = (src_rect.p0 + src_rect.size) / texture_size;
    vUv.xy = mix(uv0, uv1, aPosition.xy);

    gl_Position = uTransform * vec4(pos, 0.0, 1.0);
}
#endif

#ifdef WR_FRAGMENT_SHADER

#if defined WR_FEATURE_COLOR_TARGET
#define SAMPLE_TYPE vec4
#define SAMPLE_TEXTURE(uv)  texture(sCacheRGBA8, uv)
#else
#define SAMPLE_TYPE float
#define SAMPLE_TEXTURE(uv)  texture(sCacheA8, uv).r
#endif

// TODO(gw): Write a fast path blur that handles smaller blur radii
//           with a offset / weight uniform table and a constant
//           loop iteration count!

// TODO(gw): Make use of the bilinear sampling trick to reduce
//           the number of texture fetches needed for a gaussian blur.

void main(void) {
    SAMPLE_TYPE original_color = SAMPLE_TEXTURE(vUv);

    // TODO(gw): The gauss function gets NaNs when blur radius
    //           is zero. In the future, detect this earlier
    //           and skip the blur passes completely.
    if (vBlurRadius == 0) {
        oFragColor = vec4(original_color);
        return;
    }

    // Incremental Gaussian Coefficent Calculation (See GPU Gems 3 pp. 877 - 889)
    vec3 gauss_coefficient;
    gauss_coefficient.x = 1.0 / (sqrt(2.0 * 3.14159265) * vSigma);
    gauss_coefficient.y = exp(-0.5 / (vSigma * vSigma));
    gauss_coefficient.z = gauss_coefficient.y * gauss_coefficient.y;

    float gauss_coefficient_sum = 0.0;
    SAMPLE_TYPE avg_color = original_color * gauss_coefficient.x;
    gauss_coefficient_sum += gauss_coefficient.x;
    gauss_coefficient.xy *= gauss_coefficient.yz;

    for (int i=1 ; i <= vBlurRadius ; ++i) {
        vec2 offset = vOffsetScale * float(i);

        vec2 st0 = clamp(vUv.xy - offset, vUvRect.xy, vUvRect.zw);
        avg_color += SAMPLE_TEXTURE(vec3(st0, vUv.z)) * gauss_coefficient.x;

        vec2 st1 = clamp(vUv.xy + offset, vUvRect.xy, vUvRect.zw);
        avg_color += SAMPLE_TEXTURE(vec3(st1, vUv.z)) * gauss_coefficient.x;

        gauss_coefficient_sum += 2.0 * gauss_coefficient.x;
        gauss_coefficient.xy *= gauss_coefficient.yz;
    }

    oFragColor = vec4(avg_color) / gauss_coefficient_sum;
}
#endif
