// 1) Trial inline f1 => g (g1) => h.
// 2) Set g to g2, to fail the f1 => g1 call site.
// 3) Set g to g1 again.
// 4) Make g1's generic ICScript trial inline a different callee, h2.
// 5) Bail out from f1 => g1 => h.
//
// The bailout must not confuse the ICScripts of h1 and h2.

function noninlined1(x) {
    with (this) {};
    if (x === 4002) {
        // Step 4.
        f2();
        // Step 5.
        return true;
    }
    return false;
}
function noninlined2(x) {
    with (this) {};
    if (x === 4000) {
        // Step 2.
        g = (h, x) => {
            return x + 1;
        };
    }
    if (x === 4001) {
        // Step 3.
        g = g1;
    }
}
var h = function(x) {
    if (noninlined1(x)) {
        // Step 5.
        bailout();
    }
    return x + 1;
};
var g = function(callee, x) {
    return callee(x) + 1;
};
var g1 = g;

function f2() {
    var h2 = x => x + 1;
    for (var i = 0; i < 300; i++) {
        var x = (i % 2 === 0) ? "foo" : i; // Force trial inlining.
        g1(h2, x);
    }
}

function f1() {
    for (var i = 0; i < 4200; i++) {
        var x = (i < 900 && i % 2 === 0) ? "foo" : i; // Force trial inlining.
        g(h, x);
        noninlined2(i);
        if (i === 200) {
            trialInline();
        }
    }
}
f1();
