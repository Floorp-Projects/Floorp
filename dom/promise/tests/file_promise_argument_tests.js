/*
 * This file is meant to provide common infrastructure for several consumers.
 * The consumer is expected to define the following things:
 *
 * 1) An verifyPromiseGlobal function which does whatever test the consumer
 *    wants.
 * 2) An isXrayArgumentTest global boolean, because some of these tests act
 *    differenly based on that boolean.
 * 3) A function named getPromise.  This function is given a global object and a
 *    single argument to use for getting the promise.  The getPromise function
 *    is expected to trigger the canonical Promise.resolve for the given global
 *    with the given argument in some way that depends on the test, and return
 *    the result.
 * 4) A subframe (frames[0]) which can be used as a second global for creating
 *    promises.
 */

/* global verifyPromiseGlobal, getPromise, isXrayArgumentTest */

var label = "parent";

function passBasicPromise() {
  var p1 = Promise.resolve();
  verifyPromiseGlobal(p1, window, "Promise.resolve return value 1");
  var p2 = getPromise(window, p1);
  is(p1, p2, "Basic promise should just pass on through");
  return p2;
}

function passPrimitive(global) {
  var p = getPromise(global, 5);
  verifyPromiseGlobal(p, global, "Promise wrapping primitive");
  return p.then(function(arg) {
    is(arg, 5, "Should have the arg we passed in");
  });
}

function passThenable(global) {
  var called = false;
  var thenable = {
    then(f) {
      called = true;
      f(7);
    },
  };
  var p = getPromise(global, thenable);
  verifyPromiseGlobal(p, global, "Promise wrapping thenable");
  return p.then(function(arg) {
    ok(called, "Thenable should have been called");
    is(arg, 7, "Should have the arg our thenable passed in");
  });
}

function passWrongPromiseWithMatchingConstructor() {
  var p1 = Promise.resolve();
  verifyPromiseGlobal(p1, window, "Promise.resolve() return value 2");
  p1.constructor = frames[0].Promise;
  var p2 = getPromise(frames[0], p1);
  // The behavior here will depend on whether we're touching frames[0] via Xrays
  // or not.  If we are not, the current compartment while getting our promise
  // will be that of frames[0].  If we are, it will be our window's compartment.
  if (isXrayArgumentTest) {
    isnot(p1, p2, "Should have wrapped the Promise in a new promise, because its constructor is not matching the current-compartment Promise constructor");
    verifyPromiseGlobal(p2, window, "Promise wrapping xrayed promise with therefore non-matching constructor");
  } else {
    is(p1, p2, "Should have left the Promise alone because its constructor matched");
  }
  return p2;
}

function passCorrectPromiseWithMismatchedConstructor() {
  var p1 = Promise.resolve(9);
  verifyPromiseGlobal(p1, window, "Promise.resolve() return value 3");
  p1.constructor = frames[0].Promise;
  var p2 = getPromise(window, p1);
  isnot(p1, p2,
        "Should have wrapped promise in a new promise, since its .constructor was wrong");
  verifyPromiseGlobal(p2, window,
                      "Promise wrapping passed-in promise with mismatched constructor");
  return p2.then(function(arg) {
    is(arg, 9, "Should have propagated along our resolution value");
  });
}

function passPromiseToOtherGlobal() {
  var p1 = Promise.resolve();
  verifyPromiseGlobal(p1, window, "Promise.resolve() return value 4");
  var p2 = getPromise(frames[0], p1);
  // The behavior here will depend on whether we're touching frames[0] via Xrays
  // or not.  If we are not, the current compartment while getting our promise
  // will be that of frames[0].  If we are, it will be our window's compartment.
  if (isXrayArgumentTest) {
    is(p1, p2, "Should have left the Promise alone, because its constructor matches the current compartment's constructor");
  } else {
    isnot(p1, p2, "Should have wrapped promise in a promise from the other global");
    verifyPromiseGlobal(p2, frames[0],
                        "Promise wrapping passed-in basic promise");
  }
  return p2;
}

function passPromiseSubclass() {
  class PromiseSubclass extends Promise {
    constructor(func) {
      super(func);
    }
  }

  var p1 = PromiseSubclass.resolve(11);
  verifyPromiseGlobal(p1, window, "PromiseSubclass.resolve() return value");
  var p2 = getPromise(window, p1);
  isnot(p1, p2,
        "Should have wrapped promise subclass in a new promise");
  verifyPromiseGlobal(p2, window,
                      "Promise wrapping passed-in promise subclass");
  return p2.then(function(arg) {
    is(arg, 11, "Should have propagated along our resolution value from subclass");
  });
}

function runPromiseArgumentTests(finishFunc) {
  Promise.resolve()
    .then(passBasicPromise)
    .then(passPrimitive.bind(undefined, window))
    .then(passPrimitive.bind(undefined, frames[0]))
    .then(passThenable.bind(undefined, window))
    .then(passThenable.bind(undefined, frames[0]))
    .then(passWrongPromiseWithMatchingConstructor)
    .then(passCorrectPromiseWithMismatchedConstructor)
    .then(passPromiseToOtherGlobal)
    .then(passPromiseSubclass)
    .then(finishFunc)
    .catch(function(e) {
      ok(false, `Exception thrown: ${e}@${location.pathname}:${e.lineNumber}:${e.columnNumber}`);
      finishFunc();
    });
}
