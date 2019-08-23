/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define VECS_PER_SPECIFIC_BRUSH 3

#define COMPONENT_TRANSFER_IDENTITY 0
#define COMPONENT_TRANSFER_TABLE 1
#define COMPONENT_TRANSFER_DISCRETE 2
#define COMPONENT_TRANSFER_LINEAR 3
#define COMPONENT_TRANSFER_GAMMA 4

#include shared,prim_shared,brush

// Interpolated UV coordinates to sample.
varying vec2 vUv;
varying vec2 vLocalPos;

// X = layer index to sample, Y = flag to allow perspective interpolation of UV.
flat varying vec2 vLayerAndPerspective;
flat varying float vAmount;
flat varying int vOp;
flat varying mat3 vColorMat;
flat varying vec3 vColorOffset;
flat varying vec4 vUvClipBounds;
flat varying int vTableAddress;
flat varying int vFuncs[4];
flat varying vec4 vFloodColor;

#ifdef WR_VERTEX_SHADER

void brush_vs(
    VertexInfo vi,
    int prim_address,
    RectWithSize local_rect,
    RectWithSize segment_rect,
    ivec4 prim_user_data,
    int segment_user_data,
    mat4 transform,
    PictureTask pic_task,
    int brush_flags,
    vec4 unused
) {
    ImageResource res = fetch_image_resource(prim_user_data.x);
    vec2 uv0 = res.uv_rect.p0;
    vec2 uv1 = res.uv_rect.p1;

    vec2 texture_size = vec2(textureSize(sColor0, 0).xy);
    vec2 f = (vi.local_pos - local_rect.p0) / local_rect.size;
    f = get_image_quad_uv(prim_user_data.x, f);
    vec2 uv = mix(uv0, uv1, f);
    float perspective_interpolate = (brush_flags & BRUSH_FLAG_PERSPECTIVE_INTERPOLATION) != 0 ? 1.0 : 0.0;

    vUv = uv / texture_size * mix(vi.world_pos.w, 1.0, perspective_interpolate);
    vLayerAndPerspective = vec2(res.layer, perspective_interpolate);
    vUvClipBounds = vec4(uv0, uv1) / texture_size.xyxy;
    vLocalPos = vi.local_pos;

    float lumR = 0.2126;
    float lumG = 0.7152;
    float lumB = 0.0722;
    float oneMinusLumR = 1.0 - lumR;
    float oneMinusLumG = 1.0 - lumG;
    float oneMinusLumB = 1.0 - lumB;

    float amount = float(prim_user_data.z) / 65536.0;
    float invAmount = 1.0 - amount;

    vOp = prim_user_data.y & 0xffff;
    vAmount = amount;

    // This assignment is only used for component transfer filters but this
    // assignment has to be done here and not in the component transfer case
    // below because it doesn't get executed on Windows because of a suspected
    // miscompile of this shader on Windows. See
    // https://github.com/servo/webrender/wiki/Driver-issues#bug-1505871---assignment-to-varying-flat-arrays-inside-switch-statement-of-vertex-shader-suspected-miscompile-on-windows
    // default: just to satisfy angle_shader_validation.rs which needs one
    // default: for every switch, even in comments.
    vFuncs[0] = (prim_user_data.y >> 28) & 0xf; // R
    vFuncs[1] = (prim_user_data.y >> 24) & 0xf; // G
    vFuncs[2] = (prim_user_data.y >> 20) & 0xf; // B
    vFuncs[3] = (prim_user_data.y >> 16) & 0xf; // A

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
            vec4 mat_data[3] = fetch_from_gpu_cache_3(prim_user_data.z);
            vec4 offset_data = fetch_from_gpu_cache_1(prim_user_data.z + 4);
            vColorMat = mat3(mat_data[0].xyz, mat_data[1].xyz, mat_data[2].xyz);
            vColorOffset = offset_data.rgb;
            break;
        }
        case 13: {
            // Component Transfer
            vTableAddress = prim_user_data.z;
            break;
        }
        case 14: {
            // Flood
            vFloodColor = fetch_from_gpu_cache_1(prim_user_data.z);
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
    return if_then_else(lessThanEqual(color, vec3(0.04045)), c1, c2);
}

