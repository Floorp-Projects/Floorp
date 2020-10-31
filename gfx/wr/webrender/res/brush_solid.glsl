/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define VECS_PER_SPECIFIC_BRUSH 1

#include shared,prim_shared,brush

flat varying vec4 v_color;

#ifdef WR_FEATURE_ALPHA_PASS
varying vec2 v_local_pos;
#endif

#ifdef WR_VERTEX_SHADER

struct SolidBrush {
    vec4 color;
};

SolidBrush fetch_solid_primitive(int address) {
    vec4 data = fetch_from_gpu_cache_1(address);
    return SolidBrush(data);
}

void brush_vs(
    VertexInfo vi,
    int prim_address,
    RectWithSize local_rect,
    RectWithSize segment_rect,
    ivec4 prim_user_data,
    int specific_resource_address,
    mat4 transform,
    PictureTask pic_task,
    int brush_flags,
    vec4 unused
) {
    SolidBrush prim = fetch_solid_primitive(prim_address);

    float opacity = float(prim_user_data.x) / 65535.0;
    v_color = prim.color * opacity;

#ifdef WR_FEATURE_ALPHA_PASS
    v_local_pos = vi.local_pos;
#endif
}
#endif

#ifdef WR_FRAGMENT_SHADER
Fragment brush_fs() {
    vec4 color = v_color;
#ifdef WR_FEATURE_ALPHA_PASS
    color *= init_transform_fs(v_local_pos);
#endif
    return Fragment(color);
}

#if defined(SWGL) && (!defined(WR_FEATURE_ALPHA_PASS) || !defined(WR_FEATURE_DUAL_SOURCE_BLENDING))
void swgl_drawSpanRGBA8() {
    #ifdef WR_FEATURE_ALPHA_PASS
        if (needs_clip()) {
            while (swgl_SpanLength > 0) {
                float alpha = init_transform_fs(v_local_pos) * do_clip();
                swgl_commitColorRGBA8(v_color, alpha);
                v_local_pos += swgl_interpStep(v_local_pos);
                vClipMaskUv += swgl_interpStep(vClipMaskUv);
            }
            return;
        } else if (has_valid_transform_bounds()) {
            while (swgl_SpanLength > 0) {
                float alpha = init_transform_fs(v_local_pos);
                swgl_commitColorRGBA8(v_color, alpha);
                v_local_pos += swgl_interpStep(v_local_pos);
            }
            return;
        }
        // No clip or transform, so just fall through to a solid span...
    #endif

    swgl_commitSolidRGBA8(v_color);
}

void swgl_drawSpanR8() {
    #ifdef WR_FEATURE_ALPHA_PASS
        if (needs_clip()) {
            while (swgl_SpanLength > 0) {
                float alpha = init_transform_fs(v_local_pos) * do_clip();
                swgl_commitColorR8(v_color.x, alpha);
                v_local_pos += swgl_interpStep(v_local_pos);
                vClipMaskUv += swgl_interpStep(vClipMaskUv);
            }
            return;
        } else if (has_valid_transform_bounds()) {
            while (swgl_SpanLength > 0) {
                float alpha = init_transform_fs(v_local_pos);
                swgl_commitColorR8(v_color.x, alpha);
                v_local_pos += swgl_interpStep(v_local_pos);
            }
            return;
        }
        // No clip or transform, so just fall through to a solid span...
    #endif

    swgl_commitSolidR8(v_color.x);
}
#endif

#endif
