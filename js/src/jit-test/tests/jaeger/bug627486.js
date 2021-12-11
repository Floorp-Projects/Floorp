// |jit-test| error: TypeError
// vim: set ts=8 sts=4 et sw=4 tw=99:
g = undefined;
function L() { }

function h() {
    with (h) { }
    for (var i = 0; i < 10; i++)
        g();
}

function f(x) {
    g = x;
}

f(L);
h();
f(L);
f(2);
h();

/* Don't assert/crash. */

