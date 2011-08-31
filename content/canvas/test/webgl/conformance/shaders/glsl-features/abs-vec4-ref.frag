// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

precision mediump float;

varying vec2 vTexcoord;
varying vec4 vColor;

float abs_emu1(float value) {
  return value >= 0.0 ? value : -value;
}

vec4 abs_emu(vec4 value) {
  return vec4(
      abs_emu1(value.x),
      abs_emu1(value.y),
      abs_emu1(value.z),
      abs_emu1(value.w));
}

void main()
{
   gl_FragColor = abs_emu(vColor * 2.0 - vec4(1, 1, 1, 1));
}


