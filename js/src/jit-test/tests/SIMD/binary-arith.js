load(libdir + 'simd.js');

setJitCompilerOption("ion.warmup.trigger", 50);

function f() {
    var i1 = SIMD.Int32x4(1, 2, 3, 4);
    var i2 = SIMD.Int32x4(4, 3, 2, 1);

    var f1 = SIMD.Float32x4(1, 2, 3, 4);
    var f2 = SIMD.Float32x4(4, 3, 2, 1);

    for (var i = 0; i < 150; i++) {
        assertEqX4(SIMD.Float32x4.add(f1, f2), binaryX4((x, y) => x + y, f1, f2));
        assertEqX4(SIMD.Float32x4.sub(f1, f2), binaryX4((x, y) => x - y, f1, f2));
        assertEqX4(SIMD.Float32x4.mul(f1, f2), binaryX4((x, y) => x * y, f1, f2));

        assertEqX4(SIMD.Int32x4.add(i1, i2), binaryX4((x, y) => x + y, i1, i2));
        assertEqX4(SIMD.Int32x4.sub(i1, i2), binaryX4((x, y) => x - y, i1, i2));
        assertEqX4(SIMD.Int32x4.mul(i1, i2), binaryX4((x, y) => x * y, i1, i2));
    }
}

f();
