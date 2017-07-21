#line 1
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
