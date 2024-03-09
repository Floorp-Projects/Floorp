// 1) Trial inline f => g1 => h.
// 2) Make f => g1 call site polymorphic by calling f => g2.
//    This gets rid of the ICScript::inlinedChildren_ edge.
// 3) Restore f => g1.
// 4) Trigger a shrinking GC from f => g1 => h (h not trial-inlined; h preserves Baseline code)
//    This purges h's inlined ICScript.
// 5) Call f => g1 => h (trial inlined). This must not use the discarded ICScript.
function h(i, x) {
    if (i === 900) {
        // Step 4.
        gc(this, "shrinking");
    }
    return x + 1;
}
function g2(i, x) {
    if (i === 820) {
        // Step 3.
        callee = g1;
    }
    return h(i, x) + x;
}
function g1(i, x) {
    if (i === 800) {
        // Step 2.
        callee = g2;
    }
    if (i === 900) {
        // Step 4.
        h(i, x);
    }
    return h(i, x) + x;
}

var callee = g1;

function f() {
    for (var i = 0; i < 1000; i++) {
        callee(i, i);
        callee(i, "foo");
    }
}
f();
