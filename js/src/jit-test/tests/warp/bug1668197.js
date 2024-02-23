function f(x, y) {
    return ~Math.hypot(x >>> 0, 2 - x >>> 0);
}
f(2, Math);
oomTest(f);
