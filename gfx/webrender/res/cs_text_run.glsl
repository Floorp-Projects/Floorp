/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include shared,prim_shared

varying vec3 vUv;
flat varying vec4 vColor;

#ifdef WR_VERTEX_SHADER
// Draw a text run to a cache target. These are always
// drawn un-transformed. These are used for effects such
// as text-shadow.

void main(void) {
    Primitive prim = load_primitive();
    TextRun text = fetch_text_run(prim.specific_prim_address);

    int glyph_index = prim.user_data0;
    int resource_address = prim.user_data1;
    int picture_address = prim.user_data2;

    // Fetch the owning picture for this primitive. This allows the code
    // below to normalize the glyph offsets relative to the original text
    // shadow rect, which is the union of all elements that make up this
    // text shadow. This allows the text shadow to be rendered at an
    // arbitrary location in a render target (provided by the render
    // task render_target_origin field).
    PrimitiveGeometry shadow_geom = fetch_primitive_geometry(picture_address);
    Picture pic = fetch_picture(picture_address + VECS_PER_PRIM_HEADER);

    Glyph glyph = fetch_glyph(prim.specific_prim_address,
                              glyph_index,
                              text.subpx_dir);

    GlyphResource res = fetch_glyph_resource(resource_address);

    // Glyphs size is already in device-pixels.
    // The render task origin is in device-pixels. Offset that by
    // the glyph offset, relative to its primitive bounding rect.
    vec2 size = (res.uv_rect.zw - res.uv_rect.xy) * res.scale;
    vec2 local_pos = glyph.offset + vec2(res.offset.x, -res.offset.y) / uDevicePixelRatio;
    vec2 origin = prim.task.render_target_origin +
                  uDevicePixelRatio * (local_pos + pic.offset - shadow_geom.local_rect.p0);
    vec4 local_rect = vec4(origin, size);

    vec2 texture_size = vec2(textureSize(sColor0, 0));
    vec2 st0 = res.uv_rect.xy / texture_size;
    vec2 st1 = res.uv_rect.zw / texture_size;

    vec2 pos = mix(local_rect.xy,
                   local_rect.xy + local_rect.zw,
                   aPosition.xy);

    vUv = vec3(mix(st0, st1, aPosition.xy), res.layer);
    vColor = pic.color;

    gl_Position = uTransform * vec4(pos, 0.0, 1.0);
}
#endif

#ifdef WR_FRAGMENT_SHADER
void main(void) {
    float a = texture(sColor0, vUv).a;
    oFragColor = vec4(vColor.rgb, vColor.a * a);
}
#endif
