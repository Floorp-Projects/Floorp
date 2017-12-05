/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef WR_VERTEX_SHADER

void brush_vs(
    int prim_address,
    vec2 local_pos,
    RectWithSize local_rect,
    ivec2 user_data
);

#define RASTERIZATION_MODE_LOCAL_SPACE      0.0
#define RASTERIZATION_MODE_SCREEN_SPACE     1.0

#define SEGMENT_ALL             0
#define SEGMENT_TOP_LEFT        1
#define SEGMENT_TOP_RIGHT       2
#define SEGMENT_BOTTOM_RIGHT    3
#define SEGMENT_BOTTOM_LEFT     4
#define SEGMENT_TOP_MID         5
#define SEGMENT_MID_RIGHT       6
#define SEGMENT_BOTTOM_MID      7
#define SEGMENT_MID_LEFT        8
#define SEGMENT_CENTER          9

#define AA_KIND_DEFAULT         0
#define AA_KIND_SEGMENT         1

#define VECS_PER_BRUSH_PRIM                 4

struct BrushInstance {
    int picture_address;
    int prim_address;
    int clip_node_id;
    int scroll_node_id;
    int clip_address;
    int z;
    int segment_kind;
    ivec2 user_data;
};

BrushInstance load_brush() {
	BrushInstance bi;

    bi.picture_address = aData0.x;
    bi.prim_address = aData0.y;
    bi.clip_node_id = aData0.z / 65536;
    bi.scroll_node_id = aData0.z % 65536;
    bi.clip_address = aData0.w;
    bi.z = aData1.x;
    bi.segment_kind = aData1.y;
    bi.user_data = aData1.zw;

    return bi;
}

struct BrushPrimitive {
    RectWithSize local_rect;
    RectWithSize local_clip_rect;
    vec4 offsets;
    int aa_kind;
};

BrushPrimitive fetch_brush_primitive(int address) {
    vec4 data[4] = fetch_from_resource_cache_4(address);

    BrushPrimitive prim = BrushPrimitive(
        RectWithSize(data[0].xy, data[0].zw),
        RectWithSize(data[1].xy, data[1].zw),
        data[2],
        int(data[3].x)
    );

    return prim;
}

