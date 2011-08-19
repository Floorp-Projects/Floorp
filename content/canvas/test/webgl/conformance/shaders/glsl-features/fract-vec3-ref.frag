// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

precision mediump float;

varying vec2 vTexcoord;
varying vec4 vColor;

float fract_emu1(float value) {
  return value - floor(value);
}

vec3 fract_emu(vec3 value) {
  return vec3(
	  fract_emu1(value.x),
	  fract_emu1(value.y),
	  fract_emu1(value.z));
}

void main()
{
   gl_FragColor = vec4(
      fract_emu(vColor.xyz * 4.0 - vec3(2, 2, 2)),
      1);
}




