load(libdir + 'simd.js');

setJitCompilerOption("ion.warmup.trigger", 50);

const T = -1, F = 0;

function f() {
    for (var i = 0; i < 150; i++) {
        assertEqX4(SIMD.Int32x4.bool(i + 1, true, 'hey', null), [T, T, T, F]);
        assertEqX4(SIMD.Int32x4.bool(undefined, '', {}, objectEmulatingUndefined()), [F, F, T, F]);
        assertEqX4(SIMD.Int32x4.bool(null, NaN, false, Infinity), [F, F, F, T]);
    }
}

f();
