/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define VECS_PER_SPECIFIC_BRUSH 3

#include shared,prim_shared,brush

// Interpolated UV coordinates to sample.
varying vec2 v_uv;
varying vec2 v_local_pos;

// Normalized bounds of the source image in the texture, adjusted to avoid
// sampling artifacts.
flat varying vec4 v_uv_sample_bounds;

// Layer index to sample.
// Flag to allow perspective interpolation of UV.
flat varying vec2 v_layer_and_perspective;

flat varying float v_opacity;

#ifdef WR_VERTEX_SHADER
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
    ImageResource res = fetch_image_resource(prim_user_data.x);
    vec2 uv0 = res.uv_rect.p0;
    vec2 uv1 = res.uv_rect.p1;

    vec2 texture_size = vec2(textureSize(sColor0, 0).xy);
    vec2 f = (vi.local_pos - local_rect.p0) / local_rect.size;
    f = get_image_quad_uv(prim_user_data.x, f);
    vec2 uv = mix(uv0, uv1, f);
    float perspective_interpolate = (brush_flags & BRUSH_FLAG_PERSPECTIVE_INTERPOLATION) != 0 ? 1.0 : 0.0;

    v_uv = uv / texture_size * mix(vi.world_pos.w, 1.0, perspective_interpolate);
    v_layer_and_perspective.x = res.layer;
    v_layer_and_perspective.y = perspective_interpolate;

    v_uv_sample_bounds = vec4(uv0 + vec2(0.5), uv1 - vec2(0.5)) / texture_size.xyxy;

    #ifdef WR_FEATURE_ANTIALIASING
        v_local_pos = vi.local_pos;
    #endif

    v_opacity = float(prim_user_data.y) / 65536.0;
}
#endif

#ifdef WR_FRAGMENT_SHADER
Fragment brush_fs() {
    float perspective_divisor = mix(gl_FragCoord.w, 1.0, v_layer_and_perspective.y);
    vec2 uv = v_uv * perspective_divisor;
    // Clamp the uvs to avoid sampling artifacts.
    uv = clamp(uv, v_uv_sample_bounds.xy, v_uv_sample_bounds.zw);

    // No need to un-premultiply since we'll only apply a factor to the alpha.
    vec4 color = texture(sColor0, vec3(uv, v_layer_and_perspective.x));

    float alpha = v_opacity;

    #ifdef WR_FEATURE_ANTIALIASING
        alpha *= init_transform_fs(v_local_pos);
    #endif

    // Pre-multiply the contribution of the opacity factor.
    return Fragment(alpha * color);
}
#endif
