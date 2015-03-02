load(libdir + 'simd.js');

setJitCompilerOption("ion.warmup.trigger", 50);

function f() {
    for (var i = 0; i < 150; i++) {
        assertEqX4(SIMD.int32x4.splat(42),   [42, 42, 42, 42]);
        assertEqX4(SIMD.float32x4.splat(42), [42, 42, 42, 42]);
    }
}

f();

