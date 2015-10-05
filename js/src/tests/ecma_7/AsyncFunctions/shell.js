/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * These methods are inspired by chai-as-promised library for promise testing
 * in JS applications. They check if promises eventually resolve to a given value
 * or are rejected with a specified error type.
 */

if (typeof assertEventuallyEq === 'undefined') {
  assertEventuallyEq = function(promise, expected) {
    return promise.then(actual => assertEq(actual, expected));
  };
}

if (typeof assertEventuallyThrows === 'undefined') {
  assertEventuallyThrows = function(promise, expectedErrorType) {
    return promise.catch(actualE => assertEq(actualE instanceof expectedErrorType, true));
  };
}

if (typeof assertEventuallyDeepEq === 'undefined') {
  assertEventuallyDeepEq = function(promise, expected) {
    return promise.then(actual => assertDeepEq(actual, expected));
  };
}
