load(libdir + 'simd.js');

setJitCompilerOption("ion.warmup.trigger", 50);

function f() {
    var f1 = SIMD.Float32x4(1, 2, 3, 4);
    var f2 = SIMD.Float32x4(NaN, Infinity, 3.14, -0);

    var i1 = SIMD.Int32x4(1, 2, -3, 4);
    var i2 = SIMD.Int32x4(1, -2, 3, 0);

    var u1 = SIMD.Uint32x4(1, 2, -3, 4);
    var u2 = SIMD.Uint32x4(1, -2, 3, 0x80000000);

    for (var i = 0; i < 150; i++) {
        assertEqX4(SIMD.Int32x4.lessThan(i1, i2),             [false, false, true, false]);
        assertEqX4(SIMD.Int32x4.lessThanOrEqual(i1, i2),      [true, false, true, false]);
        assertEqX4(SIMD.Int32x4.equal(i1, i2),                [true, false, false, false]);
        assertEqX4(SIMD.Int32x4.notEqual(i1, i2),             [false, true, true, true]);
        assertEqX4(SIMD.Int32x4.greaterThan(i1, i2),          [false, true, false, true]);
        assertEqX4(SIMD.Int32x4.greaterThanOrEqual(i1, i2),   [true, true, false, true]);

        assertEqX4(SIMD.Uint32x4.lessThan(u1, u2),             [false, true, false, true]);
        assertEqX4(SIMD.Uint32x4.lessThanOrEqual(u1, u2),      [true,  true, false, true]);
        assertEqX4(SIMD.Uint32x4.equal(u1, u2),                [true, false, false, false]);
        assertEqX4(SIMD.Uint32x4.notEqual(u1, u2),             [false, true, true, true]);
        assertEqX4(SIMD.Uint32x4.greaterThan(u1, u2),          [false, false, true, false]);
        assertEqX4(SIMD.Uint32x4.greaterThanOrEqual(u1, u2),   [true, false, true, false]);

        assertEqX4(SIMD.Float32x4.lessThan(f1, f2),             [false, true, true, false]);
        assertEqX4(SIMD.Float32x4.lessThanOrEqual(f1, f2),      [false, true, true, false]);
        assertEqX4(SIMD.Float32x4.equal(f1, f2),                [false, false, false, false]);
        assertEqX4(SIMD.Float32x4.notEqual(f1, f2),             [true, true, true, true]);
        assertEqX4(SIMD.Float32x4.greaterThan(f1, f2),          [false, false, false, true]);
        assertEqX4(SIMD.Float32x4.greaterThanOrEqual(f1, f2),   [false, false, false, true]);
    }
}

f();
