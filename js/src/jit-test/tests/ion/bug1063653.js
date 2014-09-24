
function g(x) {
    return (0 > (Math.max(x, x) || x))
}
function f() {
    return g(g() >> 0)
}
for (var k = 0; k < 1; ++k) {
    f();
}
