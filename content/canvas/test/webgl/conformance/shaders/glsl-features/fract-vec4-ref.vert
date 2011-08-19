// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

attribute vec4 aPosition;
attribute vec2 aTexcoord;

varying vec2 vTexcoord;
varying vec4 vColor;

float fract_emu1(float value) {
  return value - floor(value);
}

vec4 fract_emu(vec4 value) {
  return vec4(
	  fract_emu1(value.x),
	  fract_emu1(value.y),
	  fract_emu1(value.z),
	  fract_emu1(value.w));
}

void main()
{
   gl_Position = aPosition;
   vTexcoord = aTexcoord;
   vec4 color = vec4(
       aTexcoord,
       aTexcoord.x * aTexcoord.y,
       (1.0 - aTexcoord.x) * aTexcoord.y * 0.5 + 0.5);
   vColor = fract_emu(color * 4.0 - vec4(2, 2, 2, 2));
}




