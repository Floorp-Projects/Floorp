// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

attribute vec4 aPosition;
attribute vec2 aTexcoord;

varying vec2 vTexcoord;
varying vec4 vColor;

float sign_emu1(float value) {
  if (value == 0.0) return 0.0;
  return value > 0.0 ? 1.0 : -1.0;
}

vec4 sign_emu(vec4 value) {
  return vec4(
	  sign_emu1(value.x),
	  sign_emu1(value.y),
	  sign_emu1(value.z),
	  sign_emu1(value.w));
}

void main()
{
   gl_Position = aPosition;
   vTexcoord = aTexcoord;
   vColor = sign_emu(vec4(
      aTexcoord.yx * -2.0 + vec2(1, 1),
      aTexcoord * 2.0 - vec2(1, 1)));
}




