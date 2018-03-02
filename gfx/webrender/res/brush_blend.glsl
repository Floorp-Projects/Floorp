/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define VECS_PER_SPECIFIC_BRUSH 1
#define FORCE_NO_PERSPECTIVE

#include shared,prim_shared,brush

varying vec3 vUv;

flat varying float vAmount;
flat varying int vOp;
flat varying mat4 vColorMat;
flat varying vec4 vColorOffset;
flat varying vec4 vUvClipBounds;

#ifdef WR_VERTEX_SHADER

void brush_vs(
    VertexInfo vi,
    int prim_address,
    RectWithSize local_rect,
    ivec3 user_data,
    PictureTask pic_task
) {
    PictureTask src_task = fetch_picture_task(user_data.x);
    vec2 texture_size = vec2(textureSize(sColor0, 0).xy);
    vec2 uv = vi.snapped_device_pos +
              src_task.common_data.task_rect.p0 -
              src_task.content_origin;
    vUv = vec3(uv / texture_size, src_task.common_data.texture_layer_index);

    vec2 uv0 = src_task.common_data.task_rect.p0;
    vec2 uv1 = uv0 + src_task.common_data.task_rect.size;
    vUvClipBounds = vec4(uv0, uv1) / texture_size.xyxy;

    vOp = user_data.y;

    float lumR = 0.2126;
    float lumG = 0.7152;
    float lumB = 0.0722;
    float oneMinusLumR = 1.0 - lumR;
    float oneMinusLumG = 1.0 - lumG;
    float oneMinusLumB = 1.0 - lumB;

    vec4 amount = fetch_from_resource_cache_1(prim_address);
    vAmount = amount.x;

    switch (vOp) {
        case 2: {
            // Grayscale
            vColorMat = mat4(vec4(lumR + oneMinusLumR * amount.y, lumR - lumR * amount.y, lumR - lumR * amount.y, 0.0),
                             vec4(lumG - lumG * amount.y, lumG + oneMinusLumG * amount.y, lumG - lumG * amount.y, 0.0),
                             vec4(lumB - lumB * amount.y, lumB - lumB * amount.y, lumB + oneMinusLumB * amount.y, 0.0),
                             vec4(0.0, 0.0, 0.0, 1.0));
            vColorOffset = vec4(0.0);
            break;
        }
        case 3: {
            // HueRotate
            float c = cos(amount.x);
            float s = sin(amount.x);
            vColorMat = mat4(vec4(lumR + oneMinusLumR * c - lumR * s, lumR - lumR * c + 0.143 * s, lumR - lumR * c - oneMinusLumR * s, 0.0),
                            vec4(lumG - lumG * c - lumG * s, lumG + oneMinusLumG * c + 0.140 * s, lumG - lumG * c + lumG * s, 0.0),
                            vec4(lumB - lumB * c + oneMinusLumB * s, lumB - lumB * c - 0.283 * s, lumB + oneMinusLumB * c + lumB * s, 0.0),
                            vec4(0.0, 0.0, 0.0, 1.0));
            vColorOffset = vec4(0.0);
            break;
        }
        case 5: {
            // Saturate
            vColorMat = mat4(vec4(amount.y * lumR + amount.x, amount.y * lumR, amount.y * lumR, 0.0),
                             vec4(amount.y * lumG, amount.y * lumG + amount.x, amount.y * lumG, 0.0),
                             vec4(amount.y * lumB, amount.y * lumB, amount.y * lumB + amount.x, 0.0),
                             vec4(0.0, 0.0, 0.0, 1.0));
            vColorOffset = vec4(0.0);
            break;
        }
        case 6: {
            // Sepia
            vColorMat = mat4(vec4(0.393 + 0.607 * amount.y, 0.349 - 0.349 * amount.y, 0.272 - 0.272 * amount.y, 0.0),
                             vec4(0.769 - 0.769 * amount.y, 0.686 + 0.314 * amount.y, 0.534 - 0.534 * amount.y, 0.0),
                             vec4(0.189 - 0.189 * amount.y, 0.168 - 0.168 * amount.y, 0.131 + 0.869 * amount.y, 0.0),
                             vec4(0.0, 0.0, 0.0, 1.0));
            vColorOffset = vec4(0.0);
            break;
        }
        case 10: {
            // Color Matrix
            vec4 mat_data[4] = fetch_from_resource_cache_4(user_data.z);
            vec4 offset_data = fetch_from_resource_cache_1(user_data.z + 4);
            vColorMat = mat4(mat_data[0], mat_data[1], mat_data[2], mat_data[3]);
            vColorOffset = offset_data;
            break;
        }
        default: break;
    }
}
#endif

#ifdef WR_FRAGMENT_SHADER
vec4 Contrast(vec4 Cs, float amount) {
    return vec4(Cs.rgb * amount - 0.5 * amount + 0.5, Cs.a);
}

vec4 Invert(vec4 Cs, float amount) {
    return vec4(mix(Cs.rgb, vec3(1.0) - Cs.rgb, amount), Cs.a);
}

vec4 Brightness(vec4 Cs, float amount) {
    // Apply the brightness factor.
    // Resulting color needs to be clamped to output range
    // since we are pre-multiplying alpha in the shader.
    return vec4(clamp(Cs.rgb * amount, vec3(0.0), vec3(1.0)), Cs.a);
}

vec4 Opacity(vec4 Cs, float amount) {
    return vec4(Cs.rgb, Cs.a * amount);
}

vec4 brush_fs() {
    vec4 Cs = texture(sColor0, vUv);

    // Un-premultiply the input.
    Cs.rgb /= Cs.a;

    vec4 color;

    switch (vOp) {
        case 0:
            color = Cs;
            break;
        case 1:
            color = Contrast(Cs, vAmount);
            break;
        case 4:
            color = Invert(Cs, vAmount);
            break;
        case 7:
            color = Brightness(Cs, vAmount);
            break;
        case 8:
            color = Opacity(Cs, vAmount);
            break;
        default:
            color = vColorMat * Cs + vColorOffset;
    }

    // Fail-safe to ensure that we don't sample outside the rendered
    // portion of a blend source.
    color.a *= point_inside_rect(vUv.xy, vUvClipBounds.xy, vUvClipBounds.zw);

    // Pre-multiply the alpha into the output value.
    color.rgb *= color.a;

    return color;
}
#endif
