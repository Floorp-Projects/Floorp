load(libdir + "asserts.js");

function f1(a=g()) {
  function g() {
  }
}
assertThrowsInstanceOf(f1, ReferenceError);

function f2(a=g()) {
  function g() {
    return 43;
  }
  assertEq(a, 42);
}
f2(42);
