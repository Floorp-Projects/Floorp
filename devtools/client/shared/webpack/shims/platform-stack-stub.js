/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// A few wrappers for stack-manipulation.  This version of the module
// is used in content code.  Note that this particular copy of the
// file can only be loaded via require(), because Cu.import doesn't
// exist in the content case.  So, we don't need the code to handle
// both require and import here.

"use strict";

/**
 * Looks like Cu.callFunctionWithAsyncStack, but just calls the callee.
 */
function callFunctionWithAsyncStack(callee, stack, id) {
  return callee();
}

/**
 * Return the Nth path from the stack excluding substr.
 *
 * @param {Number}
 *        n the Nth path from the stack to describe.
 * @param {String} substr
 *        A segment of the path that should be excluded.
 */
function getNthPathExcluding(n, substr) {
  if (isWorker) {
    return "";
  }

  let stack = new Error().stack.split("\n");
  stack = stack.filter(line => {
    return line && !line.includes(substr);
  });
  return stack[n + 1];
}

/**
 * Return a stack object that can be serialized and, when
 * deserialized, passed to callFunctionWithAsyncStack.
 */
function getStack() {
  // There's no reason for this to do anything fancy, since it's only
  // used to pass back into callFunctionWithAsyncStack, which we can't
  // implement.
  return null;
}

exports.callFunctionWithAsyncStack = callFunctionWithAsyncStack;
exports.getNthPathExcluding = getNthPathExcluding;
exports.getStack = getStack;
