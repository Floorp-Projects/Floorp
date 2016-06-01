load(libdir + 'simd.js');

setJitCompilerOption("ion.warmup.trigger", 50);

function f() {
    var i1 = SIMD.Int32x4(1, 2, 3, 4);
    var i2 = SIMD.Int32x4(4, 3, 2, 1);

    var f1 = SIMD.Float32x4(1, 2, 3, 4);
    var f2 = SIMD.Float32x4(4, 3, 2, 1);

    var i8_1 = SIMD.Int8x16(1, 2, 3, 4, 20, 30, 40, 50, 100, 115, 120, 125);
    var i8_2 = SIMD.Int8x16(4, 3, 2, 1,  8,  7,  6,  5,  12,  11,  10,   9);

    for (var i = 0; i < 150; i++) {
        assertEqX4(SIMD.Float32x4.add(f1, f2), binaryX((x, y) => x + y, f1, f2));
        assertEqX4(SIMD.Float32x4.sub(f1, f2), binaryX((x, y) => x - y, f1, f2));
        assertEqX4(SIMD.Float32x4.mul(f1, f2), binaryX((x, y) => x * y, f1, f2));

        assertEqX4(SIMD.Int32x4.add(i1, i2), binaryX((x, y) => x + y, i1, i2));
        assertEqX4(SIMD.Int32x4.sub(i1, i2), binaryX((x, y) => x - y, i1, i2));
        assertEqX4(SIMD.Int32x4.mul(i1, i2), binaryX((x, y) => x * y, i1, i2));

        assertEqX4(SIMD.Int8x16.add(i8_1, i8_2), binaryX((x, y) => (x + y) << 24 >> 24, i8_1, i8_2));
        assertEqX4(SIMD.Int8x16.sub(i8_1, i8_2), binaryX((x, y) => (x - y) << 24 >> 24, i8_1, i8_2));
        assertEqX4(SIMD.Int8x16.mul(i8_1, i8_2), binaryX((x, y) => (x * y) << 24 >> 24, i8_1, i8_2));
    }
}

f();
