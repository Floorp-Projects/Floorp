// Smoke test for `Promise.any`, test262 should cover the function in
// more detail.

function expectedError() {
  reportCompare(true, false, "expected error");
}

// Empty elements.
Promise.any([]).then(expectedError, e => {
  assertEq(e instanceof AggregateError, true);
  assertEq(e.errors.length, 0);
});

// Single element.
Promise.any([Promise.resolve(0)]).then(v => {
  assertEq(v, 0);
});
Promise.any([Promise.reject(1)]).then(expectedError, e => {
  assertEq(e instanceof AggregateError, true);
  assertEq(e.errors.length, 1);
  assertEq(e.errors[0], 1);
});

// Multiple elements.
Promise.any([Promise.resolve(1), Promise.resolve(2)]).then(v => {
  assertEq(v, 1);
});
Promise.any([Promise.resolve(3), Promise.reject(4)]).then(v => {
  assertEq(v, 3);
});
Promise.any([Promise.reject(5), Promise.resolve(6)]).then(v => {
  assertEq(v, 6);
});
Promise.any([Promise.reject(7), Promise.reject(8)]).then(expectedError, e => {
  assertEq(e instanceof AggregateError, true);
  assertEq(e.errors.length, 2);
  assertEq(e.errors[0], 7);
  assertEq(e.errors[1], 8);
});

// Cross-Realm tests.
//
// Note: When |g| is a cross-compartment global, Promise.any creates the errors
// array and the AggregateError in |g|'s Realm. This doesn't follow the spec, but
// the code in js/src/builtin/Promise.cpp claims this is useful when the Promise
// compartment is less-privileged. This means for this test we can't use
// assertDeepEq below, because the result array/error may have the wrong prototype.
let g = newGlobal();

if (typeof isSameCompartment !== "function") {
  var isSameCompartment = SpecialPowers.Cu.getJSTestingFunctions().isSameCompartment;
}

// Test wrapping when no `Promise.any Reject Element Function` is called.
Promise.any.call(g.Promise, []).then(expectedError, e => {
  assertEq(e.name, "AggregateError");

  assertEq(isSameCompartment(e, g), true);
  assertEq(isSameCompartment(e.errors, g), true);

  assertEq(e.errors.length, 0);
});

// Test wrapping in `Promise.any Reject Element Function`.
Promise.any.call(g.Promise, [Promise.reject("err")]).then(expectedError, e => {
  assertEq(e.name, "AggregateError");

  assertEq(isSameCompartment(e, g), true);
  assertEq(isSameCompartment(e.errors, g), true);

  assertEq(e.errors.length, 1);
  assertEq(e.errors[0], "err");
});

if (typeof reportCompare === "function")
  reportCompare(0, 0);
