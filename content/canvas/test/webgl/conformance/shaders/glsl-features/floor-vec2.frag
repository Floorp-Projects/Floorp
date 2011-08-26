// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

precision mediump float;

varying vec2 vTexcoord;
varying vec4 vColor;

void main()
{
   gl_FragColor = vec4(
      floor(vColor.xy * 8.0 - vec2(4, 4)) / 8.0 + vec2(0.5, 0.5),
      0, 1);
}



