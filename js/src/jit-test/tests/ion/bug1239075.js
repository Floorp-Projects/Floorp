
function g0() { with({}){}; }
function f0(y, x) {
    var a = y >>> 0;
    a = a - 1 + 1;
    g0(); // Capture the truncate result after the call.
    var b = x / 2; // bailout.
    return ~(a + b);
}
assertEq(f0(-1, 0), 0);
assertEq(f0(-1, 1), 0);


function g1() { with({}){}; }
function f1(y, x) {
    var a = y >>> 0;
    a = a - 1 + 1;
    g1(); // Capture the truncate result after the call.
    var b = Math.pow(x / 2, x); // bailout.
    return ~(a + b);
}
assertEq(f1(-1, 0), -1);
assertEq(f1(-1, 1), 0);

function f2(x) {
    return ~(((~0 | 0) >>> 0 || 0) + Math.pow(Math.cos(x >>> 0), Math.atan2(0, x)))
}
assertEq(f2(0), -1);
assertEq(f2(-9999), 0);
