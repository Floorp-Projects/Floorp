/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#ifdef WR_VERTEX_SHADER
#define VECS_PER_RENDER_TASK        2U

uniform HIGHP_SAMPLER_FLOAT sampler2D sRenderTasks;

struct RenderTaskData {
    RectWithEndpoint task_rect;
    vec4 user_data;
};

// See RenderTaskData in render_task.rs
RenderTaskData fetch_render_task_data(int index) {
    ivec2 uv = get_fetch_uv(index, VECS_PER_RENDER_TASK);

    vec4 texel0 = TEXEL_FETCH(sRenderTasks, uv, 0, ivec2(0, 0));
    vec4 texel1 = TEXEL_FETCH(sRenderTasks, uv, 0, ivec2(1, 0));

    RectWithEndpoint task_rect = RectWithEndpoint(
        texel0.xy,
        texel0.zw
    );

    RenderTaskData data = RenderTaskData(
        task_rect,
        texel1
    );

    return data;
}

RectWithEndpoint fetch_render_task_rect(int index) {
    ivec2 uv = get_fetch_uv(index, VECS_PER_RENDER_TASK);

    vec4 texel0 = TEXEL_FETCH(sRenderTasks, uv, 0, ivec2(0, 0));
    vec4 texel1 = TEXEL_FETCH(sRenderTasks, uv, 0, ivec2(1, 0));

    RectWithEndpoint task_rect = RectWithEndpoint(
        texel0.xy,
        texel0.zw
    );

    return task_rect;
}

#define PIC_TYPE_IMAGE          1
#define PIC_TYPE_TEXT_SHADOW    2

/*
 The dynamic picture that this brush exists on. Right now, it
 contains minimal information. In the future, it will describe
 the transform mode of primitives on this picture, among other things.
 */
struct PictureTask {
    RectWithEndpoint task_rect;
    float device_pixel_scale;
    vec2 content_origin;
};

PictureTask fetch_picture_task(int address) {
    RenderTaskData task_data = fetch_render_task_data(address);

    PictureTask task = PictureTask(
        task_data.task_rect,
        task_data.user_data.x,
        task_data.user_data.yz
    );

    return task;
}

#define CLIP_TASK_EMPTY 0x7FFFFFFF

struct ClipArea {
    RectWithEndpoint task_rect;
    float device_pixel_scale;
    vec2 screen_origin;
};

ClipArea fetch_clip_area(int index) {
    RenderTaskData task_data;
    if (index >= CLIP_TASK_EMPTY) {
      // We deliberately create a dummy RenderTaskData here then convert to a
      // ClipArea after this if-else statement, rather than initialize the
      // ClipArea in separate branches, to avoid a miscompile in some Adreno
      // drivers. See bug 1884791. Unfortunately the specific details of the bug
      // are unknown, so please take extra care not to regress this when
      // refactoring.
      task_data = RenderTaskData(RectWithEndpoint(vec2(0.0), vec2(0.0)),
                                 vec4(0.0));
    } else {
      task_data = fetch_render_task_data(index);
    }

    return ClipArea(task_data.task_rect, task_data.user_data.x,
                    task_data.user_data.yz);
}

#endif //WR_VERTEX_SHADER
