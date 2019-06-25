/*
 * This file is meant to provide common infrastructure for several consumers.
 * The consumer is expected to define the following things:
 *
 * 1) A verifyPromiseGlobal function which does whatever test the consumer
 *    wants.  This function is passed a promise and the global whose
 *    TestFunctions was used to get the promise.
 * 2) A expectedExceptionGlobal function which is handed the global whose
 *    TestFunctions was used to trigger the exception and should return the
 *    global the exception is expected to live in.
 * 3) A subframe (frames[0]) which can be used as a second global for creating
 *    promises.
 */

/* global verifyPromiseGlobal, expectedExceptionGlobal */

var label = "parent";

function testThrownException(global) {
  var p = global.TestFunctions.throwToRejectPromise();
  verifyPromiseGlobal(p, global, "throwToRejectPromise return value");
  return p.then(() => {}).catch((err) => {
    var expected = expectedExceptionGlobal(global);
    is(SpecialPowers.unwrap(SpecialPowers.Cu.getGlobalForObject(err)),
       expected,
       "Should have an exception object from the right global too");
    ok(err instanceof expected.DOMException,
       "Should have a DOMException here");
    is(Object.getPrototypeOf(err), expected.DOMException.prototype,
       "Should have a DOMException from the right global");
    is(err.name, "InvalidStateError", "Should have the right DOMException");
  });
}

function runPromiseRetvalTests(finishFunc) {
  Promise.resolve()
    .then(testThrownException.bind(undefined, window))
    .then(testThrownException.bind(undefined, frames[0]))
    .then(finishFunc)
    .catch(function(e) {
      ok(false, `Exception thrown: ${e}@${location.pathname}:${e.lineNumber}:${e.columnNumber}`);
      finishFunc();
    });
}
