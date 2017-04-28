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

    write_clip(vi.screen_pos, prim.clip_area);

    ResourceRect y_rect = fetch_resource_rect(prim.user_data.x);
    ResourceRect u_rect = fetch_resource_rect(prim.user_data.x + 1);
#ifndef WR_FEATURE_NV12
    ResourceRect v_rect = fetch_resource_rect(prim.user_data.x + 2);
#endif

    // If this is in WR_FEATURE_TEXTURE_RECT mode, the rect and size use
    // non-normalized texture coordinates.
#ifdef WR_FEATURE_TEXTURE_RECT
    vec2 y_texture_size_normalization_factor = vec2(1, 1);
#else
    vec2 y_texture_size_normalization_factor = vec2(textureSize(sColor0, 0));
#endif
    vec2 y_st0 = y_rect.uv_rect.xy / y_texture_size_normalization_factor;
    vec2 y_st1 = y_rect.uv_rect.zw / y_texture_size_normalization_factor;

    vTextureSizeY = y_st1 - y_st0;
    vTextureOffsetY = y_st0;

    // This assumes the U and V surfaces have the same size.
#ifdef WR_FEATURE_TEXTURE_RECT
    vec2 uv_texture_size_normalization_factor = vec2(1, 1);
#else
    vec2 uv_texture_size_normalization_factor = vec2(textureSize(sColor1, 0));
#endif
    vec2 u_st0 = u_rect.uv_rect.xy / uv_texture_size_normalization_factor;
    vec2 u_st1 = u_rect.uv_rect.zw / uv_texture_size_normalization_factor;

#ifndef WR_FEATURE_NV12
    vec2 v_st0 = v_rect.uv_rect.xy / uv_texture_size_normalization_factor;
#endif

    vTextureSizeUv = u_st1 - u_st0;
    vTextureOffsetU = u_st0;
#ifndef WR_FEATURE_NV12
    vTextureOffsetV = v_st0;
#endif

    YuvImage image = fetch_yuv_image(prim.prim_index);
    vStretchSize = image.size;

    vHalfTexelY = vec2(0.5) / y_texture_size_normalization_factor;
    vHalfTexelUv = vec2(0.5) / uv_texture_size_normalization_factor;
}
