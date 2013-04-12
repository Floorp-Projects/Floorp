// vim: set ts=8 sts=4 et sw=4 tw=99:

function f(x, y) {
    // Confuse the type analysis to not know the type of x.
    var u;
    var a = x + u;
    var b = x + 3;
    return x + y;
}

function g_bool(x, y) {
    var t;
    if (x + 0)
        t = true;
    else
        t = false;
    return t + y;

}
function g_null(x) {
    return null + x;
}

assertEq(g_bool(1, 2), 3);
assertEq(g_bool(0, 2), 2);
assertEq(g_null(2), 2);

// These will not bailout.
assertEq(f(Math.cos(Math.PI), 2), 1);
assertEq(f(null, 2), 2);
assertEq(f(false, 2), 2);
assertEq(f(true, 2), 3);
assertEq(f(17, 2), 19);

// These will bailout.
assertEq(f(undefined, 2), Number.NaN);
assertEq(f("20", 2), "202");
assertEq(f(16.3, 2), 18.3);
assertEq((1 / f(-0, -0)), -Infinity);


