/*
 * Copyright (c) 2009 The Chromium Authors. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *    * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *    * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

description("Tests that index validation verifies the correct number of indices");

var gl = create3DContext();
var program = loadStandardProgram(gl);

// 3 vertices => 1 triangle, interleaved data
var data = new WebGLFloatArray([0, 0, 0, 1,
                                0, 0, 1,
                                1, 0, 0, 1,
                                0, 0, 1,
                                1, 1, 1, 1,
                                0, 0, 1]);
var indices = new WebGLUnsignedShortArray([0, 1, 2]);

var buffer = gl.createBuffer();
gl.bindBuffer(gl.ARRAY_BUFFER, buffer);
gl.bufferData(gl.ARRAY_BUFFER, data, gl.STATIC_DRAW);
var elements = gl.createBuffer();
gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, elements);
gl.bufferData(gl.ELEMENT_ARRAY_BUFFER, indices, gl.STATIC_DRAW);
gl.useProgram(program);
var vertexLoc = gl.getAttribLocation(program, "a_vertex");
var normalLoc = gl.getAttribLocation(program, "a_normal");
gl.vertexAttribPointer(vertexLoc, 4, gl.FLOAT, false, 7 * gl.sizeInBytes(gl.FLOAT), 0);
gl.enableVertexAttribArray(vertexLoc);
gl.vertexAttribPointer(normalLoc, 3, gl.FLOAT, false, 7 * gl.sizeInBytes(gl.FLOAT), 3 * gl.sizeInBytes(gl.FLOAT));
gl.enableVertexAttribArray(normalLoc);
shouldBe('gl.getError()', '0');
shouldBeUndefined('gl.drawElements(gl.TRIANGLES, 3, gl.UNSIGNED_SHORT, 0)');
shouldBe('gl.getError()', '0');

successfullyParsed = true;
