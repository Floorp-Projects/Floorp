/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef WR_VERTEX_SHADER

void brush_vs(
    VertexInfo vi,
    int prim_address,
    RectWithSize local_rect,
    RectWithSize segment_rect,
    ivec3 user_data,
    mat4 transform,
    PictureTask pic_task,
    int brush_flags,
    vec4 segment_data
);

#define VECS_PER_SEGMENT                    2

#define BRUSH_FLAG_PERSPECTIVE_INTERPOLATION    1
#define BRUSH_FLAG_SEGMENT_RELATIVE             2
#define BRUSH_FLAG_SEGMENT_REPEAT_X             4
#define BRUSH_FLAG_SEGMENT_REPEAT_Y             8

void main(void) {
    // Load the brush instance from vertex attributes.
    int prim_header_address = aData.x;
    int clip_address = aData.y;
    int segment_index = aData.z & 0xffff;
    int edge_flags = (aData.z >> 16) & 0xff;
    int brush_flags = (aData.z >> 24) & 0xff;
    PrimitiveHeader ph = fetch_prim_header(prim_header_address);

    // Fetch the segment of this brush primitive we are drawing.
    int segment_address = ph.specific_prim_address +
                          VECS_PER_SPECIFIC_BRUSH +
                          segment_index * VECS_PER_SEGMENT;

    vec4[2] segment_data = fetch_from_resource_cache_2(segment_address);
    RectWithSize local_segment_rect = RectWithSize(segment_data[0].xy, segment_data[0].zw);

    VertexInfo vi;

    // Fetch the dynamic picture that we are drawing on.
    PictureTask pic_task = fetch_picture_task(ph.render_task_index);
    ClipArea clip_area = fetch_clip_area(clip_address);

    Transform transform = fetch_transform(ph.transform_id);

    // Write the normal vertex information out.
    if (transform.is_axis_aligned) {
        vi = write_vertex(
            local_segment_rect,
            ph.local_clip_rect,
            ph.z,
            transform,
            pic_task,
            ph.local_rect
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
        bvec4 edge_mask = notEqual(edge_flags & ivec4(1, 2, 4, 8), ivec4(0));
        bool do_perspective_interpolation = (brush_flags & BRUSH_FLAG_PERSPECTIVE_INTERPOLATION) != 0;

        vi = write_transform_vertex(
            local_segment_rect,
            ph.local_rect,
            ph.local_clip_rect,
            mix(vec4(0.0), vec4(1.0), edge_mask),
            ph.z,
            transform,
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
        ph.specific_prim_address,
        ph.local_rect,
        local_segment_rect,
        ph.user_data,
        transform.m,
        pic_task,
        brush_flags,
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
