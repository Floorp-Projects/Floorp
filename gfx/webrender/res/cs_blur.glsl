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

in int aBlurRenderTaskIndex;
in int aBlurSourceTaskIndex;
in int aBlurDirection;

struct BlurCommand {
    int task_id;
    int src_task_id;
    int dir;
};

BlurCommand fetch_blur() {
    BlurCommand blur;

    blur.task_id = aBlurRenderTaskIndex;
    blur.src_task_id = aBlurSourceTaskIndex;
    blur.dir = aBlurDirection;

    return blur;
}

void main(void) {
    BlurCommand cmd = fetch_blur();
    RenderTaskData task = fetch_render_task(cmd.task_id);
    RenderTaskData src_task = fetch_render_task(cmd.src_task_id);

    vec4 local_rect = task.data0;

    vec2 pos = mix(local_rect.xy,
                   local_rect.xy + local_rect.zw,
                   aPosition.xy);

    vec2 texture_size = vec2(textureSize(sCacheRGBA8, 0).xy);
    vUv.z = src_task.data1.x;
    vBlurRadius = int(task.data1.y);
    vSigma = task.data1.y * 0.5;

    switch (cmd.dir) {
        case DIR_HORIZONTAL:
            vOffsetScale = vec2(1.0 / texture_size.x, 0.0);
            break;
        case DIR_VERTICAL:
            vOffsetScale = vec2(0.0, 1.0 / texture_size.y);
            break;
    }

    vUvRect = vec4(src_task.data0.xy + vec2(0.5),
                   src_task.data0.xy + src_task.data0.zw - vec2(0.5));
    vUvRect /= texture_size.xyxy;

    vec2 uv0 = src_task.data0.xy / texture_size;
    vec2 uv1 = (src_task.data0.xy + src_task.data0.zw) / texture_size;
    vUv.xy = mix(uv0, uv1, aPosition.xy);

    gl_Position = uTransform * vec4(pos, 0.0, 1.0);
}
#endif

#ifdef WR_FRAGMENT_SHADER
// TODO(gw): Write a fast path blur that handles smaller blur radii
//           with a offset / weight uniform table and a constant
//           loop iteration count!

// TODO(gw): Make use of the bilinear sampling trick to reduce
//           the number of texture fetches needed for a gaussian blur.

float gauss(float x, float sigma) {
    return (1.0 / sqrt(6.283185307179586 * sigma * sigma)) * exp(-(x * x) / (2.0 * sigma * sigma));
}

void main(void) {
    vec4 cache_sample = texture(sCacheRGBA8, vUv);

    // TODO(gw): The gauss function gets NaNs when blur radius
    //           is zero. In the future, detect this earlier
    //           and skip the blur passes completely.
    if (vBlurRadius == 0) {
        oFragColor = cache_sample;
        return;
    }

    vec4 color = vec4(cache_sample.rgb, 1.0) * (cache_sample.a * gauss(0.0, vSigma));

    for (int i=1 ; i < vBlurRadius ; ++i) {
        vec2 offset = vec2(float(i)) * vOffsetScale;

        vec2 st0 = clamp(vUv.xy + offset, vUvRect.xy, vUvRect.zw);
        vec4 color0 = texture(sCacheRGBA8, vec3(st0, vUv.z));

        vec2 st1 = clamp(vUv.xy - offset, vUvRect.xy, vUvRect.zw);
        vec4 color1 = texture(sCacheRGBA8, vec3(st1, vUv.z));

        // Alpha must be premultiplied in order to properly blur the alpha channel.
        float weight = gauss(float(i), vSigma);
        color += vec4(color0.rgb * color0.a, color0.a) * weight;
        color += vec4(color1.rgb * color1.a, color1.a) * weight;
    }

    // Unpremultiply the alpha.
    color.rgb /= color.a;

    oFragColor = dither(color);
}
#endif
