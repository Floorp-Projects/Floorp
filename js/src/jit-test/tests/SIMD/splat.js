load(libdir + 'simd.js');

setJitCompilerOption("ion.warmup.trigger", 50);

function f() {
    for (var i = 0; i < 150; i++) {
        assertEqX4(SIMD.Int32x4.splat(42),   [42, 42, 42, 42]);
        assertEqX4(SIMD.Float32x4.splat(42), [42, 42, 42, 42]);
        assertEqX4(SIMD.Bool32x4.splat(true), [true, true, true, true]);
        assertEqX4(SIMD.Bool32x4.splat(false), [false, false, false, false]);
    }
}

f();

