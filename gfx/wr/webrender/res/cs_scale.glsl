/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include shared,prim_shared

varying vec2 vUv;
flat varying float vUvLayer;
flat varying vec4 vUvRect;

#ifdef WR_VERTEX_SHADER

PER_INSTANCE in vec4 aScaleTargetRect;
PER_INSTANCE in ivec4 aScaleSourceRect;
PER_INSTANCE in int aScaleSourceLayer;

void main(void) {
    RectWithSize src_rect = RectWithSize(vec2(aScaleSourceRect.xy), vec2(aScaleSourceRect.zw));

    // If this is in WR_FEATURE_TEXTURE_RECT mode, the rect and size use
    // non-normalized texture coordinates.
#ifdef WR_FEATURE_TEXTURE_RECT
    vec2 texture_size = vec2(1, 1);
#else
    vec2 texture_size = vec2(textureSize(sColor0, 0));
#endif

    vUvLayer = float(aScaleSourceLayer);

    vUvRect = vec4(src_rect.p0 + vec2(0.5),
                   src_rect.p0 + src_rect.size - vec2(0.5)) / texture_size.xyxy;

    vec2 pos = aScaleTargetRect.xy + aScaleTargetRect.zw * aPosition.xy;
    vUv = (src_rect.p0 + src_rect.size * aPosition.xy) / texture_size;

    gl_Position = uTransform * vec4(pos, 0.0, 1.0);
}

#endif

#ifdef WR_FRAGMENT_SHADER

void main(void) {
    vec2 st = clamp(vUv, vUvRect.xy, vUvRect.zw);
    oFragColor = TEX_SAMPLE(sColor0, vec3(st, vUvLayer));
}

#ifdef SWGL
void swgl_drawSpanRGBA8() {
    if (!swgl_isTextureRGBA8(sColor0)) {
        return;
    }

    int layer = swgl_textureLayerOffset(sColor0, vUvLayer);
    vec2 uv = swgl_linearQuantize(sColor0, vUv);
    vec2 min_uv = swgl_linearQuantize(sColor0, vUvRect.xy);
    vec2 max_uv = swgl_linearQuantize(sColor0, vUvRect.zw);
    vec2 step_uv = swgl_linearQuantizeStep(sColor0, swgl_interpStep(vUv));

    while (swgl_SpanLength > 0) {
        swgl_commitTextureLinearRGBA8(sColor0, clamp(uv, min_uv, max_uv), layer);
        uv += step_uv;
    }
}
#endif

#endif
