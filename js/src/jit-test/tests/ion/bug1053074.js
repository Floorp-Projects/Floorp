function f(y) {
        return 2147483648 < y >>> 0
}
assertEq(f(0), false);
assertEq(f(-8), true);

function g(y) {
    var t = Math.floor(y);
    return 2147483648 < t+2;
}
assertEq(g(0), false)
assertEq(g(2147483647), true)
