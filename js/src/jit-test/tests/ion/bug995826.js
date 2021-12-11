function f(x) {
        return Math.round(-Math.tan(x > 0))
}
f(2)
assertEq(f(-1), -0);
