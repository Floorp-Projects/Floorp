#line 1
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Draw a text run to a cache target. These are always
// drawn un-transformed. These are used for effects such
// as text-shadow.

void main(void) {
    PrimitiveInstance pi = fetch_prim_instance();
    RenderTaskData task = fetch_render_task(pi.render_task_index);
    TextRun text = fetch_text_run(pi.specific_prim_address);
    Glyph glyph = fetch_glyph(pi.user_data0);
    PrimitiveGeometry pg = fetch_prim_geometry(pi.global_prim_index);
    ResourceRect res = fetch_resource_rect(pi.user_data1);

    // Glyphs size is already in device-pixels.
    // The render task origin is in device-pixels. Offset that by
    // the glyph offset, relative to its primitive bounding rect.
    vec2 size = res.uv_rect.zw - res.uv_rect.xy;
    vec2 origin = task.data0.xy + uDevicePixelRatio * (glyph.offset.xy - pg.local_rect.p0);
    vec4 local_rect = vec4(origin, size);

    vec2 texture_size = vec2(textureSize(sColor0, 0));
    vec2 st0 = res.uv_rect.xy / texture_size;
    vec2 st1 = res.uv_rect.zw / texture_size;

    vec2 pos = mix(local_rect.xy,
                   local_rect.xy + local_rect.zw,
                   aPosition.xy);
	vUv = mix(st0, st1, aPosition.xy);
	vColor = text.color;

    gl_Position = uTransform * vec4(pos, 0.0, 1.0);
}
