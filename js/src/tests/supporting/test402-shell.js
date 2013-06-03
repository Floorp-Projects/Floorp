/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Test402 tests pass unless they throw. Note that this isn't true for all
 * Test262 test cases - for the ones marked @negative the logic is inverted.
 */
testPassesUnlessItThrows();

/*
 * Test262 function $ERROR throws an error with the message provided. Test262
 * test cases call it to indicate failure.
 */
function $ERROR(msg) {
    throw new Error("Test402 error: " + msg);
}

/*
 * Test262 function $INCLUDE loads a file with support functions for the tests.
 * This function is replaced in browser.js.
 */
function $INCLUDE(file) {
    loadRelativeToScript("lib/" + file);
}

/*
 * Test262 function fnGlobalObject returns the global object.
 */
var __globalObject = Function("return this;")();
function fnGlobalObject() {
     return __globalObject;
}
