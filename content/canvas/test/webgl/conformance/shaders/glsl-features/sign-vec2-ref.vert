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

vec2 sign_emu(vec2 value) {
  return vec2(sign_emu1(value.x), sign_emu1(value.y));
}

void main()
{
   gl_Position = aPosition;
   vTexcoord = aTexcoord;
   vColor = vec4(
     sign_emu(aTexcoord * 2.0 - vec2(1, 1)),
     0, 
     1);
}




