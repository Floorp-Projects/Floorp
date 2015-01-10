/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

module.metadata = {
  "stability": "unstable"
};

const { isFunction, isNull, isObject, isString,
        isRegExp, isArray, isDate, isPrimitive,
        isUndefined, instanceOf, source } = require("../lang/type");

/**
 * The `AssertionError` is defined in assert.
 * @extends Error
 * @example
 *  new assert.AssertionError({
 *    message: message,
 *    actual: actual,
 *    expected: expected
 *  })
 */
function AssertionError(options) {
  let assertionError = Object.create(AssertionError.prototype);

  if (isString(options))
    options = { message: options };
  if ("actual" in options)
    assertionError.actual = options.actual;
  if ("expected" in options)
    assertionError.expected = options.expected;
  if ("operator" in options)
    assertionError.operator = options.operator;

  assertionError.message = options.message;
  assertionError.stack = new Error().stack;
  return assertionError;
}
AssertionError.prototype = Object.create(Error.prototype, {
  constructor: { value: AssertionError },
  name: { value: "AssertionError", enumerable: true },
  toString: { value: function toString() {
    let value;
    if (this.message) {
      value = this.name + " : " + this.message;
    }
    else {
      value = [
        this.name + " : ",
        source(this.expected),
        this.operator,
        source(this.actual)
      ].join(" ");
    }
    return value;
  }}
});
exports.AssertionError = AssertionError;

function Assert(logger) {
  let assert = Object.create(Assert.prototype, { _log: { value: logger }});

  assert.fail = assert.fail.bind(assert);
  assert.pass = assert.pass.bind(assert);

  return assert;
}

Assert.prototype = {
  fail: function fail(e) {
    if (!e || typeof(e) !== 'object') {
      this._log.fail(e);
      return;
    }
    let message = e.message;
    try {
      if ('operator' in e) {
        message += [
          " -",
          source(e.actual),
          e.operator,
          source(e.expected)
        ].join(" ");
      }
    }
    catch(e) {}
    this._log.fail(message);
  },
  pass: function pass(message) {
    this._log.pass(message);
    return true;
  },
  error: function error(e) {
    this._log.exception(e);
  },
  ok: function ok(value, message) {
    if (!!!value) {
      this.fail({
        actual: value,
        expected: true,
        message: message,
        operator: "=="
      });
      return false;
    }

    this.pass(message);
    return true;
  },

  /**
   * The equality assertion tests shallow, coercive equality with `==`.
   * @example
   *    assert.equal(1, 1, "one is one");
   */
  equal: function equal(actual, expected, message) {
    if (actual == expected) {
      this.pass(message);
      return true;
    }

    this.fail({
      actual: actual,
      expected: expected,
      message: message,
      operator: "=="
    });
    return false;
  },

  /**
   * The non-equality assertion tests for whether two objects are not equal
   * with `!=`.
   * @example
   *    assert.notEqual(1, 2, "one is not two");
   */
  notEqual: function notEqual(actual, expected, message) {
    if (actual != expected) {
      this.pass(message);
      return true;
    }

    this.fail({
      actual: actual,
      expected: expected,
      message: message,
      operator: "!=",
    });
    return false;
  },

  /**
   * The equivalence assertion tests a deep (with `===`) equality relation.
   * @example
   *    assert.deepEqual({ a: "foo" }, { a: "foo" }, "equivalent objects")
   */
   deepEqual: function deepEqual(actual, expected, message) {
    if (isDeepEqual(actual, expected)) {
      this.pass(message);
      return true;
    }

    this.fail({
      actual: actual,
      expected: expected,
      message: message,
      operator: "deepEqual"
    });
    return false;
  },

  /**
   * The non-equivalence assertion tests for any deep (with `===`) inequality.
   * @example
   *    assert.notDeepEqual({ a: "foo" }, Object.create({ a: "foo" }),
   *                        "object's inherit from different prototypes");
   */
  notDeepEqual: function notDeepEqual(actual, expected, message) {
    if (!isDeepEqual(actual, expected)) {
      this.pass(message);
      return true;
    }

    this.fail({
      actual: actual,
      expected: expected,
      message: message,
      operator: "notDeepEqual"
    });
    return false;
  },

  /**
   * The strict equality assertion tests strict equality, as determined by
   * `===`.
   * @example
   *    assert.strictEqual(null, null, "`null` is `null`")
   */
  strictEqual: function strictEqual(actual, expected, message) {
    if (actual === expected) {
      this.pass(message);
      return true;
    }

    this.fail({
      actual: actual,
      expected: expected,
      message: message,
      operator: "==="
    });
    return false;
  },

  /**
   * The strict non-equality assertion tests for strict inequality, as
   * determined by `!==`.
   * @example
   *    assert.notStrictEqual(null, undefined, "`null` is not `undefined`");
   */
  notStrictEqual: function notStrictEqual(actual, expected, message) {
    if (actual !== expected) {
      this.pass(message);
      return true;
    }

    this.fail({
      actual: actual,
      expected: expected,
      message: message,
      operator: "!=="
    });
    return false;
  },

  /**
   * The assertion whether or not given `block` throws an exception. If optional
   * `Error` argument is provided and it's type of function thrown error is
   * asserted to be an instance of it, if type of `Error` is string then message
   * of throw exception is asserted to contain it.
   * @param {Function} block
   *    Function that is expected to throw.
   * @param {Error|RegExp} [Error]
   *    Error constructor that is expected to be thrown or a string that
   *    must be contained by a message of the thrown exception, or a RegExp
   *    matching a message of the thrown exception.
   * @param {String} message
   *    Description message
   *
   * @examples
   *
   *    assert.throws(function block() {
   *      doSomething(4)
   *    }, "Object is expected", "Incorrect argument is passed");
   *
   *    assert.throws(function block() {
   *      Object.create(5)
   *    }, TypeError, "TypeError is thrown");
   */
  throws: function throws(block, Error, message) {
    let threw = false;
    let exception = null;

    // If third argument is not provided and second argument is a string it
    // means that optional `Error` argument was not passed, so we shift
    // arguments.
    if (isString(Error) && isUndefined(message)) {
      message = Error;
      Error = undefined;
    }

    // Executing given `block`.
    try {
      block();
    }
    catch (e) {
      threw = true;
      exception = e;
    }

    // If exception was thrown and `Error` argument was not passed assert is
    // passed.
    if (threw && (isUndefined(Error) ||
                 // If passed `Error` is RegExp using it's test method to
                 // assert thrown exception message.
                 (isRegExp(Error) && (Error.test(exception.message) || Error.test(exception.toString()))) ||
                 // If passed `Error` is a constructor function testing if
                 // thrown exception is an instance of it.
                 (isFunction(Error) && instanceOf(exception, Error))))
    {
      this.pass(message);
      return true;
    }

    // Otherwise we report assertion failure.
    let failure = {
      message: message,
      operator: "matches"
    };

    if (exception) {
      failure.actual = exception.message || exception.toString();
    }

    if (Error) {
      failure.expected = Error.toString();
    }

    this.fail(failure);
    return false;
  }
};
exports.Assert = Assert;

