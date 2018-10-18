/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define VECS_PER_SPECIFIC_BRUSH 3

#include shared,prim_shared,brush

varying vec3 vUv;

flat varying float vAmount;
flat varying int vOp;
flat varying mat3 vColorMat;
flat varying vec3 vColorOffset;
flat varying vec4 vUvClipBounds;

#ifdef WR_VERTEX_SHADER

void brush_vs(
    VertexInfo vi,
    int prim_address,
    RectWithSize local_rect,
    RectWithSize segment_rect,
    ivec4 user_data,
    mat4 transform,
    PictureTask pic_task,
    int brush_flags,
    vec4 unused
) {
    ImageResource res = fetch_image_resource(user_data.x);
    vec2 uv0 = res.uv_rect.p0;
    vec2 uv1 = res.uv_rect.p1;

    // PictureTask src_task = fetch_picture_task(user_data.x);
    vec2 texture_size = vec2(textureSize(sColor0, 0).xy);
    vec2 f = (vi.local_pos - local_rect.p0) / local_rect.size;
    ImageResourceExtra extra_data = fetch_image_resource_extra(user_data.x);
    vec2 x = mix(extra_data.st_tl, extra_data.st_tr, f.x);
    vec2 y = mix(extra_data.st_bl, extra_data.st_br, f.x);
    f = mix(x, y, f.y);
    vec2 uv = mix(uv0, uv1, f);
    vUv = vec3(uv / texture_size, res.layer);

    vUvClipBounds = vec4(uv0, uv1) / texture_size.xyxy;

    float lumR = 0.2126;
    float lumG = 0.7152;
    float lumB = 0.0722;
    float oneMinusLumR = 1.0 - lumR;
    float oneMinusLumG = 1.0 - lumG;
    float oneMinusLumB = 1.0 - lumB;

    float amount = float(user_data.z) / 65536.0;
    float invAmount = 1.0 - amount;

    vOp = user_data.y;
    vAmount = amount;

    switch (vOp) {
        case 2: {
            // Grayscale
            vColorMat = mat3(
                vec3(lumR + oneMinusLumR * invAmount, lumR - lumR * invAmount, lumR - lumR * invAmount),
                vec3(lumG - lumG * invAmount, lumG + oneMinusLumG * invAmount, lumG - lumG * invAmount),
                vec3(lumB - lumB * invAmount, lumB - lumB * invAmount, lumB + oneMinusLumB * invAmount)
            );
            vColorOffset = vec3(0.0);
            break;
        }
        case 3: {
            // HueRotate
            float c = cos(amount);
            float s = sin(amount);
            vColorMat = mat3(
                vec3(lumR + oneMinusLumR * c - lumR * s, lumR - lumR * c + 0.143 * s, lumR - lumR * c - oneMinusLumR * s),
                vec3(lumG - lumG * c - lumG * s, lumG + oneMinusLumG * c + 0.140 * s, lumG - lumG * c + lumG * s),
                vec3(lumB - lumB * c + oneMinusLumB * s, lumB - lumB * c - 0.283 * s, lumB + oneMinusLumB * c + lumB * s)
            );
            vColorOffset = vec3(0.0);
            break;
        }
        case 5: {
            // Saturate
            vColorMat = mat3(
                vec3(invAmount * lumR + amount, invAmount * lumR, invAmount * lumR),
                vec3(invAmount * lumG, invAmount * lumG + amount, invAmount * lumG),
                vec3(invAmount * lumB, invAmount * lumB, invAmount * lumB + amount)
            );
            vColorOffset = vec3(0.0);
            break;
        }
        case 6: {
            // Sepia
            vColorMat = mat3(
                vec3(0.393 + 0.607 * invAmount, 0.349 - 0.349 * invAmount, 0.272 - 0.272 * invAmount),
                vec3(0.769 - 0.769 * invAmount, 0.686 + 0.314 * invAmount, 0.534 - 0.534 * invAmount),
                vec3(0.189 - 0.189 * invAmount, 0.168 - 0.168 * invAmount, 0.131 + 0.869 * invAmount)
            );
            vColorOffset = vec3(0.0);
            break;
        }
        case 10: {
            // Color Matrix
            vec4 mat_data[3] = fetch_from_gpu_cache_3(user_data.z);
            vec4 offset_data = fetch_from_gpu_cache_1(user_data.z + 4);
            vColorMat = mat3(mat_data[0].xyz, mat_data[1].xyz, mat_data[2].xyz);
            vColorOffset = offset_data.rgb;
            break;
        }
        default: break;
    }
}
#endif

#ifdef WR_FRAGMENT_SHADER
vec3 Contrast(vec3 Cs, float amount) {
    return Cs.rgb * amount - 0.5 * amount + 0.5;
}

vec3 Invert(vec3 Cs, float amount) {
    return mix(Cs.rgb, vec3(1.0) - Cs.rgb, amount);
}

vec3 Brightness(vec3 Cs, float amount) {
    // Apply the brightness factor.
    // Resulting color needs to be clamped to output range
    // since we are pre-multiplying alpha in the shader.
    return clamp(Cs.rgb * amount, vec3(0.0), vec3(1.0));
}

// Based on the Gecko's implementation in
// https://hg.mozilla.org/mozilla-central/file/91b4c3687d75/gfx/src/FilterSupport.cpp#l24
// These could be made faster by sampling a lookup table stored in a float texture
// with linear interpolation.

vec3 SrgbToLinear(vec3 color) {
    vec3 c1 = color / 12.92;
    vec3 c2 = pow(color / 1.055 + vec3(0.055 / 1.055), vec3(2.4));
    return mix(c1, c2, lessThanEqual(color, vec3(0.04045)));
}

vec3 LinearToSrgb(vec3 color) {
    vec3 c1 = color * 12.92;
    vec3 c2 = vec3(1.055) * pow(color, vec3(1.0 / 2.4)) - vec3(0.055);
    return mix(c1, c2, lessThanEqual(color, vec3(0.0031308)));
}

Fragment brush_fs() {
    vec4 Cs = texture(sColor0, vUv);

    if (Cs.a == 0.0) {
        return Fragment(vec4(0.0)); // could also `discard`
    }

    // Un-premultiply the input.
    float alpha = Cs.a;
    vec3 color = Cs.rgb / Cs.a;

    switch (vOp) {
        case 0:
            break;
        case 1:
            color = Contrast(color, vAmount);
            break;
        case 4:
            color = Invert(color, vAmount);
            break;
        case 7:
            color = Brightness(color, vAmount);
            break;
        case 8: // Opacity
            alpha *= vAmount;
            break;
        case 11:
            color = SrgbToLinear(color);
            break;
        case 12:
            color = LinearToSrgb(color);
            break;
        default:
            color = vColorMat * color + vColorOffset;
    }

    // Fail-safe to ensure that we don't sample outside the rendered
    // portion of a blend source.
    alpha *= point_inside_rect(vUv.xy, vUvClipBounds.xy, vUvClipBounds.zw);

    // Pre-multiply the alpha into the output value.
    return Fragment(alpha * vec4(color, 1.0));
}
#endif
