#line 1
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Edge color transition
flat varying vec4 vColor00;
flat varying vec4 vColor01;
flat varying vec4 vColor10;
flat varying vec4 vColor11;
flat varying vec4 vColorEdgeLine;

// Border radius
flat varying vec2 vClipCenter;
flat varying vec4 vRadii0;
flat varying vec4 vRadii1;
flat varying vec2 vClipSign;
flat varying vec4 vEdgeDistance;
flat varying float vSDFSelect;

// Border style
flat varying float vAlphaSelect;

#ifdef WR_FEATURE_TRANSFORM
flat varying RectWithSize vLocalRect;
varying vec3 vLocalPos;
#else
varying vec2 vLocalPos;
#endif