function isDeepEqual(actual, expected) {
  // 7.1. All identical values are equivalent, as determined by ===.
  if (actual === expected) {
    return true;
  }

  // 7.2. If the expected value is a Date object, the actual value is
  // equivalent if it is also a Date object that refers to the same time.
  else if (isDate(actual) && isDate(expected)) {
    return actual.getTime() === expected.getTime();
  }

  // XXX specification bug: this should be specified
  else if (isPrimitive(actual) || isPrimitive(expected)) {
    return expected === actual;
  }

  // 7.3. Other pairs that do not both pass typeof value == "object",
  // equivalence is determined by ==.
  else if (!isObject(actual) && !isObject(expected)) {
    return actual == expected;
  }

  // 7.4. For all other Object pairs, including Array objects, equivalence is
  // determined by having the same number of owned properties (as verified
  // with Object.prototype.hasOwnProperty.call), the same set of keys
  // (although not necessarily the same order), equivalent values for every
  // corresponding key, and an identical "prototype" property. Note: this
  // accounts for both named and indexed properties on Arrays.
  else {
    return actual.prototype === expected.prototype &&
           isEquivalent(actual, expected);
  }
}

function isEquivalent(a, b, stack) {
  let aKeys = Object.keys(a);
  let bKeys = Object.keys(b);

  return aKeys.length === bKeys.length &&
          isArrayEquivalent(aKeys.sort(), bKeys.sort()) &&
          aKeys.every(function(key) {
            return isDeepEqual(a[key], b[key], stack)
          });
}

function isArrayEquivalent(a, b, stack) {
  return isArray(a) && isArray(b) &&
         a.every(function(value, index) {
           return isDeepEqual(value, b[index]);
         });
}
