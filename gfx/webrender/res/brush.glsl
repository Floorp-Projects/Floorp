/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

varying vec2 vLocalPos;
flat varying vec4 vLocalRect;

#ifdef WR_VERTEX_SHADER

struct BrushInstance {
    int picture_address;
    int prim_address;
};

BrushInstance load_brush() {
	BrushInstance bi;

    bi.picture_address = aData0.x;
    bi.prim_address = aData0.y;

    return bi;
}

/*
 The dynamic picture that this brush exists on. Right now, it
 contains minimal information. In the future, it will describe
 the transform mode of primitives on this picture, among other things.
 */
struct PictureTask {
    RectWithSize target_rect;
};

PictureTask fetch_picture_task(int index) {
    ivec2 uv = get_fetch_uv(index, VECS_PER_RENDER_TASK);

    vec4 target_rect = TEXEL_FETCH(sRenderTasks, uv, 0, ivec2(0, 0));

    PictureTask task = PictureTask(RectWithSize(target_rect.xy, target_rect.zw));

    return task;
}

void main(void) {
    // Load the brush instance from vertex attributes.
    BrushInstance brush = load_brush();

    // Fetch the dynamic picture that we are drawing on.
    PictureTask pic_task = fetch_picture_task(brush.picture_address);

    // Load the geometry for this brush. For now, this is simply the
    // local rect of the primitive. In the future, this will support
    // loading segment rects, and other rect formats (glyphs).
    PrimitiveGeometry geom = fetch_primitive_geometry(brush.prim_address);

    // Write the (p0,p1) form of the primitive rect and the local position
    // of this vertex. Specific brush shaders can use this information to
    // interpolate texture coordinates etc.
    vLocalRect = vec4(geom.local_rect.p0, geom.local_rect.p0 + geom.local_rect.size);

    // Right now - pictures only support local positions. In the future, this
    // will be expanded to support transform picture types (the common kind).
    vec2 pos = pic_task.target_rect.p0 + aPosition.xy * pic_task.target_rect.size;
    vLocalPos = aPosition.xy * pic_task.target_rect.size / uDevicePixelRatio;

    // Run the specific brush VS code to write interpolators.
    brush_vs(brush.prim_address, vLocalRect);

    // Write the final position transformed by the orthographic device-pixel projection.
    gl_Position = uTransform * vec4(pos, 0.0, 1.0);
}
#endif

#ifdef WR_FRAGMENT_SHADER
void main(void) {
    // Run the specific brush FS code to output the color.
    vec4 color = brush_fs(vLocalPos, vLocalRect);

    // TODO(gw): Handle pre-multiply common code here as required.
    oFragColor = color;
}
#endif
