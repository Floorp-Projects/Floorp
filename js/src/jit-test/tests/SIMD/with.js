load(libdir + 'simd.js');

setJitCompilerOption("ion.warmup.trigger", 50);

function f() {
    var f4 = SIMD.float32x4(1, 2, 3, 4);
    var i4 = SIMD.int32x4(1, 2, 3, 4);

    for (var i = 0; i < 150; i++) {
        assertEqX4(SIMD.int32x4.withX(i4, 42), [42, 2, 3, 4]);
        assertEqX4(SIMD.int32x4.withY(i4, 42), [1, 42, 3, 4]);
        assertEqX4(SIMD.int32x4.withZ(i4, 42), [1, 2, 42, 4]);
        assertEqX4(SIMD.int32x4.withW(i4, 42), [1, 2, 3, 42]);

        assertEqX4(SIMD.float32x4.withX(f4, 42), [42, 2, 3, 4]);
        assertEqX4(SIMD.float32x4.withY(f4, 42), [1, 42, 3, 4]);
        assertEqX4(SIMD.float32x4.withZ(f4, 42), [1, 2, 42, 4]);
        assertEqX4(SIMD.float32x4.withW(f4, 42), [1, 2, 3, 42]);
    }
}

f();

