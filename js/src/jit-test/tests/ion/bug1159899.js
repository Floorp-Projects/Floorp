function f(x) {
        return ~~(x >>> 0) / (x >>> 0) | 0
}
f(1)
assertEq(f(-1), 0);
