function f(x) {
    return (x != x) != Math.fround(x)
}
assertEq(f(0), f(0));
