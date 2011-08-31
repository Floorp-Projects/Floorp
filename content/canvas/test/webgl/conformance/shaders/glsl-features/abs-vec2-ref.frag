// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

precision mediump float;

varying vec2 vTexcoord;
varying vec4 vColor;

float abs_emu1(float value) {
  return value >= 0.0 ? value : -value;
}

vec2 abs_emu(vec2 value) {
  return vec2(
      abs_emu1(value.x),
      abs_emu1(value.y));
}

void main()
{
   gl_FragColor = vec4(
     0,
     abs_emu(vColor.xy * 2.0 - vec2(1, 1)),
     1);
}


