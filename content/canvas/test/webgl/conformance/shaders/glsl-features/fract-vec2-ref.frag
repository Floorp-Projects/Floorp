// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

precision mediump float;

varying vec2 vTexcoord;
varying vec4 vColor;

float fract_emu1(float value) {
  return value - floor(value);
}

vec2 fract_emu(vec2 value) {
  return vec2(
	  fract_emu1(value.x),
	  fract_emu1(value.y));
}

void main()
{
   gl_FragColor = vec4(
      fract_emu(vColor.xy * 4.0 - vec2(2, 2)),
      0, 1);
}




