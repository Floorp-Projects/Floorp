'use strict';

const trivialVsSrc = `
void main()
{
    gl_Position = vec4(0,0,0,1);
}
`;
const trivialFsSrc = `
void main()
{
    gl_FragColor = vec4(0,1,0,1);
}
`;
const trivialVsMrtSrc = `#version 300 es
void main()
{
    gl_Position = vec4(0,0,0,1);
}
`;
const trivialFsMrtSrc = `#version 300 es
precision mediump float;
layout(location = 0) out vec4 o_color0;
layout(location = 1) out vec4 o_color1;
void main()
{
    o_color0 = vec4(1, 0, 0, 1);
    o_color1 = vec4(0, 1, 0, 1);
}
`;

function testExtFloatBlend(internalFormat) {
    const shouldBlend = gl.getSupportedExtensions().indexOf('EXT_float_blend') != -1;

    const prog = wtu.setupProgram(gl, [trivialVsSrc, trivialFsSrc]);
    gl.useProgram(prog);

    const tex = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_2D, tex);
    gl.texImage2D(gl.TEXTURE_2D, 0, internalFormat, 1, 1, 0, gl.RGBA, gl.FLOAT, null);

    const fb = gl.createFramebuffer();
    gl.bindFramebuffer(gl.FRAMEBUFFER, fb);
    gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_2D, tex, 0);
    shouldBe('gl.checkFramebufferStatus(gl.FRAMEBUFFER)', 'gl.FRAMEBUFFER_COMPLETE');

    gl.disable(gl.BLEND);
    gl.drawArrays(gl.POINTS, 0, 1);
    wtu.glErrorShouldBe(gl, 0, 'Float32 draw target without blending');

    gl.enable(gl.BLEND);
    gl.drawArrays(gl.POINTS, 0, 1);
    wtu.glErrorShouldBe(gl, shouldBlend ? 0 : gl.INVALID_OPERATION,
                        'Float32 blending is ' + (shouldBlend ? '' : 'not ') + 'allowed ');

    gl.deleteFramebuffer(fb);
    gl.deleteTexture(tex);
}

function testExtFloatBlendMRT() {
    const shouldBlend = gl.getSupportedExtensions().indexOf('EXT_float_blend') != -1;

    const prog = wtu.setupProgram(gl, [trivialVsMrtSrc, trivialFsMrtSrc]);
    gl.useProgram(prog);

    const tex1 = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_2D, tex1);
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, 1, 1, 0, gl.RGBA, gl.UNSIGNED_BYTE, null);

    const tex2 = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_2D, tex2);
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, 1, 1, 0, gl.RGBA, gl.UNSIGNED_BYTE, null);

    const texF1 = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_2D, texF1);
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA32F, 1, 1, 0, gl.RGBA, gl.FLOAT, null);

    const texF2 = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_2D, texF2);
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA32F, 1, 1, 0, gl.RGBA, gl.FLOAT, null);

    const fb = gl.createFramebuffer();
    gl.bindFramebuffer(gl.FRAMEBUFFER, fb);
    gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_2D, tex1, 0);
    gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT1, gl.TEXTURE_2D, tex2, 0);
    shouldBe('gl.checkFramebufferStatus(gl.FRAMEBUFFER)', 'gl.FRAMEBUFFER_COMPLETE');

    gl.drawBuffers([gl.COLOR_ATTACHMENT0, gl.COLOR_ATTACHMENT1]);

    gl.enable(gl.BLEND);

    gl.drawArrays(gl.POINTS, 0, 1);
    wtu.glErrorShouldBe(gl, 0, 'No Float32 color attachment');

    gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_2D, texF1, 0);
    gl.drawArrays(gl.POINTS, 0, 1);
    wtu.glErrorShouldBe(gl, shouldBlend ? 0 : gl.INVALID_OPERATION,
                        'Float32 blending is ' + (shouldBlend ? '' : 'not ') + 'allowed ');

    gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT1, gl.TEXTURE_2D, texF2, 0);
    gl.drawArrays(gl.POINTS, 0, 1);
    wtu.glErrorShouldBe(gl, shouldBlend ? 0 : gl.INVALID_OPERATION,
                        'Float32 blending is ' + (shouldBlend ? '' : 'not ') + 'allowed ');

    gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_2D, tex1, 0);
    gl.drawArrays(gl.POINTS, 0, 1);
    wtu.glErrorShouldBe(gl, shouldBlend ? 0 : gl.INVALID_OPERATION,
                        'Float32 blending is ' + (shouldBlend ? '' : 'not ') + 'allowed ');

    gl.drawBuffers([gl.COLOR_ATTACHMENT0]);
    shouldBe('gl.checkFramebufferStatus(gl.FRAMEBUFFER)', 'gl.FRAMEBUFFER_COMPLETE');

    gl.drawArrays(gl.POINTS, 0, 1);
    wtu.glErrorShouldBe(gl, 0, 'Float32 color attachment draw buffer is not enabled');

    gl.drawBuffers([gl.COLOR_ATTACHMENT0, gl.COLOR_ATTACHMENT1]);
    gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT1, gl.TEXTURE_2D, tex2, 0);
    shouldBe('gl.checkFramebufferStatus(gl.FRAMEBUFFER)', 'gl.FRAMEBUFFER_COMPLETE');

    gl.drawArrays(gl.POINTS, 0, 1);
    wtu.glErrorShouldBe(gl, 0, 'No Float32 color attachment');

    gl.deleteFramebuffer(fb);
    gl.deleteTexture(tex1);
    gl.deleteTexture(tex2);
    gl.deleteTexture(texF1);
    gl.deleteTexture(texF2);
}

