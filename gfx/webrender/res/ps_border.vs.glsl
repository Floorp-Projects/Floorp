#line 1
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

void main(void) {
    Primitive prim = load_primitive();
    Border border = fetch_border(prim.prim_index);
    int sub_part = prim.sub_index;
    vBorderRect = prim.local_rect;

    vec2 tl_outer = vBorderRect.p0;
    vec2 tl_inner = tl_outer + vec2(max(border.radii[0].x, border.widths.x),
                                    max(border.radii[0].y, border.widths.y));

    vec2 tr_outer = vec2(vBorderRect.p0.x + vBorderRect.size.x,
                         vBorderRect.p0.y);
    vec2 tr_inner = tr_outer + vec2(-max(border.radii[0].z, border.widths.z),
                                    max(border.radii[0].w, border.widths.y));

    vec2 br_outer = vec2(vBorderRect.p0.x + vBorderRect.size.x,
                         vBorderRect.p0.y + vBorderRect.size.y);
    vec2 br_inner = br_outer - vec2(max(border.radii[1].x, border.widths.z),
                                    max(border.radii[1].y, border.widths.w));

    vec2 bl_outer = vec2(vBorderRect.p0.x,
                         vBorderRect.p0.y + vBorderRect.size.y);
    vec2 bl_inner = bl_outer + vec2(max(border.radii[1].z, border.widths.x),
                                    -max(border.radii[1].w, border.widths.w));

    RectWithSize segment_rect;
    switch (sub_part) {
        case PST_TOP_LEFT:
            segment_rect.p0 = tl_outer;
            segment_rect.size = tl_inner - tl_outer;
            vBorderStyle = int(border.style.x);
            vHorizontalColor = border.colors[BORDER_LEFT];
            vVerticalColor = border.colors[BORDER_TOP];
            vRadii = vec4(border.radii[0].xy,
                          border.radii[0].xy - border.widths.xy);
            break;
        case PST_TOP_RIGHT:
            segment_rect.p0 = vec2(tr_inner.x, tr_outer.y);
            segment_rect.size = vec2(tr_outer.x - tr_inner.x, tr_inner.y - tr_outer.y);
            vBorderStyle = int(border.style.y);
            vHorizontalColor = border.colors[BORDER_TOP];
            vVerticalColor = border.colors[BORDER_RIGHT];
            vRadii = vec4(border.radii[0].zw,
                          border.radii[0].zw - border.widths.zy);
            break;
        case PST_BOTTOM_RIGHT:
            segment_rect.p0 = br_inner;
            segment_rect.size = br_outer - br_inner;
            vBorderStyle = int(border.style.z);
            vHorizontalColor = border.colors[BORDER_BOTTOM];
            vVerticalColor = border.colors[BORDER_RIGHT];
            vRadii = vec4(border.radii[1].xy,
                          border.radii[1].xy - border.widths.zw);
            break;
        case PST_BOTTOM_LEFT:
            segment_rect.p0 = vec2(bl_outer.x, bl_inner.y);
            segment_rect.size = vec2(bl_inner.x - bl_outer.x, bl_outer.y - bl_inner.y);
            vBorderStyle = int(border.style.w);
            vHorizontalColor = border.colors[BORDER_BOTTOM];
            vVerticalColor = border.colors[BORDER_LEFT];
            vRadii = vec4(border.radii[1].zw,
                          border.radii[1].zw - border.widths.xw);
            break;
        case PST_LEFT:
            segment_rect.p0 = vec2(tl_outer.x, tl_inner.y);
            segment_rect.size = vec2(border.widths.x, bl_inner.y - tl_inner.y);
            vBorderStyle = int(border.style.x);
            vHorizontalColor = border.colors[BORDER_LEFT];
            vVerticalColor = border.colors[BORDER_LEFT];
            vRadii = vec4(0.0);
            break;
        case PST_RIGHT:
            segment_rect.p0 = vec2(tr_outer.x - border.widths.z, tr_inner.y);
            segment_rect.size = vec2(border.widths.z, br_inner.y - tr_inner.y);
            vBorderStyle = int(border.style.z);
            vHorizontalColor = border.colors[BORDER_RIGHT];
            vVerticalColor = border.colors[BORDER_RIGHT];
            vRadii = vec4(0.0);
            break;
        case PST_BOTTOM:
            segment_rect.p0 = vec2(bl_inner.x, bl_outer.y - border.widths.w);
            segment_rect.size = vec2(br_inner.x - bl_inner.x, border.widths.w);
            vBorderStyle = int(border.style.w);
            vHorizontalColor = border.colors[BORDER_BOTTOM];
            vVerticalColor = border.colors[BORDER_BOTTOM];
            vRadii = vec4(0.0);
            break;
        case PST_TOP:
            segment_rect.p0 = vec2(tl_inner.x, tl_outer.y);
            segment_rect.size = vec2(tr_inner.x - tl_inner.x, border.widths.y);
            vBorderStyle = int(border.style.y);
            vHorizontalColor = border.colors[BORDER_TOP];
            vVerticalColor = border.colors[BORDER_TOP];
            vRadii = vec4(0.0);
            break;
    }

#ifdef WR_FEATURE_TRANSFORM
    TransformVertexInfo vi = write_transform_vertex(segment_rect,
                                                    prim.local_clip_rect,
                                                    prim.z,
                                                    prim.layer,
                                                    prim.task,
                                                    prim.local_rect.p0);
    vLocalPos = vi.local_pos;
    vLocalRect = segment_rect;
#else
    VertexInfo vi = write_vertex(segment_rect,
                                 prim.local_clip_rect,
                                 prim.z,
                                 prim.layer,
                                 prim.task,
                                 prim.local_rect.p0);
    vLocalPos = vi.local_pos.xy;
#endif

    write_clip(vi.screen_pos, prim.clip_area);

    float x0, y0, x1, y1;
    switch (sub_part) {
        // These are the layer tile part PrimitivePart as uploaded by the tiling.rs
        case PST_TOP_LEFT:
            x0 = segment_rect.p0.x;
            y0 = segment_rect.p0.y;
            // These are width / heights
            x1 = segment_rect.p0.x + segment_rect.size.x;
            y1 = segment_rect.p0.y + segment_rect.size.y;

            // The radius here is the border-radius. This is 0, so vRefPoint will
            // just be the top left (x,y) corner.
            vRefPoint = vec2(x0, y0) + vRadii.xy;
            break;
        case PST_TOP_RIGHT:
            x0 = segment_rect.p0.x + segment_rect.size.x;
            y0 = segment_rect.p0.y;
            x1 = segment_rect.p0.x;
            y1 = segment_rect.p0.y + segment_rect.size.y;
            vRefPoint = vec2(x0, y0) + vec2(-vRadii.x, vRadii.y);
            break;
        case PST_BOTTOM_LEFT:
            x0 = segment_rect.p0.x;
            y0 = segment_rect.p0.y + segment_rect.size.y;
            x1 = segment_rect.p0.x + segment_rect.size.x;
            y1 = segment_rect.p0.y;
            vRefPoint = vec2(x0, y0) + vec2(vRadii.x, -vRadii.y);
            break;
        case PST_BOTTOM_RIGHT:
            x0 = segment_rect.p0.x;
            y0 = segment_rect.p0.y;
            x1 = segment_rect.p0.x + segment_rect.size.x;
            y1 = segment_rect.p0.y + segment_rect.size.y;
            vRefPoint = vec2(x1, y1) + vec2(-vRadii.x, -vRadii.y);
            break;
        case PST_TOP:
        case PST_LEFT:
        case PST_BOTTOM:
        case PST_RIGHT:
            vRefPoint = segment_rect.p0.xy;
            x0 = segment_rect.p0.x;
            y0 = segment_rect.p0.y;
            x1 = segment_rect.p0.x + segment_rect.size.x;
            y1 = segment_rect.p0.y + segment_rect.size.y;
            break;
    }

    // y1 - y0 is the height of the corner / line
    // x1 - x0 is the width of the corner / line.
    float width = x1 - x0;
    float height = y1 - y0;

    vBorderPart = sub_part;
    vPieceRect = vec4(x0, y0, width, height);

    // The fragment shader needs to calculate the distance from the bisecting line
    // to properly mix border colors. For transformed borders, we calculate this distance
    // in the fragment shader itself. For non-transformed borders, we can use the
    // interpolator.
#ifdef WR_FEATURE_TRANSFORM
    vPieceRectHypotenuseLength = sqrt(pow(width, 2.0) + pow(height, 2.0));
#else
    vDistanceFromMixLine = (vi.local_pos.x - x0) * height -
                           (vi.local_pos.y - y0) * width;
    vDistanceFromMiddle = (vi.local_pos.x - vBorderRect.p0.x) +
                          (vi.local_pos.y - vBorderRect.p0.y) -
                          0.5 * (vBorderRect.size.x + vBorderRect.size.y);
#endif
}
