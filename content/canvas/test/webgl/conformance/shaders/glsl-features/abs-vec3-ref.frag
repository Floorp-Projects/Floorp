// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

precision mediump float;

varying vec2 vTexcoord;
varying vec4 vColor;

float abs_emu1(float value) {
  return value >= 0.0 ? value : -value;
}

vec3 abs_emu(vec3 value) {
  return vec3(
      abs_emu1(value.x),
      abs_emu1(value.y),
      abs_emu1(value.z));
}

void main()
{
   gl_FragColor = vec4(
     abs_emu(vColor.xyz * 2.0 - vec3(1, 1, 1)),
     1);
}


