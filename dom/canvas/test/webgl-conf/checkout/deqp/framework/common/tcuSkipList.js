/*
** Copyright (c) 2016 The Khronos Group Inc.
**
** Permission is hereby granted, free of charge, to any person obtaining a
** copy of this software and/or associated documentation files (the
** "Materials"), to deal in the Materials without restriction, including
** without limitation the rights to use, copy, modify, merge, publish,
** distribute, sublicense, and/or sell copies of the Materials, and to
** permit persons to whom the Materials are furnished to do so, subject to
** the following conditions:
**
** The above copyright notice and this permission notice shall be included
** in all copies or substantial portions of the Materials.
**
** THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
** EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
** MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
** IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
** CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
** TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
** MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.
*/

/**
 * This class defines the individual tests which are skipped because
 * of graphics driver bugs which simply can not be worked around in
 * WebGL 2.0 implementations.
 *
 * The intent is that this list be kept as small as possible; and that
 * bugs are filed with the respective GPU vendors for entries in this
 * list.
 *
 * Pass the query argument "runSkippedTests" in the URL in order to
 * force the skipped tests to be run. So, for example:
 *
 * http://localhost:8080/sdk/tests/deqp/functional/gles3/transformfeedback.html?filter=transform_feedback.basic_types.separate.points&runSkippedTests
 */
'use strict';
goog.provide('framework.common.tcuSkipList');

goog.scope(function() {

    var tcuSkipList = framework.common.tcuSkipList;

    var _skipEntries = {};
    var _reason = "";

    function _setReason(reason) {
        _reason = reason;
    }

    function _skip(testName) {
        _skipEntries[testName] = _reason;
    }

    var runSkippedTests = false;
    var queryVars = window.location.search.substring(1).split('&');
    for (var i = 0; i < queryVars.length; i++) {
        var value = queryVars[i].split('=');
        if (decodeURIComponent(value[0]) === 'runSkippedTests') {
            // Assume that presence of this query arg implies to run
            // the skipped tests; the value is ignored.
            runSkippedTests = true;
            break;
        }
    }

    if (!runSkippedTests) {
        // Example usage:
        //
        // _setReason("Bugs in FooVendor 30.03 driver");
        // _skip("transform_feedback.basic_types.separate.points.lowp_mat2");

        // Please see https://android.googlesource.com/platform/external/deqp/+/7c5323116bb164d64bfecb68e8da1af634317b24
        _setReason("Native dEQP also fails on these tests and suppresses them");
        _skip("texture_functions.textureoffset.sampler3d_fixed_fragment");
        _skip("texture_functions.textureoffset.isampler3d_fragment");
        _skip("texture_functions.textureoffset.usampler3d_fragment");
        _skip("texture_functions.textureprojoffset.sampler3d_fixed_fragment");
        _skip("texture_functions.textureprojoffset.isampler3d_fragment");
        _skip("texture_functions.textureprojoffset.usampler3d_fragment");
        // Please see https://android.googlesource.com/platform/external/deqp/+/master/android/cts/master/src/gles3-test-issues.txt
        _skip("texture_functions.textureprojlodoffset.usampler3d_vertex");
        _skip("texture_functions.textureoffset.sampler3d_float_fragment");
        _skip("texture_functions.textureprojoffset.sampler3d_float_fragment");
        // Please see https://android.googlesource.com/platform/external/deqp/+/master/android/cts/master/src/gles3-driver-issues.txt
        _skip("texture_functions.texturegrad.samplercubeshadow_vertex");
        _skip("texture_functions.texturegrad.samplercubeshadow_fragment");
    } // if (!runSkippedTests)

    /*
     * Gets the skip status of the given test. Returns an
     * object with the properties "skip", a boolean, and "reason", a
     * string.
     */
    tcuSkipList.getSkipStatus = function(testName) {
        var skipEntry = _skipEntries[testName];
        if (skipEntry === undefined) {
            return { 'skip': false, 'reason': '' };
        } else {
            return { 'skip': true, 'reason': skipEntry };
        }
    }

});
