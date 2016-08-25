load(libdir + "asserts.js");

function f(a=42) {
    return a;
    function a() { return 19; }
}
assertEq(f()(), 19);
function h(a=b, b=43) {
    return [a, b];
    function b() { return 42; }
}
// TDZ
assertThrowsInstanceOf(h, ReferenceError);
function i(b=FAIL) {
    function b() {}
}
assertThrowsInstanceOf(i, ReferenceError);
i(42);
function j(a=(b=42), b=8) {
    return b;
    function b() { return 43; }
}
// TDZ
assertThrowsInstanceOf(j, ReferenceError);
function k(a=(b=42), b=8) {
    return b;
    function a() { return 43; }
}
// TDZ
assertThrowsInstanceOf(k, ReferenceError);
function l(a=8, b=a) {
    return b;
    function a() { return 42; }
}
assertEq(l(), 8);
function m([a, b]=[1, 2], c=a) {
  function a() { return 42; }
  assertEq(typeof a, "function");
  assertEq(a(), 42);
  assertEq(b, 2);
  assertEq(c, 1);
}
m();
