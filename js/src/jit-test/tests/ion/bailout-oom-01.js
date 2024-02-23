// |jit-test| --no-threads; --fast-warmup

setJitCompilerOption("ion.warmup.trigger", 20);
gczeal(0);

var nonce = 0;

function doTest() {
    // Block Warp/Ion compile.
    with ({}) {};

    nonce += 1;

    // Fresh function and script.
    let fn = new Function("arg", `
        /* {nonce} */
        var r1 = [];
        var r2 = [];
        return (() => arg + 1)();
    `);

    // Warm up JITs.
    for (var i = 0; i < 20; ++i) {
        assertEq(fn(i), i + 1);
    }

    // Trigger bailout.
    fn();
}

// Warmup doTest already.
doTest();
doTest();

// OOM test the JIT compilation and bailout.
oomTest(doTest);
