/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Composite a picture cache tile into the framebuffer.

#include shared

varying vec2 vUv;
flat varying vec4 vColor;
flat varying float vLayer;

#ifdef WR_VERTEX_SHADER
in vec4 aDeviceRect;
in vec4 aDeviceClipRect;
in vec4 aColor;
in float aLayer;
in float aZId;

void main(void) {
	// Get world position
    vec2 world_pos = aDeviceRect.xy + aPosition.xy * aDeviceRect.zw;

    // Clip the position to the world space clip rect
    vec2 clipped_world_pos = clamp(world_pos, aDeviceClipRect.xy, aDeviceClipRect.xy + aDeviceClipRect.zw);

    // Derive the normalized UV from the clipped vertex position
    vUv = (clipped_world_pos - aDeviceRect.xy) / aDeviceRect.zw;

    // Pass through color and texture array layer
    vColor = aColor;
    vLayer = aLayer;
    gl_Position = uTransform * vec4(clipped_world_pos, aZId, 1.0);
}
#endif

#ifdef WR_FRAGMENT_SHADER
void main(void) {
	// The color is just the texture sample modulated by a supplied color
	vec4 texel = textureLod(sColor0, vec3(vUv, vLayer), 0.0);
    vec4 color = vColor * texel;
	write_output(color);
}
#endif
