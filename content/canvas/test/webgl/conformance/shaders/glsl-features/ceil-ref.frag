// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

precision mediump float;

varying vec2 vTexcoord;
varying vec4 vColor;

float ceil_emu(float value) {
  float m = mod(value, 1.0);
  return m != 0.0 ? (value + 1.0 - m) : value;
}

void main()
{
   gl_FragColor = vec4(
      ceil_emu(vColor.x * 8.0 - 4.0) / 8.0 + 0.5,
      0, 0, 1);
}




