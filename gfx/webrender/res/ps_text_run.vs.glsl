#line 1
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

void main(void) {
    Primitive prim = load_primitive();
    TextRun text = fetch_text_run(prim.prim_index);
    Glyph glyph = fetch_glyph(prim.user_data0);
    ResourceRect res = fetch_resource_rect(prim.user_data1);

    RectWithSize local_rect = RectWithSize(glyph.offset.xy,
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
