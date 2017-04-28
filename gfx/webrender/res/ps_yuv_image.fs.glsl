#line 1
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(WR_FEATURE_YUV_REC601) && !defined(WR_FEATURE_YUV_REC709)
#define WR_FEATURE_YUV_REC601
#endif

// The constants added to the Y, U and V components are applied in the fragment shader.
#if defined(WR_FEATURE_YUV_REC601)
// From Rec601:
// [R]   [1.1643835616438356,  0.0,                 1.5960267857142858   ]   [Y -  16]
// [G] = [1.1643835616438358, -0.3917622900949137, -0.8129676472377708   ] x [U - 128]
// [B]   [1.1643835616438356,  2.017232142857143,   8.862867620416422e-17]   [V - 128]
//
// For the range [0,1] instead of [0,255].
const mat3 YuvColorMatrix = mat3(
    1.16438,  0.0,      1.59603,
    1.16438, -0.39176, -0.81297,
    1.16438,  2.01723,  0.0
);
#elif defined(WR_FEATURE_YUV_REC709)
// From Rec709:
// [R]   [1.1643835616438356,  4.2781193979771426e-17, 1.7927410714285714]   [Y -  16]
// [G] = [1.1643835616438358, -0.21324861427372963,   -0.532909328559444 ] x [U - 128]
// [B]   [1.1643835616438356,  2.1124017857142854,     0.0               ]   [V - 128]
//
// For the range [0,1] instead of [0,255]:
const mat3 YuvColorMatrix = mat3(
    1.16438,  0.0,      1.79274,
    1.16438, -0.21325, -0.53291,
    1.16438,  2.11240,  0.0
);
#endif

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
    // NV12 only uses 2 textures. The sColor0 is for y and sColor1 is for uv.
    // The texture coordinates of u and v are the same. So, we could skip the
    // st_v if the format is NV12.
    vec2 st_u = vTextureOffsetU + uv_offset;

    vec3 yuv_value;
#ifdef WR_FEATURE_NV12
    yuv_value.x = TEX_SAMPLE(sColor0, st_y).r;
    yuv_value.yz = TEX_SAMPLE(sColor1, st_u).rg;
#else
    // The yuv_planar format should have this third texture coordinate.
    vec2 st_v = vTextureOffsetV + uv_offset;

    yuv_value.x = TEX_SAMPLE(sColor0, st_y).r;
    yuv_value.y = TEX_SAMPLE(sColor1, st_u).r;
    yuv_value.z = TEX_SAMPLE(sColor2, st_v).r;
#endif

    // See the YuvColorMatrix definition for an explanation of where the constants come from.
    vec3 rgb = YuvColorMatrix * (yuv_value - vec3(0.06275, 0.50196, 0.50196));
    oFragColor = vec4(rgb, alpha);
}
