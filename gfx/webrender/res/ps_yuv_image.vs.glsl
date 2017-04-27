#line 1
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

void main(void) {
    Primitive prim = load_primitive();
#ifdef WR_FEATURE_TRANSFORM
    TransformVertexInfo vi = write_transform_vertex(prim.local_rect,
                                                    prim.local_clip_rect,
                                                    prim.z,
                                                    prim.layer,
                                                    prim.task,
                                                    prim.local_rect.p0);
    vLocalRect = prim.local_rect;
    vLocalPos = vi.local_pos;
#else
    VertexInfo vi = write_vertex(prim.local_rect,
                                 prim.local_clip_rect,
                                 prim.z,
                                 prim.layer,
                                 prim.task,
                                 prim.local_rect.p0);
    vLocalPos = vi.local_pos - prim.local_rect.p0;
#endif

    YuvImage image = fetch_yuv_image(prim.prim_index);
    ResourceRect y_rect = fetch_resource_rect(prim.user_data.x);
    ResourceRect u_rect = fetch_resource_rect(prim.user_data.x + 1);
    ResourceRect v_rect = fetch_resource_rect(prim.user_data.x + 2);

    vec2 y_texture_size = vec2(textureSize(sColor0, 0));
    vec2 y_st0 = y_rect.uv_rect.xy / y_texture_size;
    vec2 y_st1 = y_rect.uv_rect.zw / y_texture_size;

    vTextureSizeY = y_st1 - y_st0;
    vTextureOffsetY = y_st0;

    vec2 uv_texture_size = vec2(textureSize(sColor1, 0));
    vec2 u_st0 = u_rect.uv_rect.xy / uv_texture_size;
    vec2 u_st1 = u_rect.uv_rect.zw / uv_texture_size;

    vec2 v_st0 = v_rect.uv_rect.xy / uv_texture_size;
    vec2 v_st1 = v_rect.uv_rect.zw / uv_texture_size;

    // This assumes the U and V surfaces have the same size.
    vTextureSizeUv = u_st1 - u_st0;
    vTextureOffsetU = u_st0;
    vTextureOffsetV = v_st0;

    vStretchSize = image.size;

    vHalfTexelY = vec2(0.5) / y_texture_size;
    vHalfTexelUv = vec2(0.5) / uv_texture_size;

    // The constants added to the Y, U and V components are applied in the fragment shader.
    if (image.color_space == YUV_REC601) {
        // From Rec601:
        // [R]   [1.1643835616438356,  0.0,                 1.5960267857142858   ]   [Y -  16]
        // [G] = [1.1643835616438358, -0.3917622900949137, -0.8129676472377708   ] x [U - 128]
        // [B]   [1.1643835616438356,  2.017232142857143,   8.862867620416422e-17]   [V - 128]
        //
        // For the range [0,1] instead of [0,255].
        vYuvColorMatrix = mat3(
            1.16438,  0.0,      1.59603,
            1.16438, -0.39176, -0.81297,
            1.16438,  2.01723,  0.0
        );
    } else { // if (image.color_space == YUV_REC709)
        // From Rec709:
        // [R]   [1.1643835616438356,  4.2781193979771426e-17, 1.7927410714285714]   [Y -  16]
        // [G] = [1.1643835616438358, -0.21324861427372963,   -0.532909328559444 ] x [U - 128]
        // [B]   [1.1643835616438356,  2.1124017857142854,     0.0               ]   [V - 128]
        //
        // For the range [0,1] instead of [0,255]:
        vYuvColorMatrix = mat3(
            1.16438,  0.0,      1.79274,
            1.16438, -0.21325, -0.53291,
            1.16438,  2.11240,  0.0
        );
    }

    write_clip(vi.screen_pos, prim.clip_area);

}
