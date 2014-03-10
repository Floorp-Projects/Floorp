function f(x) {
    return Math.fround() ? x : x >> 0;
}
function g() {
    return (f() !== Math.fround(0))
}
assertEq(g(), g());
