/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * The current crop of Test262 test cases that we run are expected to pass
 * unless they crash or throw.  (This isn't true for all Test262 test cases --
 * for the ones marked @negative the logic is inverted.  We'll have to deal with
 * that concern eventually, but for now we're punting so we can run subsets of
 * Test262 tests.)
 */
testPassesUnlessItThrows();

/*
 * Test262 function $ERROR throws an error with the message provided. Test262
 * test cases call it to indicate failure.
 */
function $ERROR(msg)
{
  throw new Error("Test262 error: " + msg);
}

/*
 * Test262 function $INCLUDE loads a file with support functions for the tests.
 * This function is replaced in browser.js.
 */
function $INCLUDE(file)
{
  load("supporting/" + file);
}

/*
 * Test262 function fnGlobalObject returns the global object.
 */
var fnGlobalObject = (function()
{
  var global = Function("return this")();
  return function fnGlobalObject() { return global; };
})();
