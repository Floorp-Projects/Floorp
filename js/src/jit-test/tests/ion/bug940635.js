function f(y) {
    return (y > 0) == y;
}
assertEq(f(0), true);
assertEq(f(0), true);
assertEq(f(null), false);
assertEq(f(null), false);
