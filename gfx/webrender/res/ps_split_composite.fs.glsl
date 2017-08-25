/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

void main(void) {
    bvec4 inside = lessThanEqual(vec4(vUvTaskBounds.xy, vUv.xy),
                                 vec4(vUv.xy, vUvTaskBounds.zw));
    if (all(inside)) {
        vec2 uv = clamp(vUv.xy, vUvSampleBounds.xy, vUvSampleBounds.zw);
        oFragColor = textureLod(sCacheRGBA8, vec3(uv, vUv.z), 0.0);
    } else {
        oFragColor = vec4(0.0);
    }
}
