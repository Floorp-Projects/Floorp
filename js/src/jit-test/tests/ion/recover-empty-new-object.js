// |jit-test| test-join=--no-unboxed-objects
//
// Unboxed object optimization might not trigger in all cases, thus we ensure
// that Sink optimization is working well independently of the
// object representation.

// Ion eager fails the test below because we have not yet created any
// template object in baseline before running the content of the top-level
// function.
if (getJitCompilerOptions()["ion.warmup.trigger"] <= 20)
    setJitCompilerOption("ion.warmup.trigger", 20);

// These arguments have to be computed by baseline, and thus captured in a
// resume point. The next function checks that we can remove the computation of
// these arguments.
function f(a, b, c, d) { }

function topLevel() {
    for (var i = 0; i < 30; i++) {
        var unused = {};
        var a = {};
        var b = {};
        var c = {};
        var d = {};
        assertRecoveredOnBailout(unused, true);
        assertRecoveredOnBailout(a, true);
        assertRecoveredOnBailout(b, true);
        assertRecoveredOnBailout(c, true);
        assertRecoveredOnBailout(d, true);
        f(a, b, c, d);
    }
}

topLevel();
