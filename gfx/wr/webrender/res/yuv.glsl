/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include shared

#define YUV_FORMAT_NV12             0
#define YUV_FORMAT_P010             1
#define YUV_FORMAT_PLANAR           2
#define YUV_FORMAT_INTERLEAVED      3

//#define YUV_PRECISION mediump
#define YUV_PRECISION highp

#ifdef WR_VERTEX_SHADER

#ifdef WR_FEATURE_TEXTURE_RECT
    #define TEX_SIZE_YUV(sampler) vec2(1.0)
#else
    #define TEX_SIZE_YUV(sampler) vec2(TEX_SIZE(sampler).xy)
#endif

// `YuvRangedColorSpace`
#define YUV_COLOR_SPACE_REC601_NARROW  0
#define YUV_COLOR_SPACE_REC601_FULL    1
#define YUV_COLOR_SPACE_REC709_NARROW  2
#define YUV_COLOR_SPACE_REC709_FULL    3
#define YUV_COLOR_SPACE_REC2020_NARROW 4
#define YUV_COLOR_SPACE_REC2020_FULL   5
#define YUV_COLOR_SPACE_GBR_IDENTITY   6

// The constants added to the Y, U and V components are applied in the fragment shader.

// `rgbFromYuv` from https://jdashg.github.io/misc/colors/from-coeffs.html
// The matrix is stored in column-major.
const mat3 RgbFromYuv_Rec601 = mat3(
  1.00000, 1.00000, 1.00000,
  0.00000,-0.17207, 0.88600,
  0.70100,-0.35707, 0.00000
);
const mat3 RgbFromYuv_Rec709 = mat3(
  1.00000, 1.00000, 1.00000,
  0.00000,-0.09366, 0.92780,
  0.78740,-0.23406, 0.00000
);
const mat3 RgbFromYuv_Rec2020 = mat3(
  1.00000, 1.00000, 1.00000,
  0.00000,-0.08228, 0.94070,
  0.73730,-0.28568, 0.00000
);

// The matrix is stored in column-major.
// Identity is stored as GBR
const mat3 RgbFromYuv_GbrIdentity = mat3(
    0.0              ,  1.0,                0.0,
    0.0              ,  0.0,                1.0,
    1.0              ,  0.0,                0.0
);

// -

struct YuvPrimitive {
    int channel_bit_depth;
    int color_space;
    int yuv_format;
};

struct YuvColorSamplingInfo {
    mat3 rgb_from_yuv;
    vec4 packed_zero_one_vals;
};

struct YuvColorMatrixInfo {
    vec3 ycbcr_bias;
    mat3 rgb_from_debiased_ycbrc;
};

// -

vec4 yuv_channel_zero_one_identity(int bit_depth, float channel_max) {
    float all_ones_normalized = float((1 << bit_depth) - 1) / channel_max;
    return vec4(0.0, 0.0, all_ones_normalized, all_ones_normalized);
}

vec4 yuv_channel_zero_one_narrow_range(int bit_depth, float channel_max) {
    // Note: 512/1023 != 128/255
    ivec4 zero_one_ints = ivec4(16, 128, 235, 240) << (bit_depth - 8);
    return vec4(zero_one_ints) / channel_max;
}

vec4 yuv_channel_zero_one_full_range(int bit_depth, float channel_max) {
    vec4 narrow = yuv_channel_zero_one_narrow_range(bit_depth, channel_max);
    vec4 identity = yuv_channel_zero_one_identity(bit_depth, channel_max);
    return vec4(0.0, narrow.y, identity.z, identity.w);
}

YuvColorSamplingInfo get_yuv_color_info(YuvPrimitive prim) {
    float channel_max = 255.0;
    if (prim.channel_bit_depth > 8) {
        if (prim.yuv_format == YUV_FORMAT_P010) {
            // This is an MSB format.
            channel_max = float((1 << prim.channel_bit_depth) - 1);
        } else {
            // For >8bpc, we get the low bits, not the high bits:
            // 10bpc(1.0): 0b0000_0011_1111_1111
            channel_max = 65535.0;
        }
    }
    if (prim.color_space == YUV_COLOR_SPACE_REC601_NARROW) {
        return YuvColorSamplingInfo(RgbFromYuv_Rec601,
                yuv_channel_zero_one_narrow_range(prim.channel_bit_depth, channel_max));
    } else if (prim.color_space == YUV_COLOR_SPACE_REC601_FULL) {
        return YuvColorSamplingInfo(RgbFromYuv_Rec601,
                yuv_channel_zero_one_full_range(prim.channel_bit_depth, channel_max));

    } else if (prim.color_space == YUV_COLOR_SPACE_REC709_NARROW) {
        return YuvColorSamplingInfo(RgbFromYuv_Rec709,
                yuv_channel_zero_one_narrow_range(prim.channel_bit_depth, channel_max));
    } else if (prim.color_space == YUV_COLOR_SPACE_REC709_FULL) {
        return YuvColorSamplingInfo(RgbFromYuv_Rec709,
                yuv_channel_zero_one_full_range(prim.channel_bit_depth, channel_max));

    } else if (prim.color_space == YUV_COLOR_SPACE_REC2020_NARROW) {
        return YuvColorSamplingInfo(RgbFromYuv_Rec2020,
                yuv_channel_zero_one_narrow_range(prim.channel_bit_depth, channel_max));
    } else if (prim.color_space == YUV_COLOR_SPACE_REC2020_FULL) {
        return YuvColorSamplingInfo(RgbFromYuv_Rec2020,
                yuv_channel_zero_one_full_range(prim.channel_bit_depth, channel_max));

    } else {
        // Identity
        return YuvColorSamplingInfo(RgbFromYuv_GbrIdentity,
                yuv_channel_zero_one_identity(prim.channel_bit_depth, channel_max));
    }
}

