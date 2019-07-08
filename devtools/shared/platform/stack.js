/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// A few wrappers for stack-manipulation.  This version of the module
// is used in chrome code.

"use strict";

const { Cu, components } = require("chrome");

/**
 * Return the Nth path from the stack excluding substr.
 *
 * @param {Number}
 *        n the Nth path from the stack to describe.
 * @param {String} substr
 *        A segment of the path that should be excluded.
 */
function getNthPathExcluding(n, substr) {
  let stack = components.stack.formattedStack.split("\n");

  // Remove this method from the stack
  stack = stack.splice(1);

  stack = stack.map(line => {
    if (line.includes(" -> ")) {
      return line.split(" -> ")[1];
    }
    return line;
  });

  if (substr) {
    stack = stack.filter(line => {
      return line && !line.includes(substr);
    });
  }

  if (!stack[n]) {
    n = 0;
  }
  return stack[n] || "";
}

/**
 * Return a stack object that can be serialized and, when
 * deserialized, passed to callFunctionWithAsyncStack.
 */
function getStack() {
  return components.stack.caller;
}

/**
 * Like Cu.callFunctionWithAsyncStack but handles the isWorker case
 * -- |Cu| isn't defined in workers.
 */
function callFunctionWithAsyncStack(callee, stack, id) {
  if (isWorker) {
    return callee();
  }
  return Cu.callFunctionWithAsyncStack(callee, stack, id);
}

exports.callFunctionWithAsyncStack = callFunctionWithAsyncStack;
exports.getNthPathExcluding = getNthPathExcluding;
exports.getStack = getStack;
