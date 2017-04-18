#line 1
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Edge color transition
flat varying vec4 vColor0;
flat varying vec4 vColor1;
flat varying vec4 vColorEdgeLine;

// Border radius
flat varying vec2 vClipCenter;
flat varying vec2 vOuterRadii;
flat varying vec2 vInnerRadii;
flat varying vec2 vClipSign;

#ifdef WR_FEATURE_TRANSFORM
flat varying RectWithSize vLocalRect;
varying vec3 vLocalPos;
#else
varying vec2 vLocalPos;
#endif