YuvColorMatrixInfo get_rgb_from_ycbcr_info(YuvPrimitive prim) {
    YuvColorSamplingInfo info = get_yuv_color_info(prim);

    vec2 zero = info.packed_zero_one_vals.xy;
    vec2 one = info.packed_zero_one_vals.zw;
    // Such that yuv_value = (ycbcr_sample - zero) / (one - zero)
    vec2 scale = 1.0 / (one - zero);

    YuvColorMatrixInfo mat_info;
    mat_info.ycbcr_bias = zero.xyy;
    mat3 yuv_from_debiased_ycbcr = mat3(scale.x,     0.0,     0.0,
                                            0.0, scale.y,     0.0,
                                            0.0,     0.0, scale.y);
    mat_info.rgb_from_debiased_ycbrc = info.rgb_from_yuv * yuv_from_debiased_ycbcr;
    return mat_info;
}

void write_uv_rect(
    vec2 uv0,
    vec2 uv1,
    vec2 f,
    vec2 texture_size,
    out vec2 uv,
    out vec4 uv_bounds
) {
    uv = mix(uv0, uv1, f);

    uv_bounds = vec4(uv0 + vec2(0.5), uv1 - vec2(0.5));

    #ifndef WR_FEATURE_TEXTURE_RECT
        uv /= texture_size;
        uv_bounds /= texture_size.xyxy;
    #endif
}
#endif

#ifdef WR_FRAGMENT_SHADER

vec4 sample_yuv(
    int format,
    YUV_PRECISION vec3 ycbcr_bias,
    YUV_PRECISION mat3 rgb_from_debiased_ycbrc,
    vec2 in_uv_y,
    vec2 in_uv_u,
    vec2 in_uv_v,
    vec4 uv_bounds_y,
    vec4 uv_bounds_u,
    vec4 uv_bounds_v
) {
    YUV_PRECISION vec3 ycbcr_sample;

    switch (format) {
        case YUV_FORMAT_PLANAR:
            {
                // The yuv_planar format should have this third texture coordinate.
                vec2 uv_y = clamp(in_uv_y, uv_bounds_y.xy, uv_bounds_y.zw);
                vec2 uv_u = clamp(in_uv_u, uv_bounds_u.xy, uv_bounds_u.zw);
                vec2 uv_v = clamp(in_uv_v, uv_bounds_v.xy, uv_bounds_v.zw);
                ycbcr_sample.x = TEX_SAMPLE(sColor0, uv_y).r;
                ycbcr_sample.y = TEX_SAMPLE(sColor1, uv_u).r;
                ycbcr_sample.z = TEX_SAMPLE(sColor2, uv_v).r;
            }
            break;

        case YUV_FORMAT_NV12:
        case YUV_FORMAT_P010:
            {
                vec2 uv_y = clamp(in_uv_y, uv_bounds_y.xy, uv_bounds_y.zw);
                vec2 uv_uv = clamp(in_uv_u, uv_bounds_u.xy, uv_bounds_u.zw);
                ycbcr_sample.x = TEX_SAMPLE(sColor0, uv_y).r;
                ycbcr_sample.yz = TEX_SAMPLE(sColor1, uv_uv).rg;
            }
            break;

        case YUV_FORMAT_INTERLEAVED:
            {
                // "The Y, Cb and Cr color channels within the 422 data are mapped into
                // the existing green, blue and red color channels."
                // https://www.khronos.org/registry/OpenGL/extensions/APPLE/APPLE_rgb_422.txt
                vec2 uv_y = clamp(in_uv_y, uv_bounds_y.xy, uv_bounds_y.zw);
                ycbcr_sample = TEX_SAMPLE(sColor0, uv_y).gbr;
            }
            break;

        default:
            ycbcr_sample = vec3(0.0);
            break;
    }
    //if (true) return vec4(ycbcr_sample, 1.0);

    // See the YuvColorMatrix definition for an explanation of where the constants come from.
    YUV_PRECISION vec3 rgb = rgb_from_debiased_ycbrc * (ycbcr_sample - ycbcr_bias);

    #if defined(WR_FEATURE_ALPHA_PASS) && defined(SWGL_CLIP_MASK)
        // Avoid out-of-range RGB values that can mess with blending. These occur due to invalid
        // YUV values outside the mappable space that never the less can be generated.
        rgb = clamp(rgb, 0.0, 1.0);
    #endif
    return vec4(rgb, 1.0);
}
#endif
