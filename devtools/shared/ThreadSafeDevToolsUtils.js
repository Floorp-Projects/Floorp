/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * General utilities used throughout devtools that can also be used in
 * workers.
 */

/**
 * Immutably reduce the given `...objs` into one object. The reduction is
 * applied from left to right, so `immutableUpdate({ a: 1 }, { a: 2 })` will
 * result in `{ a: 2 }`. The resulting object is frozen.
 *
 * Example usage:
 *
 *     const original = { foo: 1, bar: 2, baz: 3 };
 *     const modified = immutableUpdate(original, { baz: 0, bang: 4 });
 *
 *     // We get the new object that we expect...
 *     assert(modified.baz === 0);
 *     assert(modified.bang === 4);
 *
 *     // However, the original is not modified.
 *     assert(original.baz === 2);
 *     assert(original.bang === undefined);
 *
 * @param {...Object} ...objs
 * @returns {Object}
 */
exports.immutableUpdate = function(...objs) {
  return Object.freeze(Object.assign({}, ...objs));
};

/**
 * Utility function for updating an object with the properties of
 * other objects.
 *
 * DEPRECATED: Just use Object.assign() instead!
 *
 * @param aTarget Object
 *        The object being updated.
 * @param aNewAttrs Object
 *        The rest params are objects to update aTarget with. You
 *        can pass as many as you like.
 */
exports.update = function update(target, ...args) {
  for (const attrs of args) {
    for (const key in attrs) {
      const desc = Object.getOwnPropertyDescriptor(attrs, key);

      if (desc) {
        Object.defineProperty(target, key, desc);
      }
    }
  }
  return target;
};

/**
 * Utility function for getting the values from an object as an array
 *
 * @param object Object
 *        The object to iterate over
 */
exports.values = function values(object) {
  return Object.keys(object).map(k => object[k]);
};

/**
 * This is overridden in DevToolsUtils for the main thread, where we have the
 * Cu object available.
 */
exports.isCPOW = function() {
  return false;
};

/**
 * Report that |who| threw an exception, |exception|.
 */
exports.reportException = function reportException(who, exception) {
  const msg = `${who} threw an exception: ${exports.safeErrorString(exception)}`;
  dump(msg + "\n");

  if (typeof console !== "undefined" && console && console.error) {
    console.error(msg);
  }
};

/**
 * Given a handler function that may throw, return an infallible handler
 * function that calls the fallible handler, and logs any exceptions it
 * throws.
 *
 * @param handler function
 *      A handler function, which may throw.
 * @param aName string
 *      A name for handler, for use in error messages. If omitted, we use
 *      handler.name.
 *
 * (SpiderMonkey does generate good names for anonymous functions, but we
 * don't have a way to get at them from JavaScript at the moment.)
 */
exports.makeInfallible = function(handler, name = handler.name) {
  return function() {
    try {
      return handler.apply(this, arguments);
    } catch (ex) {
      let who = "Handler function";
      if (name) {
        who += " " + name;
      }
      exports.reportException(who, ex);
      return undefined;
    }
  };
};

/**
 * Turn the |error| into a string, without fail.
 *
 * @param {Error|any} error
 */
exports.safeErrorString = function(error) {
  try {
    let errorString = error.toString();
    if (typeof errorString == "string") {
      // Attempt to attach a stack to |errorString|. If it throws an error, or
      // isn't a string, don't use it.
      try {
        if (error.stack) {
          const stack = error.stack.toString();
          if (typeof stack == "string") {
            errorString += "\nStack: " + stack;
          }
        }
      } catch (ee) {
        // Ignore.
      }

      // Append additional line and column number information to the output,
      // since it might not be part of the stringified error.
      if (typeof error.lineNumber == "number" && typeof error.columnNumber == "number") {
        errorString += "Line: " + error.lineNumber + ", column: " + error.columnNumber;
      }

      return errorString;
    }
  } catch (ee) {
    // Ignore.
  }

  // We failed to find a good error description, so do the next best thing.
  return Object.prototype.toString.call(error);
};

/**
 * Interleaves two arrays element by element, returning the combined array, like
 * a zip. In the case of arrays with different sizes, undefined values will be
 * interleaved at the end along with the extra values of the larger array.
 *
 * @param Array a
 * @param Array b
 * @returns Array
 *          The combined array, in the form [a1, b1, a2, b2, ...]
 */
exports.zip = function(a, b) {
  if (!b) {
    return a;
  }
  if (!a) {
    return b;
  }
  const pairs = [];
  for (let i = 0, aLength = a.length, bLength = b.length;
       i < aLength || i < bLength;
       i++) {
    pairs.push([a[i], b[i]]);
  }
  return pairs;
};