void main(void) {
    // Load the brush instance from vertex attributes.
    BrushInstance brush = load_brush();

    // Load the geometry for this brush. For now, this is simply the
    // local rect of the primitive. In the future, this will support
    // loading segment rects, and other rect formats (glyphs).
    BrushPrimitive brush_prim = fetch_brush_primitive(brush.prim_address);

    // Fetch the segment of this brush primitive we are drawing.
    RectWithSize local_segment_rect;
    vec4 edge_aa_segment_mask;

    // p0 = origin of outer rect
    // p1 = origin of inner rect
    // p2 = bottom right corner of inner rect
    // p3 = bottom right corner of outer rect
    vec2 p0 = brush_prim.local_rect.p0;
    vec2 p1 = brush_prim.local_rect.p0 + brush_prim.offsets.xy;
    vec2 p2 = brush_prim.local_rect.p0 + brush_prim.local_rect.size - brush_prim.offsets.zw;
    vec2 p3 = brush_prim.local_rect.p0 + brush_prim.local_rect.size;

    switch (brush.segment_kind) {
        case SEGMENT_ALL:
            local_segment_rect = brush_prim.local_rect;
            break;

        case SEGMENT_TOP_LEFT:
            local_segment_rect = RectWithSize(p0, p1 - p0);
            break;
        case SEGMENT_TOP_RIGHT:
            local_segment_rect = RectWithSize(vec2(p2.x, p0.y), vec2(p3.x - p2.x, p1.y - p0.y));
            break;
        case SEGMENT_BOTTOM_RIGHT:
            local_segment_rect = RectWithSize(vec2(p2.x, p2.y), vec2(p3.x - p2.x, p3.y - p2.y));
            break;
        case SEGMENT_BOTTOM_LEFT:
            local_segment_rect = RectWithSize(vec2(p0.x, p2.y), vec2(p1.x - p0.x, p3.y - p2.y));
            break;

        case SEGMENT_TOP_MID:
            local_segment_rect = RectWithSize(vec2(p1.x, p0.y), vec2(p2.x - p1.x, p1.y - p0.y));
            break;
        case SEGMENT_MID_RIGHT:
            local_segment_rect = RectWithSize(vec2(p2.x, p1.y), vec2(p3.x - p2.x, p2.y - p1.y));
            break;
        case SEGMENT_BOTTOM_MID:
            local_segment_rect = RectWithSize(vec2(p1.x, p2.y), vec2(p2.x - p1.x, p3.y - p2.y));
            break;
        case SEGMENT_MID_LEFT:
            local_segment_rect = RectWithSize(vec2(p0.x, p1.y), vec2(p1.x - p0.x, p2.y - p1.y));
            break;

        case SEGMENT_CENTER:
            local_segment_rect = RectWithSize(p1, p2 - p1);
            break;

        default:
            local_segment_rect = RectWithSize(vec2(0.0), vec2(0.0));
            break;
    }

    switch (brush_prim.aa_kind) {
        case AA_KIND_SEGMENT:
            // TODO: select these correctly based on the segment kind.
            edge_aa_segment_mask = vec4(1.0);
            break;
        case AA_KIND_DEFAULT:
            edge_aa_segment_mask = vec4(1.0);
            break;
    }

    vec2 device_pos, local_pos;

    // Fetch the dynamic picture that we are drawing on.
    PictureTask pic_task = fetch_picture_task(brush.picture_address);

    if (pic_task.rasterization_mode == RASTERIZATION_MODE_LOCAL_SPACE) {
        local_pos = local_segment_rect.p0 + aPosition.xy * local_segment_rect.size;

        // Right now - pictures only support local positions. In the future, this
        // will be expanded to support transform picture types (the common kind).
        device_pos = pic_task.common_data.task_rect.p0 +
                     uDevicePixelRatio * (local_pos - pic_task.content_origin);

        // Write the final position transformed by the orthographic device-pixel projection.
        gl_Position = uTransform * vec4(device_pos, 0.0, 1.0);
    } else {
        VertexInfo vi;
        Layer layer = fetch_layer(brush.clip_node_id, brush.scroll_node_id);
        ClipArea clip_area = fetch_clip_area(brush.clip_address);

        // Write the normal vertex information out.
        if (layer.is_axis_aligned) {
            vi = write_vertex(
                local_segment_rect,
                brush_prim.local_clip_rect,
                float(brush.z),
                layer,
                pic_task,
                brush_prim.local_rect
            );

            // TODO(gw): vLocalBounds may be referenced by
            //           the fragment shader when running in
            //           the alpha pass, even on non-transformed
            //           items. For now, just ensure it has no
            //           effect. We can tidy this up as we move
            //           more items to be brush shaders.
#ifdef WR_FEATURE_ALPHA_PASS
            vLocalBounds = vec4(vec2(-1000000.0), vec2(1000000.0));
#endif
        } else {
            vi = write_transform_vertex(
                local_segment_rect,
                brush_prim.local_rect,
                brush_prim.local_clip_rect,
                edge_aa_segment_mask,
                float(brush.z),
                layer,
                pic_task
            );
        }

        local_pos = vi.local_pos;

        // For brush instances in the alpha pass, always write
        // out clip information.
        // TODO(gw): It's possible that we might want alpha
        //           shaders that don't clip in the future,
        //           but it's reasonable to assume that one
        //           implies the other, for now.
#ifdef WR_FEATURE_ALPHA_PASS
        write_clip(
            vi.screen_pos,
            clip_area
        );
#endif
    }

    // Run the specific brush VS code to write interpolators.
    brush_vs(
        brush.prim_address + VECS_PER_BRUSH_PRIM,
        local_pos,
        brush_prim.local_rect,
        brush.user_data
    );
}
#endif

#ifdef WR_FRAGMENT_SHADER

vec4 brush_fs();

void main(void) {
    // Run the specific brush FS code to output the color.
    vec4 color = brush_fs();

#ifdef WR_FEATURE_ALPHA_PASS
    // Apply the clip mask
    color *= do_clip();
#endif

    // TODO(gw): Handle pre-multiply common code here as required.
    oFragColor = color;
}
#endif
