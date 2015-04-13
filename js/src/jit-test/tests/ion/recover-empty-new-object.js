
// Ion eager fails the test below because we have not yet created any
// template object in baseline before running the content of the top-level
// function.
if (getJitCompilerOptions()["ion.warmup.trigger"] <= 20)
    setJitCompilerOption("ion.warmup.trigger", 20);

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
