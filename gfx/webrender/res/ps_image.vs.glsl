#line 1
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

void main(void) {
    Primitive prim = load_primitive();
    Image image = fetch_image(prim.prim_index);
    ResourceRect res = fetch_resource_rect(prim.user_data0);

#ifdef WR_FEATURE_TRANSFORM
    TransformVertexInfo vi = write_transform_vertex(prim.local_rect,
                                                    prim.local_clip_rect,
                                                    prim.z,
                                                    prim.layer,
                                                    prim.task,
                                                    prim.local_rect.p0);
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

    // If this is in WR_FEATURE_TEXTURE_RECT mode, the rect and size use
    // non-normalized texture coordinates.
#ifdef WR_FEATURE_TEXTURE_RECT
    vec2 texture_size_normalization_factor = vec2(1, 1);
#else
    vec2 texture_size_normalization_factor = vec2(textureSize(sColor0, 0));
#endif

    // vUv will contain how many times this image has wrapped around the image size.
    vec2 st0 = res.uv_rect.xy / texture_size_normalization_factor;
    vec2 st1 = res.uv_rect.zw / texture_size_normalization_factor;

    vTextureSize = st1 - st0;
    vTextureOffset = st0;
    vTileSpacing = image.stretch_size_and_tile_spacing.zw;
    vStretchSize = image.stretch_size_and_tile_spacing.xy;

    // We clamp the texture coordinates to the half-pixel offset from the borders
    // in order to avoid sampling outside of the texture area.
    vec2 half_texel = vec2(0.5) / texture_size_normalization_factor;
    vStRect = vec4(min(st0, st1) + half_texel, max(st0, st1) - half_texel);
}
