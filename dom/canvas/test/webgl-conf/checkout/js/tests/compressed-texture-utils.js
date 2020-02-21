/*
Copyright (c) 2019 The Khronos Group Inc.
Use of this source code is governed by an MIT-style license that can be
found in the LICENSE.txt file.
*/

"use strict";

var CompressedTextureUtils = (function() {

var formatToString = function(ext, format) {
    for (var p in ext) {
        if (ext[p] == format) {
            return p;
        }
    }
    return "0x" + format.toString(16);
};

/**
 * Make an image element from Uint8Array bitmap data.
 * @param {number} imageHeight Height of the data in pixels.
 * @param {number} imageWidth Width of the data in pixels.
 * @param {number} dataWidth Width of each row in the data buffer, in pixels.
 * @param {Uint8Array} data Image data buffer to display. Each pixel takes up 4 bytes in the array regardless of the alpha parameter.
 * @param {boolean} alpha True if alpha data should be taken from data. Otherwise alpha channel is set to 255.
 * @return {HTMLImageElement} The image element.
 */
var makeScaledImage = function(imageWidth, imageHeight, dataWidth, data, alpha, opt_scale) {
    var scale = opt_scale ? opt_scale : 8;
    var c = document.createElement("canvas");
    c.width = imageWidth * scale;
    c.height = imageHeight * scale;
    var ctx = c.getContext("2d");
    for (var yy = 0; yy < imageHeight; ++yy) {
        for (var xx = 0; xx < imageWidth; ++xx) {
            var offset = (yy * dataWidth + xx) * 4;
            ctx.fillStyle = "rgba(" +
                    data[offset + 0] + "," +
                    data[offset + 1] + "," +
                    data[offset + 2] + "," +
                    (alpha ? data[offset + 3] / 255 : 1) + ")";
            ctx.fillRect(xx * scale, yy * scale, scale, scale);
        }
    }
    return wtu.makeImageFromCanvas(c);
};

var insertCaptionedImg = function(parent, caption, img) {
    var div = document.createElement("div");
    div.appendChild(img);
    var label = document.createElement("div");
    label.appendChild(document.createTextNode(caption));
    div.appendChild(label);
    parent.appendChild(div);
};

/**
 * @param {WebGLRenderingContextBase} gl
 * @param {Object} compressedFormats Mapping from format names to format enum values.
 * @param expectedByteLength A function that takes in width, height and format and returns the expected buffer size in bytes.
 */
var testCompressedFormatsUnavailableWhenExtensionDisabled = function(gl, compressedFormats, expectedByteLength, testSize) {
    var tex = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_2D, tex);
    for (var name in compressedFormats) {
        if (compressedFormats.hasOwnProperty(name)) {
            gl.compressedTexImage2D(gl.TEXTURE_2D, 0, compressedFormats[name], testSize, testSize, 0, new Uint8Array(expectedByteLength(testSize, testSize, compressedFormats[name])));
            wtu.glErrorShouldBe(gl, gl.INVALID_ENUM, "Trying to use format " + name + " with extension disabled.");
        }
    }
    gl.bindTexture(gl.TEXTURE_2D, null);
    gl.deleteTexture(tex);
};

/**
 * @param {WebGLRenderingContextBase} gl
 * @param {Object} expectedFormats Mapping from format names to format enum values.
 */
var testCompressedFormatsListed = function(gl, expectedFormats) {
    debug("");
    debug("Testing that every format is listed by the compressed texture formats query");

    var supportedFormats = gl.getParameter(gl.COMPRESSED_TEXTURE_FORMATS);

    var failed;
    var count = 0;
    for (var name in expectedFormats) {
        if (expectedFormats.hasOwnProperty(name)) {
            ++count;
            var format = expectedFormats[name];
            failed = true;
            for (var ii = 0; ii < supportedFormats.length; ++ii) {
                if (format == supportedFormats[ii]) {
                    testPassed("supported format " + name + " exists");
                    failed = false;
                    break;
                }
            }
            if (failed) {
                testFailed("supported format " + name + " does not exist");
            }
        }
    }
    if (supportedFormats.length != count) {
        testFailed("Incorrect number of supported formats, was " + supportedFormats.length + " should be " + count);
    }
};

/**
 * @param {Object} ext Compressed texture extension object.
 * @param {Object} expectedFormats Mapping from format names to format enum values.
 */
var testCorrectEnumValuesInExt = function(ext, expectedFormats) {
    debug("");
    debug("Testing that format enum values in the extension object are correct");

    for (name in expectedFormats) {
        if (expectedFormats.hasOwnProperty(name)) {
            if (isResultCorrect(ext[name], expectedFormats[name])) {
                testPassed("Enum value for " + name + " matches 0x" + ext[name].toString(16));
            } else {
                testFailed("Enum value for " + name + " mismatch: 0x" + ext[name].toString(16) + " should be 0x" + expectedFormats[name].toString(16));
            }
        }
    }
};

/**
 * @param {WebGLRenderingContextBase} gl
 * @param {Object} validFormats Mapping from format names to format enum values.
 * @param expectedByteLength A function that takes in width, height and format and returns the expected buffer size in bytes.
 * @param getBlockDimensions A function that takes in a format and returns block size in pixels.
 */
var testFormatRestrictionsOnBufferSize = function(gl, validFormats, expectedByteLength, getBlockDimensions) {
    debug("");
    debug("Testing format restrictions on texture upload buffer size");

    var tex = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_2D, tex);
    for (var formatId in validFormats) {
        if (validFormats.hasOwnProperty(formatId)) {
            var format = validFormats[formatId];
            var blockSize = getBlockDimensions(format);
            var expectedSize = expectedByteLength(blockSize.width * 4, blockSize.height * 4, format);
            var data = new Uint8Array(expectedSize);
            gl.compressedTexImage2D(gl.TEXTURE_2D, 0, format, blockSize.width * 3, blockSize.height * 4, 0, data);
            wtu.glErrorShouldBe(gl, gl.INVALID_VALUE, formatId + " data size does not match dimensions (too small width)");
            gl.compressedTexImage2D(gl.TEXTURE_2D, 0, format, blockSize.width * 5, blockSize.height * 4, 0, data);
            wtu.glErrorShouldBe(gl, gl.INVALID_VALUE, formatId + " data size does not match dimensions (too large width)");
            gl.compressedTexImage2D(gl.TEXTURE_2D, 0, format, blockSize.width * 4, blockSize.height * 3, 0, data);
            wtu.glErrorShouldBe(gl, gl.INVALID_VALUE, formatId + " data size does not match dimensions (too small height)");
            gl.compressedTexImage2D(gl.TEXTURE_2D, 0, format, blockSize.width * 4, blockSize.height * 5, 0, data);
            wtu.glErrorShouldBe(gl, gl.INVALID_VALUE, formatId + " data size does not match dimensions (too large height)");
        }
    }
};