/**
 * Converts an object into an array with 2-element arrays as key/value
 * pairs of the object. `{ foo: 1, bar: 2}` would become
 * `[[foo, 1], [bar 2]]` (order not guaranteed).
 *
 * @param object obj
 * @returns array
 */
exports.entries = function entries(obj) {
  return Object.keys(obj).map(k => [k, obj[k]]);
};

/*
 * Takes an array of 2-element arrays as key/values pairs and
 * constructs an object using them.
 */
exports.toObject = function(arr) {
  const obj = {};
  for (const [k, v] of arr) {
    obj[k] = v;
  }
  return obj;
};

/**
 * Composes the given functions into a single function, which will
 * apply the results of each function right-to-left, starting with
 * applying the given arguments to the right-most function.
 * `compose(foo, bar, baz)` === `args => foo(bar(baz(args)))`
 *
 * @param ...function funcs
 * @returns function
 */
exports.compose = function compose(...funcs) {
  return (...args) => {
    const initialValue = funcs[funcs.length - 1](...args);
    const leftFuncs = funcs.slice(0, -1);
    return leftFuncs.reduceRight((composed, f) => f(composed),
                                 initialValue);
  };
};

/**
 * Return true if `thing` is a generator function, false otherwise.
 */
exports.isGenerator = function(fn) {
  if (typeof fn !== "function") {
    return false;
  }
  const proto = Object.getPrototypeOf(fn);
  if (!proto) {
    return false;
  }
  const ctor = proto.constructor;
  if (!ctor) {
    return false;
  }
  return ctor.name == "GeneratorFunction";
};

/**
 * Return true if `thing` is an async function, false otherwise.
 */
exports.isAsyncFunction = function(fn) {
  if (typeof fn !== "function") {
    return false;
  }
  const proto = Object.getPrototypeOf(fn);
  if (!proto) {
    return false;
  }
  const ctor = proto.constructor;
  if (!ctor) {
    return false;
  }
  return ctor.name == "AsyncFunction";
};

/**
 * Return true if `thing` is a Promise or then-able, false otherwise.
 */
exports.isPromise = function(p) {
  return p && typeof p.then === "function";
};

/**
 * Return true if `thing` is a SavedFrame, false otherwise.
 */
exports.isSavedFrame = function(thing) {
  return Object.prototype.toString.call(thing) === "[object SavedFrame]";
};

/**
 * Return true iff `thing` is a `Set` object (possibly from another global).
 */
exports.isSet = function(thing) {
  return Object.prototype.toString.call(thing) === "[object Set]";
};

/**
 * Given a list of lists, flatten it. Only flattens one level; does not
 * recursively flatten all levels.
 *
 * @param {Array<Array<Any>>} lists
 * @return {Array<Any>}
 */
exports.flatten = function(lists) {
  return Array.prototype.concat.apply([], lists);
};

/**
 * Returns a promise that is resolved or rejected when all promises have settled
 * (resolved or rejected).
 *
 * This differs from Promise.all, which will reject immediately after the first
 * rejection, instead of waiting for the remaining promises to settle.
 *
 * @param values
 *        Iterable of promises that may be pending, resolved, or rejected. When
 *        when all promises have settled (resolved or rejected), the returned
 *        promise will be resolved or rejected as well.
 *
 * @return A new promise that is fulfilled when all values have settled
 *         (resolved or rejected). Its resolution value will be an array of all
 *         resolved values in the given order, or undefined if values is an
 *         empty array. The reject reason will be forwarded from the first
 *         promise in the list of given promises to be rejected.
 */
exports.settleAll = values => {
  if (values === null || typeof (values[Symbol.iterator]) != "function") {
    throw new Error("settleAll() expects an iterable.");
  }

  return new Promise((resolve, reject) => {
    values = Array.isArray(values) ? values : [...values];
    let countdown = values.length;
    const resolutionValues = new Array(countdown);
    let rejectionValue;
    let rejectionOccurred = false;

    if (!countdown) {
      resolve(resolutionValues);
      return;
    }

    function checkForCompletion() {
      if (--countdown > 0) {
        return;
      }
      if (!rejectionOccurred) {
        resolve(resolutionValues);
      } else {
        reject(rejectionValue);
      }
    }

    for (let i = 0; i < values.length; i++) {
      const index = i;
      const value = values[i];
      const resolver = result => {
        resolutionValues[index] = result;
        checkForCompletion();
      };
      const rejecter = error => {
        if (!rejectionOccurred) {
          rejectionValue = error;
          rejectionOccurred = true;
        }
        checkForCompletion();
      };

      if (value && typeof (value.then) == "function") {
        value.then(resolver, rejecter);
      } else {
        // Given value is not a promise, forward it as a resolution value.
        resolver(value);
      }
    }
  });
};
