// |jit-test| --fast-warmup
function h(i) {
    with(this) {} // Don't inline.
    if (i === 100) {
        gc(this, "shrinking");
    }
}
function g(i, x) {
    h(i);
    return x + 1;
}
function f() {
    for (var i = 0; i < 200; i++) {
        g(i, "foo");
        g(i, 1);
    }
}
f();