/**
 * @param {WebGLRenderingContextBase} gl
 * @param {Object} validFormats Mapping from format names to format enum values.
 * @param expectedByteLength A function that takes in width, height and format and returns the expected buffer size in bytes.
 * @param getBlockDimensions A function that takes in a format and returns block size in pixels.
 * @param {number} width Width of the image in pixels.
 * @param {number} height Height of the image in pixels.
 * @param {Object} subImageConfigs configs for compressedTexSubImage calls
 */
var testTexSubImageDimensions = function(gl, validFormats, expectedByteLength, getBlockDimensions, width, height, subImageConfigs) {
    var tex = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_2D, tex);

    for (var formatId in validFormats) {
        if (validFormats.hasOwnProperty(formatId)) {
            var format = validFormats[formatId];
            var blockSize = getBlockDimensions(format);
            var expectedSize = expectedByteLength(width, height, format);
            var data = new Uint8Array(expectedSize);

            gl.compressedTexImage2D(gl.TEXTURE_2D, 0, format, width, height, 0, data);
            wtu.glErrorShouldBe(gl, gl.NO_ERROR, "setting up compressed texture");

            for (let i = 0, len = subImageConfigs.length; i < len; ++i) {
                let c = subImageConfigs[i];
                var subData = new Uint8Array(expectedByteLength(c.width, c.height, format));
                gl.compressedTexSubImage2D(gl.TEXTURE_2D, 0, c.xoffset, c.yoffset, c.width, c.height, format, subData);
                wtu.glErrorShouldBe(gl, c.expectation, c.message);
            }
        }
    }

    gl.bindTexture(gl.TEXTURE_2D, null);
    gl.deleteTexture(tex);
};

return {
    formatToString: formatToString,
    insertCaptionedImg: insertCaptionedImg,
    makeScaledImage: makeScaledImage,
    testCompressedFormatsListed: testCompressedFormatsListed,
    testCompressedFormatsUnavailableWhenExtensionDisabled: testCompressedFormatsUnavailableWhenExtensionDisabled,
    testCorrectEnumValuesInExt: testCorrectEnumValuesInExt,
    testFormatRestrictionsOnBufferSize: testFormatRestrictionsOnBufferSize,
    testTexSubImageDimensions: testTexSubImageDimensions,
};

})();