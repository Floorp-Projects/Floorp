// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

attribute vec4 aPosition;
attribute vec2 aTexcoord;

varying vec2 vTexcoord;
varying vec4 vColor;

float ceil_emu1(float value) {
  float m = mod(value, 1.0);
  return m != 0.0 ? (value + 1.0 - m) : value;
}

vec4 ceil_emu(vec4 value) {
  return vec4(
	  ceil_emu1(value.x),
	  ceil_emu1(value.y),
	  ceil_emu1(value.z),
	  ceil_emu1(value.w));
}

void main()
{
   gl_Position = aPosition;
   vTexcoord = aTexcoord;
   vec4 color = vec4(
       aTexcoord,
       aTexcoord.x * aTexcoord.y,
       (1.0 - aTexcoord.x) * aTexcoord.y * 0.5 + 0.5);
   vColor = ceil_emu(
       color * 8.0 - vec4(4, 4, 4, 4)) / 8.0 + vec4(0.5, 0.5, 0.5, 0.5);
}




