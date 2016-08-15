/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// A few wrappers for stack-manipulation.  This version of the module
// is used in chrome code.

"use strict";

(function (factory) {
  // This file might be require()d, but might also be loaded via
  // Cu.import.  Account for the differences here.
  if (this.module && module.id.indexOf("stack") >= 0) {
    // require.
    const {components, Cu} = require("chrome");
    factory.call(this, components, Cu, exports);
  } else {
    // Cu.import.
    this.isWorker = false;
    factory.call(this, Components, Components.utils, this);
    this.EXPORTED_SYMBOLS = ["callFunctionWithAsyncStack", "describeNthCaller",
                             "getStack"];
  }
}).call(this, function (components, Cu, exports) {
  /**
   * Return a description of the Nth caller, suitable for logging.
   *
   * @param {Number} n the caller to describe
   * @return {String} a description of the nth caller.
   */
  function describeNthCaller(n) {
    if (isWorker) {
      return "";
    }

    let caller = components.stack;
    // Do one extra iteration to skip this function.
    while (n >= 0) {
      --n;
      caller = caller.caller;
    }

    let func = caller.name;
    let file = caller.filename;
    if (file.includes(" -> ")) {
      file = caller.filename.split(/ -> /)[1];
    }
    let path = file + ":" + caller.lineNumber;

    return func + "() -> " + path;
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
  exports.describeNthCaller = describeNthCaller;
  exports.getStack = getStack;
});
