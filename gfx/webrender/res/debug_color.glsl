/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include shared,shared_other

varying vec4 vColor;

#ifdef WR_VERTEX_SHADER
in vec4 aColor;

void main(void) {
    vColor = aColor;
    vec4 pos = vec4(aPosition, 1.0);
    pos.xy = floor(pos.xy * uDevicePixelRatio + 0.5) / uDevicePixelRatio;
    gl_Position = uTransform * pos;
}
#endif

#ifdef WR_FRAGMENT_SHADER
void main(void) {
    oFragColor = vColor;
}
#endif
