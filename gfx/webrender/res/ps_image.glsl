/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include shared,prim_shared

// If this is in WR_FEATURE_TEXTURE_RECT mode, the rect and size use non-normalized
// texture coordinates. Otherwise, it uses normalized texture coordinates. Please
// check GL_TEXTURE_RECTANGLE.
flat varying vec2 vTextureOffset; // Offset of this image into the texture atlas.
flat varying vec2 vTextureSize;   // Size of the image in the texture atlas.
flat varying vec2 vTileSpacing;   // Amount of space between tiled instances of this image.
flat varying vec4 vStRect;        // Rectangle of valid texture rect.
flat varying float vLayer;

#ifdef WR_FEATURE_TRANSFORM
flat varying vec4 vLocalRect;
#endif

varying vec2 vLocalPos;
flat varying vec2 vStretchSize;

#ifdef WR_VERTEX_SHADER
void main(void) {
    Primitive prim = load_primitive();
    Image image = fetch_image(prim.specific_prim_address);
    ImageResource res = fetch_image_resource(prim.user_data0);

#ifdef WR_FEATURE_TRANSFORM
    VertexInfo vi = write_transform_vertex_primitive(prim);
    vLocalPos = vi.local_pos;
    vLocalRect = vec4(prim.local_rect.p0, prim.local_rect.p0 + prim.local_rect.size);
#else
    VertexInfo vi = write_vertex(prim.local_rect,
                                 prim.local_clip_rect,
                                 prim.z,
                                 prim.scroll_node,
                                 prim.task,
                                 prim.local_rect);
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

    vec2 uv0, uv1;

    if (image.sub_rect.x < 0.0) {
        uv0 = res.uv_rect.xy;
        uv1 = res.uv_rect.zw;
    } else {
        uv0 = res.uv_rect.xy + image.sub_rect.xy;
        uv1 = res.uv_rect.xy + image.sub_rect.zw;
    }

    // vUv will contain how many times this image has wrapped around the image size.
    vec2 st0 = uv0 / texture_size_normalization_factor;
    vec2 st1 = uv1 / texture_size_normalization_factor;

    vLayer = res.layer;
    vTextureSize = st1 - st0;
    vTextureOffset = st0;
    vTileSpacing = image.stretch_size_and_tile_spacing.zw;
    vStretchSize = image.stretch_size_and_tile_spacing.xy;

    // We clamp the texture coordinates to the half-pixel offset from the borders
    // in order to avoid sampling outside of the texture area.
    vec2 half_texel = vec2(0.5) / texture_size_normalization_factor;
    vStRect = vec4(min(st0, st1) + half_texel, max(st0, st1) - half_texel);
}
#endif

#ifdef WR_FRAGMENT_SHADER
void main(void) {
#ifdef WR_FEATURE_TRANSFORM
    float alpha = init_transform_fs(vLocalPos);

    // We clamp the texture coordinate calculation here to the local rectangle boundaries,
    // which makes the edge of the texture stretch instead of repeat.
    vec2 upper_bound_mask = step(vLocalRect.zw, vLocalPos);
    vec2 relative_pos_in_rect = clamp(vLocalPos, vLocalRect.xy, vLocalRect.zw) - vLocalRect.xy;
#else
    float alpha = 1.0;
    vec2 relative_pos_in_rect = vLocalPos;
    vec2 upper_bound_mask = vec2(0.0);
#endif

    alpha *= do_clip();

    // We calculate the particular tile this fragment belongs to, taking into
    // account the spacing in between tiles. We only paint if our fragment does
    // not fall into that spacing.
    // If the pixel is at the local rectangle upper bound, we force the current
    // tile upper bound in order to avoid wrapping.
    vec2 position_in_tile = mix(
        mod(relative_pos_in_rect, vStretchSize + vTileSpacing),
        vStretchSize,
        upper_bound_mask);
    vec2 st = vTextureOffset + ((position_in_tile / vStretchSize) * vTextureSize);
    st = clamp(st, vStRect.xy, vStRect.zw);

    alpha = alpha * float(all(bvec2(step(position_in_tile, vStretchSize))));

    oFragColor = vec4(alpha) * TEX_SAMPLE(sColor0, vec3(st, vLayer));
}
#endif
