/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define VECS_PER_SPECIFIC_BRUSH 3
#define WR_FEATURE_TEXTURE_2D

#include shared,prim_shared,brush,blend

// Interpolated UV coordinates to sample.
varying highp vec2 v_uv;

// Normalized bounds of the source image in the texture, adjusted to avoid
// sampling artifacts.
flat varying highp vec4 v_uv_sample_bounds;

// x: Flag to allow perspective interpolation of UV.
// y: Filter-dependent "amount" parameter.
// Packed in to a vector to work around bug 1630356.
flat varying mediump vec2 v_perspective_amount;
#define v_perspective v_perspective_amount.x
#define v_amount v_perspective_amount.y

// x: Blend op, y: Lookup table GPU cache address.
// Packed in to a vector to work around bug 1630356.
// Must be explicitly marked as highp, as the default integer precision in
// fragment shaders is mediump which may only be 16 bits in ESSL 3, and GPU
// cache address can exceed that maximum representable value.
flat varying highp ivec2 v_op_table_address_vec;
#define v_op v_op_table_address_vec.x
#define v_table_address v_op_table_address_vec.y

// We must keep this as highp as we encoutered shader compilation crashes on
// Mali-T devices when mediump.
flat varying highp mat4 v_color_mat;
// The function to use for each component of a component transfer filter. Using a int[4]
// or ivec4 (with each element or component containing the function for each component) has
// ran in to bugs 1695912 and 1731758, so instead use a vec4 and cast the values to/from floats.
flat varying mediump vec4 v_funcs;
flat varying mediump vec4 v_color_offset;

#ifdef WR_VERTEX_SHADER
void brush_vs(
    VertexInfo vi,
    int prim_address,
    RectWithEndpoint local_rect,
    RectWithEndpoint segment_rect,
    ivec4 prim_user_data,
    int specific_resource_address,
    mat4 transform,
    PictureTask pic_task,
    int brush_flags,
    vec4 unused
) {
    ImageSource res = fetch_image_source(prim_user_data.x);
    vec2 uv0 = res.uv_rect.p0;
    vec2 uv1 = res.uv_rect.p1;

    vec2 inv_texture_size = vec2(1.0) / vec2(TEX_SIZE(sColor0).xy);
    vec2 f = (vi.local_pos - local_rect.p0) / rect_size(local_rect);
    f = get_image_quad_uv(prim_user_data.x, f);
    vec2 uv = mix(uv0, uv1, f);
    float perspective_interpolate = (brush_flags & BRUSH_FLAG_PERSPECTIVE_INTERPOLATION) != 0 ? 1.0 : 0.0;

    v_uv = uv * inv_texture_size * mix(vi.world_pos.w, 1.0, perspective_interpolate);
    v_perspective = perspective_interpolate;

    v_uv_sample_bounds = vec4(uv0 + vec2(0.5), uv1 - vec2(0.5)) * inv_texture_size.xyxy;

    float amount = float(prim_user_data.z) / 65536.0;

    v_op = prim_user_data.y & 0xffff;
    v_amount = amount;

    v_funcs.r = float((prim_user_data.y >> 28) & 0xf);
    v_funcs.g = float((prim_user_data.y >> 24) & 0xf);
    v_funcs.b = float((prim_user_data.y >> 20) & 0xf);
    v_funcs.a = float((prim_user_data.y >> 16) & 0xf);

    SetupFilterParams(
        v_op,
        amount,
        prim_user_data.z,
        v_color_offset,
        v_color_mat,
        v_table_address
    );
}
#endif

#ifdef WR_FRAGMENT_SHADER
Fragment brush_fs() {
    float perspective_divisor = mix(gl_FragCoord.w, 1.0, v_perspective);
    vec2 uv = v_uv * perspective_divisor;
    // Clamp the uvs to avoid sampling artifacts.
    uv = clamp(uv, v_uv_sample_bounds.xy, v_uv_sample_bounds.zw);

    vec4 Cs = texture(sColor0, uv);

    float alpha;
    vec3 color;
    CalculateFilter(
        Cs,
        v_op,
        v_amount,
        v_table_address,
        v_color_offset,
        v_color_mat,
        v_funcs,
        color,
        alpha
    );

    #ifdef WR_FEATURE_ALPHA_PASS
        alpha *= antialias_brush();
    #endif

    // Pre-multiply the alpha into the output value.
    return Fragment(alpha * vec4(color, 1.0));
}
#endif
