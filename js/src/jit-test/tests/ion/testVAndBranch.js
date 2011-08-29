// vim: set ts=4 sw=4 tw=99 et:
function f(x) {
    if (x)
        return true;
    return false;
}

assertEq(f(NaN), false);
assertEq(f(-0), false);
assertEq(f(3.3), true);
assertEq(f(0), false);
assertEq(f(3), true);
assertEq(f("hi"), true);
assertEq(f(""), false);
assertEq(f(true), true);
assertEq(f(false), false);
assertEq(f(undefined), false);
assertEq(f({}), true);
assertEq(f(null), false);

