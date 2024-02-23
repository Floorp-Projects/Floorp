// |jit-test| --fast-warmup

// Prevent slowness with --ion-eager.
setJitCompilerOption("ion.warmup.trigger", 100);

function h() {
    return 1;
}
function g() {
    for (var j = 0; j < 10; j++) {
        h();
    }
    trialInline();
}
function f() {
    for (var i = 0; i < 2; i++) {
        var fun = Function(g.toString() + "g()");
        try {
            fun();
        } catch {}
        try {
            fun();
        } catch {}
    }

}
oomTest(f);
