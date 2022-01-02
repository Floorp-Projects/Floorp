/* vim: set ts=8 sts=4 et sw=4 tw=99: */

function J(i) {
    /* Cause a branch to build(?) */
    if (i % 3)
        [1,2,3]
}

function h(i) {
    J(i);

    /* Generate a safe point in the method JIT. */
    if (1 == 14) { eval(); }

    return J(i);
}

function g(i) {
    /* Method JIT will try to remove this frame(?) */
    if (i == 14) { with ({}); }
    return h(i);
}

function f() {
    for (var i = 0; i < 9 * 2; i++) {
        g(i);
    }
}

f();

/* Don't crash. */

