// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

precision mediump float;

varying vec2 vTexcoord;
varying vec4 vColor;

float ceil_emu1(float value) {
  float m = mod(value, 1.0);
  return m != 0.0 ? (value + 1.0 - m) : value;
}

vec2 ceil_emu(vec2 value) {
  return vec2(
	  ceil_emu1(value.x),
	  ceil_emu1(value.y));
}

void main()
{
   gl_FragColor = vec4(
      ceil_emu(vColor.xy * 8.0 - vec2(4, 4)) / 8.0 + vec2(0.5, 0.5),
      0, 1);
}




