function f(x) {
    return Math.round((x >>> 0) / 2) >> 0;
}
f(2);
assertEq(f(1), 1);
