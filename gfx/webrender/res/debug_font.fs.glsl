/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

varying vec2 vColorTexCoord;
varying vec4 vColor;

void main(void)
{
    float alpha = texture(sColor0, vec3(vColorTexCoord.xy, 0.0)).r;
    oFragColor = vec4(vColor.xyz, vColor.w * alpha);
}
