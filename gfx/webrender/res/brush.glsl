/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef WR_VERTEX_SHADER

void brush_vs(
    VertexInfo vi,
    int prim_address,
    RectWithSize local_rect,
    ivec3 user_data,
    mat4 transform,
    PictureTask pic_task,
    vec4 segment_data
);

#define VECS_PER_BRUSH_PRIM                 2
#define VECS_PER_SEGMENT                    2

#define BRUSH_FLAG_PERSPECTIVE_INTERPOLATION    1

struct BrushInstance {
    int picture_address;
    int prim_address;
    int clip_chain_rect_index;
    int scroll_node_id;
    int clip_address;
    int z;
    int segment_index;
    int edge_mask;
    int flags;
    ivec3 user_data;
};

BrushInstance load_brush() {
    BrushInstance bi;

    bi.picture_address = aData0.x & 0xffff;
    bi.clip_address = aData0.x >> 16;
    bi.prim_address = aData0.y;
    bi.clip_chain_rect_index = aData0.z >> 16;
    bi.scroll_node_id = aData0.z & 0xffff;
    bi.z = aData0.w;
    bi.segment_index = aData1.x & 0xffff;
    bi.edge_mask = (aData1.x >> 16) & 0xff;
    bi.flags = (aData1.x >> 24);
    bi.user_data = aData1.yzw;

    return bi;
}

struct BrushPrimitive {
    RectWithSize local_rect;
    RectWithSize local_clip_rect;
};

BrushPrimitive fetch_brush_primitive(int address, int clip_chain_rect_index) {
    vec4 data[2] = fetch_from_resource_cache_2(address);

    RectWithSize clip_chain_rect = fetch_clip_chain_rect(clip_chain_rect_index);
    RectWithSize brush_clip_rect = RectWithSize(data[1].xy, data[1].zw);
    RectWithSize clip_rect = intersect_rects(clip_chain_rect, brush_clip_rect);

    BrushPrimitive prim = BrushPrimitive(RectWithSize(data[0].xy, data[0].zw), clip_rect);

    return prim;
}

void main(void) {
    // Load the brush instance from vertex attributes.
    BrushInstance brush = load_brush();

    // Load the geometry for this brush. For now, this is simply the
    // local rect of the primitive. In the future, this will support
    // loading segment rects, and other rect formats (glyphs).
    BrushPrimitive brush_prim =
        fetch_brush_primitive(brush.prim_address, brush.clip_chain_rect_index);

    // Fetch the segment of this brush primitive we are drawing.
    int segment_address = brush.prim_address +
                          VECS_PER_BRUSH_PRIM +
                          VECS_PER_SPECIFIC_BRUSH +
                          brush.segment_index * VECS_PER_SEGMENT;

    vec4[2] segment_data = fetch_from_resource_cache_2(segment_address);
    RectWithSize local_segment_rect = RectWithSize(segment_data[0].xy, segment_data[0].zw);

    VertexInfo vi;

    // Fetch the dynamic picture that we are drawing on.
    PictureTask pic_task = fetch_picture_task(brush.picture_address);
    ClipArea clip_area = fetch_clip_area(brush.clip_address);

    ClipScrollNode scroll_node = fetch_clip_scroll_node(brush.scroll_node_id);

    // Write the normal vertex information out.
    if (scroll_node.is_axis_aligned) {
        vi = write_vertex(
            local_segment_rect,
            brush_prim.local_clip_rect,
            float(brush.z),
            scroll_node,
            pic_task,
            brush_prim.local_rect
        );

        // TODO(gw): transform bounds may be referenced by
        //           the fragment shader when running in
        //           the alpha pass, even on non-transformed
        //           items. For now, just ensure it has no
        //           effect. We can tidy this up as we move
        //           more items to be brush shaders.
#ifdef WR_FEATURE_ALPHA_PASS
        init_transform_vs(vec4(vec2(-1000000.0), vec2(1000000.0)));
#endif
    } else {
        bvec4 edge_mask = notEqual(brush.edge_mask & ivec4(1, 2, 4, 8), ivec4(0));
        bool do_perspective_interpolation = (brush.flags & BRUSH_FLAG_PERSPECTIVE_INTERPOLATION) != 0;

        vi = write_transform_vertex(
            local_segment_rect,
            brush_prim.local_rect,
            brush_prim.local_clip_rect,
            mix(vec4(0.0), vec4(1.0), edge_mask),
            float(brush.z),
            scroll_node,
            pic_task,
            do_perspective_interpolation
        );
    }

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

    // Run the specific brush VS code to write interpolators.
    brush_vs(
        vi,
        brush.prim_address + VECS_PER_BRUSH_PRIM,
        brush_prim.local_rect,
        brush.user_data,
        scroll_node.transform,
        pic_task,
        segment_data[1]
    );
}
#endif

#ifdef WR_FRAGMENT_SHADER

struct Fragment {
    vec4 color;
#ifdef WR_FEATURE_DUAL_SOURCE_BLENDING
    vec4 blend;
#endif
};

Fragment brush_fs();

void main(void) {
    // Run the specific brush FS code to output the color.
    Fragment frag = brush_fs();

#ifdef WR_FEATURE_ALPHA_PASS
    // Apply the clip mask
    float clip_alpha = do_clip();

    frag.color *= clip_alpha;

    #ifdef WR_FEATURE_DUAL_SOURCE_BLENDING
        oFragBlend = frag.blend * clip_alpha;
    #endif
#endif

    // TODO(gw): Handle pre-multiply common code here as required.
    oFragColor = frag.color;
}
#endif
