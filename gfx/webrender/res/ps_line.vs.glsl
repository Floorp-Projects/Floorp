#line 1
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define LINE_ORIENTATION_VERTICAL       0
#define LINE_ORIENTATION_HORIZONTAL     1

void main(void) {
    Primitive prim = load_primitive();
    Line line = fetch_line(prim.specific_prim_address);

    vec2 pos, size;

    switch (int(line.orientation)) {
        case LINE_ORIENTATION_HORIZONTAL:
            vAxisSelect = 0.0;
            pos = prim.local_rect.p0;
            size = prim.local_rect.size;
            break;
        case LINE_ORIENTATION_VERTICAL:
            vAxisSelect = 1.0;
            pos = prim.local_rect.p0.yx;
            size = prim.local_rect.size.yx;
            break;
    }

    vLocalOrigin = pos;
    vStyle = int(line.style);

    switch (vStyle) {
        case LINE_STYLE_SOLID: {
            break;
        }
        case LINE_STYLE_DASHED: {
            // y = dash on + off length
            // z = dash length
            // w = center line of edge cross-axis (for dots only)
            float desired_dash_length = size.y * 3.0;
            // Consider half total length since there is an equal on/off for each dash.
            float dash_count = 1.0 + ceil(size.x / desired_dash_length);
            float dash_length = size.x / dash_count;
            vParams = vec4(2.0 * dash_length,
                           dash_length,
                           0.0,
                           0.0);
            break;
        }
        case LINE_STYLE_DOTTED: {
            float diameter = size.y;
            float radius = 0.5 * diameter;
            float dot_count = ceil(0.5 * size.x / diameter);
            float empty_space = size.x - dot_count * diameter;
            float distance_between_centers = diameter + empty_space / dot_count;
            float center_line = pos.y + 0.5 * size.y;
            vParams = vec4(distance_between_centers,
                           radius,
                           center_line,
                           0.0);
            break;
        }
        case LINE_STYLE_WAVY: {
            // Choose some arbitrary values to scale thickness,
            // wave period etc.
            // TODO(gw): Tune these to get closer to what Gecko uses.
            float thickness = 0.2 * size.y;
            vParams = vec4(thickness,
                           size.y * 0.5,
                           size.y * 0.75,
                           max(3.0, thickness * 2.0));
            break;
        }
    }

#ifdef WR_FEATURE_CACHE
    int text_shadow_address = prim.user_data0;
    PrimitiveGeometry shadow_geom = fetch_primitive_geometry(text_shadow_address);
    TextShadow shadow = fetch_text_shadow(text_shadow_address + VECS_PER_PRIM_HEADER);

    vec2 device_origin = prim.task.render_target_origin +
                         uDevicePixelRatio * (prim.local_rect.p0 + shadow.offset - shadow_geom.local_rect.p0);
    vec2 device_size = uDevicePixelRatio * prim.local_rect.size;

    vec2 device_pos = mix(device_origin,
                          device_origin + device_size,
                          aPosition.xy);

    vColor = shadow.color;
    vLocalPos = mix(prim.local_rect.p0,
                    prim.local_rect.p0 + prim.local_rect.size,
                    aPosition.xy);

    gl_Position = uTransform * vec4(device_pos, 0.0, 1.0);
#else
    vColor = line.color;

    #ifdef WR_FEATURE_TRANSFORM
        TransformVertexInfo vi = write_transform_vertex(prim.local_rect,
                                                        prim.local_clip_rect,
                                                        prim.z,
                                                        prim.layer,
                                                        prim.task,
                                                        prim.local_rect);
    #else
        VertexInfo vi = write_vertex(prim.local_rect,
                                     prim.local_clip_rect,
                                     prim.z,
                                     prim.layer,
                                     prim.task,
                                     prim.local_rect);
    #endif

    vLocalPos = vi.local_pos;
    write_clip(vi.screen_pos, prim.clip_area);
#endif
}
