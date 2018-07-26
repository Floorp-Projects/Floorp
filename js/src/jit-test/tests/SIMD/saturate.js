load(libdir + 'simd.js');

setJitCompilerOption("ion.warmup.trigger", 50);

const INT8_MIN = -128;
const INT8_MAX = 127;
const UINT8_MAX = 255;

function sat8(x) {
    if (x < INT8_MIN) return INT8_MIN;
    if (x > INT8_MAX) return INT8_MAX;
    return x;
}

function usat8(x) {
    if (x < 0) return 0;
    if (x > UINT8_MAX) return UINT8_MAX;
    return x;
}

function f() {
    var i1 = SIMD.Int8x16(1, 100, 3, 4);
    var i2 = SIMD.Int8x16(4, 30, 2, 1);

    var u1 = SIMD.Uint8x16(1, 2, 3, 4);
    var u2 = SIMD.Uint8x16(4, 3, 2, 1);

    for (var i = 0; i < 150; i++) {
        assertEqX4(SIMD.Int8x16.addSaturate(i1, i2), binaryX((x, y) => sat8(x + y), i1, i2));
        assertEqX4(SIMD.Int8x16.subSaturate(i1, i2), binaryX((x, y) => sat8(x - y), i1, i2));

        assertEqX4(SIMD.Uint8x16.addSaturate(u1, u2), binaryX((x, y) => usat8(x + y), u1, u2));
        assertEqX4(SIMD.Uint8x16.subSaturate(u1, u2), binaryX((x, y) => usat8(x - y), u1, u2));
    }
}

f();
