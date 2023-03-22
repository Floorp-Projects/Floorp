/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include base

#ifdef WR_VERTEX_SHADER

attribute vec2 aPosition;

// See CopyInstance struct.
attribute vec4 a_src_rect;
attribute vec4 a_dst_rect;
attribute vec2 a_dst_texture_size;

varying highp vec2 v_uv;

void main(void) {
    // We use texel fetch so v_uv is in unnormalized device space.
    v_uv = mix(a_src_rect.xy, a_src_rect.zw, aPosition.xy);

    // Transform into framebuffer [-1, 1] space.
    vec2 pos = mix(a_dst_rect.xy, a_dst_rect.zw, aPosition.xy);
    gl_Position = vec4(pos / (a_dst_texture_size  * 0.5) - vec2(1.0, 1.0), 0.0, 1.0);
}
#endif

#ifdef WR_FRAGMENT_SHADER


out vec4 oFragColor;

varying highp vec2 v_uv;

uniform sampler2D sColor0;

void main(void) {
    oFragColor = texelFetch(sColor0, ivec2(v_uv), 0);
}

#endif