function testExtFloatBlendNonFloat32Type() {
    const shouldBlend = gl.getSupportedExtensions().indexOf('EXT_float_blend') != -1;

    const prog = wtu.setupProgram(gl, [trivialVsSrc, trivialFsSrc]);
    gl.useProgram(prog);

    gl.enable(gl.BLEND);

    const tex = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_2D, tex);
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA32F, 1, 1, 0, gl.RGBA, gl.FLOAT, null);

    const fb = gl.createFramebuffer();
    gl.bindFramebuffer(gl.FRAMEBUFFER, fb);
    gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_2D, tex, 0);
    shouldBe('gl.checkFramebufferStatus(gl.FRAMEBUFFER)', 'gl.FRAMEBUFFER_COMPLETE');

    gl.drawArrays(gl.POINTS, 0, 1);
    wtu.glErrorShouldBe(gl, shouldBlend ? 0 : gl.INVALID_OPERATION,
                        'Float32 blending is ' + (shouldBlend ? '' : 'not ') + 'allowed ');

    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, 1, 1, 0, gl.RGBA, gl.UNSIGNED_BYTE, null);
    gl.drawArrays(gl.POINTS, 0, 1);
    wtu.glErrorShouldBe(gl, 0, 'UNSIGNED_BYTE should blend anyway');

    const formats = [
        [gl.RGBA16F, gl.RGBA, gl.HALF_FLOAT],
        [gl.RGBA16F, gl.RGBA, gl.FLOAT],
        [gl.RG16F, gl.RG, gl.FLOAT],
        [gl.R16F, gl.RED, gl.FLOAT],
        [gl.R11F_G11F_B10F, gl.RGB, gl.FLOAT]
    ];

    for (let i = 0, len = formats.length; i < len; i++) {
        gl.texImage2D(gl.TEXTURE_2D, 0, formats[i][0], 1, 1, 0, formats[i][1], formats[i][2], null);
        gl.drawArrays(gl.POINTS, 0, 1);
        wtu.glErrorShouldBe(gl, 0, 'Any other float type which is not 32-bit-Float should blend anyway');
    }

    gl.deleteFramebuffer(fb);
    gl.deleteTexture(tex);
}

/*
** Copyright (c) 2013 The Khronos Group Inc.
**
** Permission is hereby granted, free of charge, to any person obtaining a
** copy of this software and/or associated documentation files (the
** 'Materials'), to deal in the Materials without restriction, including
** without limitation the rights to use, copy, modify, merge, publish,
** distribute, sublicense, and/or sell copies of the Materials, and to
** permit persons to whom the Materials are furnished to do so, subject to
** the following conditions:
**
** The above copyright notice and this permission notice shall be included
** in all copies or substantial portions of the Materials.
**
** THE MATERIALS ARE PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND,
** EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
** MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
** IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
** CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
** TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
** MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.
*/
