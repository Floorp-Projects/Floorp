/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define VECS_PER_SPECIFIC_BRUSH 2

#include shared,prim_shared,brush,gradient_shared

flat varying vec2 v_center;
flat varying float v_start_radius;
flat varying float v_radius_scale;

#ifdef WR_VERTEX_SHADER

struct RadialGradient {
    vec4 center_start_end_radius;
    float ratio_xy;
    int extend_mode;
    vec2 stretch_size;
};

RadialGradient fetch_radial_gradient(int address) {
    vec4 data[2] = fetch_from_gpu_cache_2(address);
    return RadialGradient(
        data[0],
        data[1].x,
        int(data[1].y),
        data[1].zw
    );
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
    vec4 texel_rect
) {
    RadialGradient gradient = fetch_radial_gradient(prim_address);

    write_gradient_vertex(
        vi,
        local_rect,
        segment_rect,
        prim_user_data,
        brush_flags,
        texel_rect,
        gradient.extend_mode,
        gradient.stretch_size
    );

    v_center = gradient.center_start_end_radius.xy;
    v_start_radius = gradient.center_start_end_radius.z;
    if (gradient.center_start_end_radius.z != gradient.center_start_end_radius.w) {
      // Store 1/rd where rd = end_radius - start_radius
      v_radius_scale = 1.0 / (gradient.center_start_end_radius.w - gradient.center_start_end_radius.z);
    } else {
      // If rd = 0, we can't get its reciprocal. Instead, just use a zero scale.
      v_radius_scale = 0.0;
    }

    // Transform all coordinates by the y scale so the
    // fragment shader can work with circles
    v_center.y *= gradient.ratio_xy;
    v_repeated_size.y *=  gradient.ratio_xy;
}
#endif

#ifdef WR_FRAGMENT_SHADER
float get_gradient_offset() {
    // Get the brush position to solve for gradient offset.
    vec2 pos = compute_gradient_pos();

    // Rescale UV to actual repetition size. This can't be done in the vertex
    // shader due to the use of length() below.
    pos *= v_repeated_size;

    // Solve for t in length(pd) = v_start_radius + t * rd
    vec2 pd = pos - v_center;
    return (length(pd) - v_start_radius) * v_radius_scale;
}

Fragment brush_fs() {
    vec4 color = sample_gradient(get_gradient_offset());

#ifdef WR_FEATURE_ALPHA_PASS
    color *= init_transform_fs(v_local_pos);
#endif

    return Fragment(color);
}

#ifdef SWGL
void swgl_drawSpanRGBA8() {
    int address = swgl_validateGradient(sGpuCache, get_gpu_cache_uv(v_gradient_address), int(GRADIENT_ENTRIES + 2.0));
    if (address < 0) {
        return;
    }
#ifdef WR_FEATURE_ALPHA_PASS
    if (has_valid_transform_bounds()) {
        // If there is a transform, need to anti-alias the result.
        float aa_range = compute_aa_range(v_local_pos);
        while (swgl_SpanLength > 0) {
            float alpha = init_transform_fs_noperspective(v_local_pos, aa_range);
            v_local_pos += swgl_interpStep(v_local_pos);
            float offset = get_gradient_offset();
            // Handle both repeating and clamped gradients.
            offset -= floor(offset) * v_gradient_repeat;
            float entry = clamp_gradient_entry(offset);
            swgl_commitGradientColorRGBA8(sGpuCache, address, entry, alpha);
            v_pos += swgl_interpStep(v_pos);
        }
        return;
    }
#endif
    if (v_gradient_repeat != 0.0) {
        // The gradient repeats, so use fract() on the offset.
        while (swgl_SpanLength > 0) {
            float entry = clamp_gradient_entry(fract(get_gradient_offset()));
            swgl_commitGradientRGBA8(sGpuCache, address, entry);
            v_pos += swgl_interpStep(v_pos);
        }
    } else {
        // The gradient offset is only clamped.
        while (swgl_SpanLength > 0) {
            float entry = clamp_gradient_entry(get_gradient_offset());
            swgl_commitGradientRGBA8(sGpuCache, address, entry);
            v_pos += swgl_interpStep(v_pos);
        }
    }
}
#endif

#endif