vec3 LinearToSrgb(vec3 color) {
    vec3 c1 = color * 12.92;
    vec3 c2 = vec3(1.055) * pow(color, vec3(1.0 / 2.4)) - vec3(0.055);
    return if_then_else(lessThanEqual(color, vec3(0.0031308)), c1, c2);
}

// This function has to be factored out due to the following issue:
// https://github.com/servo/webrender/wiki/Driver-issues#bug-1532245---switch-statement-inside-control-flow-inside-switch-statement-fails-to-compile-on-some-android-phones
// (and now the words "default: default:" so angle_shader_validation.rs passes)
vec4 ComponentTransfer(vec4 colora) {
    // We push a different amount of data to the gpu cache depending on the
    // function type.
    // Identity => 0 blocks
    // Table/Discrete => 64 blocks (256 values)
    // Linear => 1 block (2 values)
    // Gamma => 1 block (3 values)
    // We loop through the color components and increment the offset (for the
    // next color component) into the gpu cache based on how many blocks that
    // function type put into the gpu cache.
    // Table/Discrete use a 256 entry look up table.
    // Linear/Gamma are a simple calculation.
    int offset = 0;
    vec4 texel;
    int k;

    for (int i = 0; i < 4; i++) {
        switch (vFuncs[i]) {
            case COMPONENT_TRANSFER_IDENTITY:
                break;
            case COMPONENT_TRANSFER_TABLE:
            case COMPONENT_TRANSFER_DISCRETE: {
                // fetch value from lookup table
                k = int(floor(colora[i]*255.0));
                texel = fetch_from_gpu_cache_1(vTableAddress + offset + k/4);
                colora[i] = clamp(texel[k % 4], 0.0, 1.0);
                // offset plus 256/4 blocks
                offset = offset + 64;
                break;
            }
            case COMPONENT_TRANSFER_LINEAR: {
                // fetch the two values for use in the linear equation
                texel = fetch_from_gpu_cache_1(vTableAddress + offset);
                colora[i] = clamp(texel[0] * colora[i] + texel[1], 0.0, 1.0);
                // offset plus 1 block
                offset = offset + 1;
                break;
            }
            case COMPONENT_TRANSFER_GAMMA: {
                // fetch the three values for use in the gamma equation
                texel = fetch_from_gpu_cache_1(vTableAddress + offset);
                colora[i] = clamp(texel[0] * pow(colora[i], texel[1]) + texel[2], 0.0, 1.0);
                // offset plus 1 block
                offset = offset + 1;
                break;
            }
            default:
                // shouldn't happen
                break;
        }
    }
    return colora;
}

Fragment brush_fs() {
    float perspective_divisor = mix(gl_FragCoord.w, 1.0, vLayerAndPerspective.y);
    vec2 uv = vUv * perspective_divisor;
    vec4 Cs = texture(sColor0, vec3(uv, vLayerAndPerspective.x));

    // Un-premultiply the input.
    float alpha = Cs.a;
    vec3 color = alpha != 0.0 ? Cs.rgb / alpha : Cs.rgb;

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
        case 13: {
            // Component Transfer
            // Get the unpremultiplied color with alpha.
            vec4 colora = vec4(color, alpha);
            colora = ComponentTransfer(colora);
            color = colora.rgb;
            alpha = colora.a;
            break;
        }
        case 14: // Flood
            color = vFloodColor.rgb;
            alpha = vFloodColor.a;
            break;
        default:
            color = vColorMat * color + vColorOffset;
    }

    // Fail-safe to ensure that we don't sample outside the rendered
    // portion of a blend source.
    alpha *= min(point_inside_rect(uv, vUvClipBounds.xy, vUvClipBounds.zw),
                 init_transform_fs(vLocalPos));

    // Pre-multiply the alpha into the output value.
    return Fragment(alpha * vec4(color, 1.0));
}
#endif
