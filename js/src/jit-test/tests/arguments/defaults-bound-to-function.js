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
var res = h();
assertEq(res[0], undefined);
assertEq(res[1](), 42);
function i(b=FAIL) {
    function b() {}
}
assertThrowsInstanceOf(i, ReferenceError);
i(42);
function j(a=(b=42), b=8) {
    return b;
    function b() { return 43; }
}
assertEq(j()(), 43);
function k(a=(b=42), b=8) {
    return b;
    function a() { return 43; }
}
assertEq(k(), 42);
function l(a=8, b=a) {
    return b;
    function a() { return 42; }
}
assertEq(l(), 8);
