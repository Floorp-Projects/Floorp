// |jit-test| --fast-warmup
function f(x) {
    var a = Math.fround(Math.fround(false));
    var b = Math.min(a, x ? Math.fround(x) : Math.fround(x));
    return b >>> 0;
}
function test() {
    with (this) {} // Don't inline.
    for (var i = 0; i < 100; i++) {
        assertEq(f(Infinity), 0);
    }
    assertEq(f(-1), 4294967295);
}
test();
