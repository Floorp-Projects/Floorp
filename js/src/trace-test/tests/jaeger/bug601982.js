/* vim: set ts=4 sw=4 tw=99 et: */

function J(i) {
    /* Cause a branch to build. */
    if (i % 3)
        <xml></xml>
}

function h(i) {
    J(i);

    /* Generate a safe point in the method JIT. */
    if (1 == 14) { eval(); }

    return J(i);
}

function g(i) {
    /* Method JIT will try to remove this frame. */
    if (i == 14) { <xml></xml> }
    return h(i);
}

function f() {
    for (var i = 0; i < RUNLOOP * 2; i++) {
        g(i);
    }
}

f();

/* Don't crash. */

