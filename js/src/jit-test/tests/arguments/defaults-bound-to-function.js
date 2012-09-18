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
assertEq(res[0], res[1]);
assertEq(res[0](), 42);
function i(b=FAIL) {
    function b() {}
}
i();
i(42);
function j(a=(b=42), b=8) {
    return b;
    function b() {}
}
assertEq(j(), 42);
