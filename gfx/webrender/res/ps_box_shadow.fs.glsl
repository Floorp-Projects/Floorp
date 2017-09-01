/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

void main(void) {
    vec4 clip_scale = vec4(1.0, 1.0, 1.0, do_clip());

    // Mirror and stretch the box shadow corner over the entire
    // primitives.
    vec2 uv = vMirrorPoint - abs(vUv.xy - vMirrorPoint);

    // Ensure that we don't fetch texels outside the box
    // shadow corner. This can happen, for example, when
    // drawing the outer parts of an inset box shadow.
    uv = clamp(uv, vec2(0.0), vec2(1.0));

    // Map the unit UV to the actual UV rect in the cache.
    uv = mix(vCacheUvRectCoords.xy, vCacheUvRectCoords.zw, uv);

    // Modulate the box shadow by the color.
    float mask = texture(sColor1, vec3(uv, vUv.z)).r;
    oFragColor = clip_scale * dither(vColor * vec4(1.0, 1.0, 1.0, mask));
}
