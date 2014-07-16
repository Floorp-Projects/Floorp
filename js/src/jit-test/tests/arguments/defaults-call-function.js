load(libdir + "asserts.js");

function f1(a=g()) {
  function g() {
  }
}
// FIXME Bug 1022967 - ES6 requires a ReferenceError for this case.
assertThrowsInstanceOf(f1, TypeError);

function f2(a=g()) {
  function g() {
    return 43;
  }
  assertEq(a, 42);
}
f2(42);
