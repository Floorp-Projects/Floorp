/*
Copyright (C) 2009 Apple Computer, Inc.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

description("Test of getActiveAttrib and getActiveUniform");

var context = create3DContext();
var context2 = create3DContext();
var program = loadStandardProgram(context);
var program2 = loadStandardProgram(context2);

glErrorShouldBe(context, context.NO_ERROR);
shouldBe("context.getActiveUniform(program, 0).name", "'u_modelViewProjMatrix'");
shouldBe("context.getActiveUniform(program, 0).type", "context.FLOAT_MAT4");
shouldBe("context.getActiveUniform(program, 0).size", "1");
shouldBe("context.getActiveUniform(program, 1)", "null");
glErrorShouldBe(context, context.INVALID_VALUE);
shouldBe("context.getActiveUniform(program, -1)", "null");
glErrorShouldBe(context, context.INVALID_VALUE);
shouldBe("context.getActiveUniform(null, 0)", "null");
glErrorShouldBe(context, context.INVALID_VALUE);

// we don't know the order the attribs will appear.
var info = [
  context.getActiveAttrib(program, 0),
  context.getActiveAttrib(program, 1)
];

var expected = [
  { name: 'a_normal', type: context.FLOAT_VEC3, size: 1 },
  { name: 'a_vertex', type: context.FLOAT_VEC4, size: 1 }
];

if (info[0].name != expected[0].name) {
  t = info[0];
  info[0] = info[1];
  info[1] = t;
}

for (var ii = 0; ii < info.length; ++ii) {
  shouldBe("info[ii].name", "expected[ii].name");
  shouldBe("info[ii].type", "expected[ii].type");
  shouldBe("info[ii].size", "expected[ii].size");
}
shouldBe("context.getActiveAttrib(program, 2)", "null");
glErrorShouldBe(context, context.INVALID_VALUE);
shouldBe("context.getActiveAttrib(program, -1)", "null");
glErrorShouldBe(context, context.INVALID_VALUE);
shouldBe("context.getActiveAttrib(null, 0)", "null");
glErrorShouldBe(context, context.INVALID_VALUE);

debug("Check trying to get attribs from different context");
shouldBe("context2.getActiveAttrib(program, 0)", "null");
glErrorShouldBe(context2, context2.INVALID_OPERATION);
shouldBe("context2.getActiveUniform(program, 0)", "null");
glErrorShouldBe(context2, context2.INVALID_OPERATION);

debug("Check trying to get attribs from deleted program");
context.deleteProgram(program);
shouldBe("context.getActiveUniform(program, 0)", "null");
glErrorShouldBe(context, context.INVALID_VALUE);
shouldBe("context.getActiveAttrib(program, 0)", "null");
glErrorShouldBe(context, context.INVALID_VALUE);

successfullyParsed = true;
