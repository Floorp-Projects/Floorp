/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include shared,prim_shared

varying vec3 vUv;
flat varying vec4 vColor;
flat varying vec4 vStRect;

#ifdef WR_VERTEX_SHADER
// Draw a text run to a cache target. These are always
// drawn un-transformed. These are used for effects such
// as text-shadow.

void main(void) {
    Primitive prim = load_primitive();
    TextRun text = fetch_text_run(prim.specific_prim_address);

    int glyph_index = prim.user_data0;
    int resource_address = prim.user_data1;
    int subpx_dir = prim.user_data2;

    Glyph glyph = fetch_glyph(prim.specific_prim_address,
                              glyph_index,
                              subpx_dir);

    GlyphResource res = fetch_glyph_resource(resource_address);

    // Scale from glyph space to local space.
    float scale = res.scale / uDevicePixelRatio;

    // Compute the glyph rect in local space.
    RectWithSize glyph_rect = RectWithSize(scale * res.offset + text.offset + glyph.offset,
                                           scale * (res.uv_rect.zw - res.uv_rect.xy));

    // Select the corner of the glyph rect that we are processing.
    vec2 local_pos = (glyph_rect.p0 + glyph_rect.size * aPosition.xy);

    // Clamp the local position to the text run's local clipping rectangle.
    local_pos = clamp_rect(local_pos, prim.local_clip_rect);

    // Move the point into device pixel space.
    local_pos = (local_pos - prim.task.content_origin) * uDevicePixelRatio;
    local_pos += prim.task.common_data.task_rect.p0;
    gl_Position = uTransform * vec4(local_pos, 0.0, 1.0);

    vec2 texture_size = vec2(textureSize(sColor0, 0));
    vec2 st0 = res.uv_rect.xy / texture_size;
    vec2 st1 = res.uv_rect.zw / texture_size;

    vUv = vec3(mix(st0, st1, aPosition.xy), res.layer);
    vColor = prim.task.color;

    // We clamp the texture coordinates to the half-pixel offset from the borders
    // in order to avoid sampling outside of the texture area.
    vec2 half_texel = vec2(0.5) / texture_size;
    vStRect = vec4(min(st0, st1) + half_texel, max(st0, st1) - half_texel);
}
#endif

#ifdef WR_FRAGMENT_SHADER
void main(void) {
    vec2 uv = clamp(vUv.xy, vStRect.xy, vStRect.zw);
    float a = texture(sColor0, vec3(uv, vUv.z)).a;
    oFragColor = vColor * a;
}
#endif
