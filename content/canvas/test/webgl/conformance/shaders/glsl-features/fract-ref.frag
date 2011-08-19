// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

precision mediump float;

varying vec2 vTexcoord;
varying vec4 vColor;

float fract_emu(float value) {
  return value - floor(value);
}

void main()
{
   gl_FragColor = vec4(
      fract_emu(vColor.x * 4.0 - 2.0),
      0, 0, 1);
}




