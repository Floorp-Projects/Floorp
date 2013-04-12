// vim: set ts=8 sts=4 et sw=4 tw=99:

function w(y)
{
    var x = 23.5;
    return x & y;
}

function f(x, y) {
    // Confuse the type analysis to not know the type of x.
    var t = 3.5 + x;
    t + 3.5;
    return x & y;
}

function g_bool(x, y) {
    var t;
    if (x + 0)
        t = true;
    else
        t = false;
    return t & y;

}

function g_null(x) {
    return null & x;
}

var obj = { valueOf: function () { return 5; } }

assertEq(w(93), 21);
assertEq(g_bool(1, 3), 1);
assertEq(g_bool(0, 3), 0);
assertEq(g_null(2), 0);

assertEq(f(1, 7), 1);
assertEq(f(true, 7), 1);
assertEq(f(false, 7), 0);
assertEq(f("3", 7), 3);
assertEq(f(obj, 7), 5);
assertEq(f(3.5, 7), 3);
assertEq(f(undefined, 7), 0);
assertEq(f(null, 7), 0);
assertEq(f(Math.NaN, 7), 0);

