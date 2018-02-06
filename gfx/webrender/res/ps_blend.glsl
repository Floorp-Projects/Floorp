/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include shared,prim_shared

varying vec3 vUv;
flat varying vec4 vUvBounds;
flat varying float vAmount;
flat varying int vOp;
flat varying mat4 vColorMat;
flat varying vec4 vColorOffset;

#ifdef WR_VERTEX_SHADER
void main(void) {
    CompositeInstance ci = fetch_composite_instance();
    PictureTask dest_task = fetch_picture_task(ci.render_task_index);
    PictureTask src_task = fetch_picture_task(ci.src_task_index);

    vec2 dest_origin = dest_task.common_data.task_rect.p0 -
                       dest_task.content_origin +
                       src_task.content_origin;

    vec2 local_pos = mix(dest_origin,
                         dest_origin + src_task.common_data.task_rect.size,
                         aPosition.xy);

    vec2 texture_size = vec2(textureSize(sCacheRGBA8, 0));
    vec2 st0 = src_task.common_data.task_rect.p0;
    vec2 st1 = src_task.common_data.task_rect.p0 + src_task.common_data.task_rect.size;

    vec2 uv = src_task.common_data.task_rect.p0 + aPosition.xy * src_task.common_data.task_rect.size;
    vUv = vec3(uv / texture_size, src_task.common_data.texture_layer_index);
    vUvBounds = vec4(st0 + 0.5, st1 - 0.5) / texture_size.xyxy;

    vOp = ci.user_data0;
    vAmount = float(ci.user_data1) / 65535.0;

    float lumR = 0.2126;
    float lumG = 0.7152;
    float lumB = 0.0722;
    float oneMinusLumR = 1.0 - lumR;
    float oneMinusLumG = 1.0 - lumG;
    float oneMinusLumB = 1.0 - lumB;
    float oneMinusAmount = 1.0 - vAmount;

    switch (vOp) {
        case 2: {
            // Grayscale
            vColorMat = mat4(vec4(lumR + oneMinusLumR * oneMinusAmount, lumR - lumR * oneMinusAmount, lumR - lumR * oneMinusAmount, 0.0),
                             vec4(lumG - lumG * oneMinusAmount, lumG + oneMinusLumG * oneMinusAmount, lumG - lumG * oneMinusAmount, 0.0),
                             vec4(lumB - lumB * oneMinusAmount, lumB - lumB * oneMinusAmount, lumB + oneMinusLumB * oneMinusAmount, 0.0),
                             vec4(0.0, 0.0, 0.0, 1.0));
            vColorOffset = vec4(0.0);
            break;
        }
        case 3: {
            // HueRotate
            float c = cos(vAmount * 0.01745329251);
            float s = sin(vAmount * 0.01745329251);
            vColorMat = mat4(vec4(lumR + oneMinusLumR * c - lumR * s, lumR - lumR * c + 0.143 * s, lumR - lumR * c - oneMinusLumR * s, 0.0),
                            vec4(lumG - lumG * c - lumG * s, lumG + oneMinusLumG * c + 0.140 * s, lumG - lumG * c + lumG * s, 0.0),
                            vec4(lumB - lumB * c + oneMinusLumB * s, lumB - lumB * c - 0.283 * s, lumB + oneMinusLumB * c + lumB * s, 0.0),
                            vec4(0.0, 0.0, 0.0, 1.0));
            vColorOffset = vec4(0.0);
            break;
        }
        case 5: {
            // Saturate
            vColorMat = mat4(vec4(oneMinusAmount * lumR + vAmount, oneMinusAmount * lumR, oneMinusAmount * lumR, 0.0),
                             vec4(oneMinusAmount * lumG, oneMinusAmount * lumG + vAmount, oneMinusAmount * lumG, 0.0),
                             vec4(oneMinusAmount * lumB, oneMinusAmount * lumB, oneMinusAmount * lumB + vAmount, 0.0),
                             vec4(0.0, 0.0, 0.0, 1.0));
            vColorOffset = vec4(0.0);
            break;
        }
        case 6: {
            // Sepia
            vColorMat = mat4(vec4(0.393 + 0.607 * oneMinusAmount, 0.349 - 0.349 * oneMinusAmount, 0.272 - 0.272 * oneMinusAmount, 0.0),
                             vec4(0.769 - 0.769 * oneMinusAmount, 0.686 + 0.314 * oneMinusAmount, 0.534 - 0.534 * oneMinusAmount, 0.0),
                             vec4(0.189 - 0.189 * oneMinusAmount, 0.168 - 0.168 * oneMinusAmount, 0.131 + 0.869 * oneMinusAmount, 0.0),
                             vec4(0.0, 0.0, 0.0, 1.0));
            vColorOffset = vec4(0.0);
            break;
        }
        case 10: {
          // Color Matrix
          const int dataOffset = 2; // Offset in GPU cache where matrix starts
          vec4 data[8] = fetch_from_resource_cache_8(ci.user_data2);

          vColorMat = mat4(data[dataOffset], data[dataOffset + 1],
                           data[dataOffset + 2], data[dataOffset + 3]);
          vColorOffset = data[dataOffset + 4];
          break;
        }
    }


    gl_Position = uTransform * vec4(local_pos, ci.z, 1.0);
}
#endif

#ifdef WR_FRAGMENT_SHADER
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

void main(void) {
    vec2 uv = clamp(vUv.xy, vUvBounds.xy, vUvBounds.zw);
    vec4 Cs = textureLod(sCacheRGBA8, vec3(uv, vUv.z), 0.0);

    if (Cs.a == 0.0) {
        discard;
    }

    // Un-premultiply the input.
    Cs.rgb /= Cs.a;

    switch (vOp) {
        case 0:
            oFragColor = Cs;
            break;
        case 1:
            oFragColor = Contrast(Cs, vAmount);
            break;
        case 4:
            oFragColor = Invert(Cs, vAmount);
            break;
        case 7:
            oFragColor = Brightness(Cs, vAmount);
            break;
        case 8:
            oFragColor = Opacity(Cs, vAmount);
            break;
        default:
            oFragColor = vColorMat * Cs + vColorOffset;
    }

    // Pre-multiply the alpha into the output value.
    oFragColor.rgb *= oFragColor.a;
}
#endif
