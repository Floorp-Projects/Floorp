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
    vec2 relative_pos_in_rect =
         clamp(pos, vLocalRect.xy, vLocalRect.xy + vLocalRect.zw) - vLocalRect.xy;
#else
    float alpha = 1.0;;
    vec2 relative_pos_in_rect = vLocalPos;
#endif

    alpha = min(alpha, do_clip());

    vec2 st_y = vTextureOffsetY + relative_pos_in_rect / vStretchSize * vTextureSizeY;
    vec2 st_u = vTextureOffsetU + relative_pos_in_rect / vStretchSize * vTextureSizeUv;
    vec2 st_v = vTextureOffsetV + relative_pos_in_rect / vStretchSize * vTextureSizeUv;

    float y = texture(sColor0, st_y).r;
    float u = texture(sColor1, st_u).r;
    float v = texture(sColor2, st_v).r;

    // See the vertex shader for an explanation of where the constants come from.
    vec3 rgb = vYuvColorMatrix * vec3(y - 0.06275, u - 0.50196, v - 0.50196);
    oFragColor = vec4(rgb, alpha);
}
