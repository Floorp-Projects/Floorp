// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

precision mediump float;

varying vec2 vTexcoord;
varying vec4 vColor;

void main()
{
   gl_FragColor = vec4(
      floor(vColor.x * 8.0 - 4.0) / 8.0 + 0.5,
      0, 0, 1);
}



