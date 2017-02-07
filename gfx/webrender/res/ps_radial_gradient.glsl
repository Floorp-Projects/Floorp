/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

flat varying int vStopCount;
flat varying vec2 vStartCenter;
flat varying vec2 vEndCenter;
flat varying float vStartRadius;
flat varying float vEndRadius;
varying vec2 vPos;
flat varying vec4 vColors[MAX_STOPS_PER_RADIAL_GRADIENT];
flat varying vec4 vOffsets[MAX_STOPS_PER_RADIAL_GRADIENT/4];
