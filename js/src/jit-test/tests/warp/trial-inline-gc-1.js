// |jit-test| --fast-warmup
function g(x) {
    return x + 1;
}
function f() {
    for (var i = 0; i < 150; i++) {
        assertEq(g("foo"), "foo1");
        assertEq(g(1), 2);
        if (i === 100) {
            gc(this, "shrinking");
        }
    }
}
f();
