#line 1
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

void main(void) {
#ifdef WR_FEATURE_TRANSFORM
    float alpha = 0.0;
    vec2 pos = init_transform_fs(vLocalPos, vLocalRect, alpha);

    // We clamp the texture coordinate calculation here to the local rectangle boundaries,
    // which makes the edge of the texture stretch instead of repeat.
    vec2 relative_pos_in_rect = clamp_rect(pos, vLocalRect) - vLocalRect.p0;
#else
    float alpha = 1.0;;
    vec2 relative_pos_in_rect = vLocalPos;
#endif

    alpha = min(alpha, do_clip());

    // We clamp the texture coordinates to the half-pixel offset from the borders
    // in order to avoid sampling outside of the texture area.
    vec2 st_y = vTextureOffsetY + clamp(
        relative_pos_in_rect / vStretchSize * vTextureSizeY,
        vHalfTexelY, vTextureSizeY - vHalfTexelY);
    vec2 uv_offset = clamp(
        relative_pos_in_rect / vStretchSize * vTextureSizeUv,
        vHalfTexelUv, vTextureSizeUv - vHalfTexelUv);
    vec2 st_u = vTextureOffsetU + uv_offset;
    vec2 st_v = vTextureOffsetV + uv_offset;

    float y = textureLod(sColor0, st_y, 0.0).r;
    float u = textureLod(sColor1, st_u, 0.0).r;
    float v = textureLod(sColor2, st_v, 0.0).r;

    // See the vertex shader for an explanation of where the constants come from.
    vec3 rgb = vYuvColorMatrix * vec3(y - 0.06275, u - 0.50196, v - 0.50196);
    oFragColor = vec4(rgb, alpha);
}
