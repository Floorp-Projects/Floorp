// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

attribute vec4 aPosition;
attribute vec2 aTexcoord;

varying vec2 vTexcoord;
varying vec4 vColor;

void main()
{
   gl_Position = aPosition;
   vTexcoord = aTexcoord;
   vColor = vec4(
     sign(vec3(0, aTexcoord * 2.0 - vec2(1, 1))),
     1);
}




