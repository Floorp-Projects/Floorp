/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include shared,prim_shared

varying vec4 vColor;

#ifdef WR_FEATURE_TRANSFORM
varying vec3 vLocalPos;
#endif

#ifdef WR_VERTEX_SHADER
void main(void) {
    Primitive prim = load_primitive();
    Rectangle rect = fetch_rectangle(prim.specific_prim_address);
    vColor = rect.color;
#ifdef WR_FEATURE_TRANSFORM
    TransformVertexInfo vi = write_transform_vertex(prim.local_rect,
                                                    prim.local_clip_rect,
                                                    prim.z,
                                                    prim.layer,
                                                    prim.task,
                                                    prim.local_rect);
    vLocalPos = vi.local_pos;
#else
    VertexInfo vi = write_vertex(prim.local_rect,
                                 prim.local_clip_rect,
                                 prim.z,
                                 prim.layer,
                                 prim.task,
                                 prim.local_rect);
#endif

#ifdef WR_FEATURE_CLIP
    write_clip(vi.screen_pos, prim.clip_area);
#endif
}
#endif

#ifdef WR_FRAGMENT_SHADER
void main(void) {
    float alpha = 1.0;
#ifdef WR_FEATURE_TRANSFORM
    alpha = 0.0;
    init_transform_fs(vLocalPos, alpha);
#endif

#ifdef WR_FEATURE_CLIP
    alpha = min(alpha, do_clip());
#endif
    oFragColor = vColor * vec4(1.0, 1.0, 1.0, alpha);
}
#endif
