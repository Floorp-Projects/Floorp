// Test implementation details when arguments object keep strong references
// to their elements.

function checkWeakRef(f, isCleared = true) {
  let [wr, args] = f({});

  assertEq(wr.deref() !== undefined, true);

  clearKeptObjects();
  gc();

  // In an ideal world, the reference is always cleared. But in specific
  // circumstances we're currently keeping the reference alive. Should this
  // ever change, feel free to update this test case accordingly. IOW having
  // |isCleared == false| is okay for spec correctness, but it isn't optimal.
  assertEq(wr.deref() === undefined, isCleared);
}

checkWeakRef(function() {
  // Create a weak-ref for the first argument.
  let wr = new WeakRef(arguments[0]);

  // Clear the reference from the arguments object.
  arguments[0] = null;

  // Let the arguments object escape.
  return [wr, arguments];
});

checkWeakRef(function() {
  // Create a weak-ref for the first argument.
  let wr = new WeakRef(arguments[0]);

  // Clear the reference from the arguments object.
  Object.defineProperty(arguments, 0, {value: null});

  // Let the arguments object escape.
  return [wr, arguments];
});

checkWeakRef(function self() {
  // Create a weak-ref for the first argument.
  let wr = new WeakRef(arguments[0]);

  // Delete operation doesn't clear the reference!
  delete arguments[0];

  // Let the arguments object escape.
  return [wr, arguments];
}, /*isCleared=*/ false);

checkWeakRef(function() {
  "use strict";

  // Create a weak-ref for the first argument.
  let wr = new WeakRef(arguments[0]);

  // Clear the reference from the arguments object.
  arguments[0] = null;

  // Let the arguments object escape.
  return [wr, arguments];
});

checkWeakRef(function() {
  "use strict";

  // Create a weak-ref for the first argument.
  let wr = new WeakRef(arguments[0]);

  // This define operation doesn't clear the reference!
  Object.defineProperty(arguments, 0, {value: null});

  // Let the arguments object escape.
  return [wr, arguments];
}, /*isCleared=*/ false);

checkWeakRef(function() {
  "use strict";

  // Create a weak-ref for the first argument.
  let wr = new WeakRef(arguments[0]);

  // Delete operation doesn't clear the reference!
  delete arguments[0];

  // Let the arguments object escape.
  return [wr, arguments];
}, /*isCleared=*/ false);
