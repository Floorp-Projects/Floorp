#line 1
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define RENDER_MODE_MONO        0
#define RENDER_MODE_ALPHA       1
#define RENDER_MODE_SUBPIXEL    2

void main(void) {
    Primitive prim = load_primitive();
    TextRun text = fetch_text_run(prim.specific_prim_address);

    int glyph_index = prim.user_data0;
    int render_mode = prim.user_data1;
    int resource_address = prim.user_data2;

    Glyph glyph = fetch_glyph(prim.specific_prim_address, glyph_index);
    GlyphResource res = fetch_glyph_resource(resource_address);

    switch (render_mode) {
        case RENDER_MODE_ALPHA:
            break;
        case RENDER_MODE_MONO:
            break;
        case RENDER_MODE_SUBPIXEL:
            // In subpixel mode, the subpixel offset has already been
            // accounted for while rasterizing the glyph.
            glyph.offset = trunc(glyph.offset);
            break;
    }

    vec2 local_pos = glyph.offset +
                     text.offset +
                     vec2(res.offset.x, -res.offset.y) / uDevicePixelRatio;

    RectWithSize local_rect = RectWithSize(local_pos,
                                           (res.uv_rect.zw - res.uv_rect.xy) / uDevicePixelRatio);

#ifdef WR_FEATURE_TRANSFORM
    TransformVertexInfo vi = write_transform_vertex(local_rect,
                                                    prim.local_clip_rect,
                                                    prim.z,
                                                    prim.layer,
                                                    prim.task,
                                                    local_rect);
    vLocalPos = vi.local_pos;
    vec2 f = (vi.local_pos.xy / vi.local_pos.z - local_rect.p0) / local_rect.size;
#else
    VertexInfo vi = write_vertex(local_rect,
                                 prim.local_clip_rect,
                                 prim.z,
                                 prim.layer,
                                 prim.task,
                                 local_rect);
    vec2 f = (vi.local_pos - local_rect.p0) / local_rect.size;
#endif

    write_clip(vi.screen_pos, prim.clip_area);

    vec2 texture_size = vec2(textureSize(sColor0, 0));
    vec2 st0 = res.uv_rect.xy / texture_size;
    vec2 st1 = res.uv_rect.zw / texture_size;

    vColor = text.color;
    vUv = mix(st0, st1, f);
    vUvBorder = (res.uv_rect + vec4(0.5, 0.5, -0.5, -0.5)) / texture_size.xyxy;
}
