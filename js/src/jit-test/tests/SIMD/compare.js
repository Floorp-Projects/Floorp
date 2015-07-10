load(libdir + 'simd.js');

setJitCompilerOption("ion.warmup.trigger", 50);

function bool(x) {
    return x ? -1 : 0;
}

function f() {
    var f1 = SIMD.Float32x4(1, 2, 3, 4);
    var f2 = SIMD.Float32x4(NaN, Infinity, 3.14, -0);

    var i1 = SIMD.Int32x4(1, 2, -3, 4);
    var i2 = SIMD.Int32x4(1, -2, 3, 0);

    for (var i = 0; i < 150; i++) {
        assertEqX4(SIMD.Int32x4.lessThan(i1, i2),             [0, 0, 1, 0].map(bool));
        assertEqX4(SIMD.Int32x4.lessThanOrEqual(i1, i2),      [1, 0, 1, 0].map(bool));
        assertEqX4(SIMD.Int32x4.equal(i1, i2),                [1, 0, 0, 0].map(bool));
        assertEqX4(SIMD.Int32x4.notEqual(i1, i2),             [0, 1, 1, 1].map(bool));
        assertEqX4(SIMD.Int32x4.greaterThan(i1, i2),          [0, 1, 0, 1].map(bool));
        assertEqX4(SIMD.Int32x4.greaterThanOrEqual(i1, i2),   [1, 1, 0, 1].map(bool));

        assertEqX4(SIMD.Float32x4.lessThan(f1, f2),             [0, 1, 1, 0].map(bool));
        assertEqX4(SIMD.Float32x4.lessThanOrEqual(f1, f2),      [0, 1, 1, 0].map(bool));
        assertEqX4(SIMD.Float32x4.equal(f1, f2),                [0, 0, 0, 0].map(bool));
        assertEqX4(SIMD.Float32x4.notEqual(f1, f2),             [1, 1, 1, 1].map(bool));
        assertEqX4(SIMD.Float32x4.greaterThan(f1, f2),          [0, 0, 0, 1].map(bool));
        assertEqX4(SIMD.Float32x4.greaterThanOrEqual(f1, f2),   [0, 0, 0, 1].map(bool));
    }
}

f();

