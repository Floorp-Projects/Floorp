/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

void main(void) {
    vec2 pos = mod(vPos, vTileRepeat);

    if (pos.x >= vTileSize.x ||
        pos.y >= vTileSize.y) {
        discard;
    }

    float offset = dot(pos - vStartPoint, vScaledDir);

    oFragColor = sample_gradient(offset,
                                 vGradientRepeat,
                                 vGradientIndex,
                                 vGradientTextureSize);
}
