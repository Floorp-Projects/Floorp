// // based on node assert, original notice:
// // NB: The URL to the CommonJS spec is kept just for tradition.
// //     node-assert has evolved a lot since then, both in API and behavior.
//
// // http://wiki.commonjs.org/wiki/Unit_Testing/1.0
// //
// // THIS IS NOT TESTED NOR LIKELY TO WORK OUTSIDE V8!
// //
// // Originally from narwhal.js (http://narwhaljs.org)
// // Copyright (c) 2009 Thomas Robinson <280north.com>
// //
// // Permission is hereby granted, free of charge, to any person obtaining a copy
// // of this software and associated documentation files (the 'Software'), to
// // deal in the Software without restriction, including without limitation the
// // rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// // sell copies of the Software, and to permit persons to whom the Software is
// // furnished to do so, subject to the following conditions:
// //
// // The above copyright notice and this permission notice shall be included in
// // all copies or substantial portions of the Software.
// //
// // THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// // IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// // FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// // AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
// // ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// // WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

"use strict";

var regex = /\s*function\s+([^\(\s]*)\s*/;

var functionsHaveNames = (function () {
  return function foo() {}.name === "foo";
}());

function assert(value, message) {
  if (!value) {
    fail(value, true, message, "==", assert.ok);
  }
}

assert.equal = function equal(actual, expected, message) {
  if (actual != expected) {
    fail(actual, expected, message, "==", assert.equal);
  }
};

assert.throws = function (block, error, message) {
  _throws(true, block, error, message);
};

function _throws(shouldThrow, block, expected, message) {
  var actual;

  if (typeof block !== "function") {
    throw new TypeError(`"block" argument must be a function`);
  }

  if (typeof expected === "string") {
    message = expected;
    expected = null;
  }

  actual = _tryBlock(block);

  message = (expected && expected.name ? " (" + expected.name + ")." : ".") +
            (message ? " " + message : ".");

  if (shouldThrow && !actual) {
    fail(actual, expected, "Missing expected exception" + message);
  }

  var userProvidedMessage = typeof message === "string";
  var isUnwantedException = !shouldThrow && isError(actual);
  var isUnexpectedException = !shouldThrow && actual && !expected;

  if ((isUnwantedException &&
      userProvidedMessage &&
      expectedException(actual, expected)) ||
      isUnexpectedException) {
    fail(actual, expected, "Got unwanted exception" + message);
  }

  if ((shouldThrow && actual && expected &&
      !expectedException(actual, expected)) || (!shouldThrow && actual)) {
    throw actual;
  }
}

function fail(actual, expected, message, operator, stackStartFunction) {
  throw new assert.AssertionError({
    message: message,
    actual: actual,
    expected: expected,
    operator: operator,
    stackStartFunction: stackStartFunction
  });
}

assert.fail = fail;

assert.AssertionError = function AssertionError(options) {
  this.name = "AssertionError";
  this.actual = options.actual;
  this.expected = options.expected;
  this.operator = options.operator;
  if (options.message) {
    this.message = options.message;
    this.generatedMessage = false;
  } else {
    this.message = getMessage(this);
    this.generatedMessage = true;
  }
  var stackStartFunction = options.stackStartFunction || fail;
  if (Error.captureStackTrace) {
    Error.captureStackTrace(this, stackStartFunction);
  } else {
    // non v8 browsers so we can have a stacktrace
    var err = new Error();
    if (err.stack) {
      var out = err.stack;

      // try to strip useless frames
      var fn_name = getName(stackStartFunction);
      var idx = out.indexOf("\n" + fn_name);
      if (idx >= 0) {
        // once we have located the function frame
        // we need to strip out everything before it (and its line)
        var next_line = out.indexOf("\n", idx + 1);
        out = out.substring(next_line + 1);
      }

      this.stack = out;
    }
  }
};

function expectedException(actual, expected) {
  if (!actual || !expected) {
    return false;
  }

  if (Object.prototype.toString.call(expected) == "[object RegExp]") {
    return expected.test(actual);
  }

  try {
    if (actual instanceof expected) {
      return true;
    }
  } catch (e) {
    // Ignore.  The instanceof check doesn"t work for arrow functions.
  }

  if (Error.isPrototypeOf(expected)) {
    return false;
  }

  return expected.call({}, actual) === true;
}

function _tryBlock(block) {
  var error;
  try {
    block();
  } catch (e) {
    error = e;
  }
  return error;
}

function isError(obj) {
  return obj instanceof Error;
}

function isFunction(value) {
  return typeof value === "function";
}

function getMessage(self) {
  return truncate(inspect(self.actual), 128) + " " +
         self.operator + " " +
         truncate(inspect(self.expected), 128);
}

function getName(func) {
  if (!isFunction(func)) {
    return null;
  }
  if (functionsHaveNames) {
    return func.name;
  }
  var str = func.toString();
  var match = str.match(regex);
  return match && match[1];
}

function truncate(s, n) {
  if (typeof s === "string") {
    return s.length < n ? s : s.slice(0, n);
  }
  return s;
}

function inspect(something) {
  if (functionsHaveNames || !isFunction(something)) {
    throw new Error(something);
  }
  var rawname = getName(something);
  var name = rawname ? ": " + rawname : "";
  return "[Function" + name + "]";
}

exports.assert = assert;
