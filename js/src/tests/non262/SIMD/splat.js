// |reftest| skip-if(!this.hasOwnProperty("SIMD"))

var Float64x2 = SIMD.Float64x2;
var Float32x4 = SIMD.Float32x4;
var Int8x16 = SIMD.Int8x16;
var Int16x8 = SIMD.Int16x8;
var Int32x4 = SIMD.Int32x4;
var Uint8x16 = SIMD.Uint8x16;
var Uint16x8 = SIMD.Uint16x8;
var Uint32x4 = SIMD.Uint32x4;
var Bool8x16 = SIMD.Bool8x16;
var Bool16x8 = SIMD.Bool16x8;
var Bool32x4 = SIMD.Bool32x4;

function TestSplatX16(type, inputs, coerceFunc) {
    for (var x of inputs) {
        assertEqX16(SIMD[type].splat(x), [x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x].map(coerceFunc));
    }
}

function TestSplatX8(type, inputs, coerceFunc) {
    for (var x of inputs) {
        assertEqX8(SIMD[type].splat(x), [x, x, x, x, x, x, x, x].map(coerceFunc));
    }
}

function TestSplatX4(type, inputs, coerceFunc) {
    for (var x of inputs) {
        assertEqX4(SIMD[type].splat(x), [x, x, x, x].map(coerceFunc));
    }
}

function TestSplatX2(type, inputs, coerceFunc) {
    for (var x of inputs) {
        assertEqX2(SIMD[type].splat(x), [x, x].map(coerceFunc));
    }
}

function test() {
    function TestError(){};

    var good = {valueOf: () => 19.89};
    var bad = {valueOf: () => { throw new TestError(); }};

    TestSplatX16('Int8x16', [0, 1, 2, -1, -2, 3, -3, 4, -4, 5, -5, 6, INT8_MIN, INT8_MAX, INT8_MIN - 1, INT8_MAX + 1], (x) => x << 24 >> 24);
    assertEqX16(Int8x16.splat(), [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]);
    assertThrowsInstanceOf(() => SIMD.Int8x16.splat(bad), TestError);

    TestSplatX8('Int16x8', [0, 1, 2, -1, INT16_MIN, INT16_MAX, INT16_MIN - 1, INT16_MAX + 1], (x) => x << 16 >> 16);
    assertEqX8(Int16x8.splat(), [0, 0, 0, 0, 0, 0, 0, 0]);
    assertThrowsInstanceOf(() => SIMD.Int16x8.splat(bad), TestError);

    TestSplatX4('Int32x4', [0, undefined, 3.5, 42, -1337, INT32_MAX, INT32_MAX + 1, good], (x) => x | 0);
    assertEqX4(SIMD.Int32x4.splat(), [0, 0, 0, 0]);
    assertThrowsInstanceOf(() => SIMD.Int32x4.splat(bad), TestError);

    TestSplatX16('Uint8x16', [0, 1, 2, -1, -2, 3, -3, 4, -4, 5, -5, 6, INT8_MIN, INT8_MAX, INT8_MIN - 1, INT8_MAX + 1], (x) => x << 24 >>> 24);
    assertEqX16(Uint8x16.splat(), [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]);
    assertThrowsInstanceOf(() => SIMD.Uint8x16.splat(bad), TestError);

    TestSplatX8('Uint16x8', [0, 1, 2, -1, INT16_MIN, INT16_MAX, INT16_MIN - 1, INT16_MAX + 1], (x) => x << 16 >>> 16);
    assertEqX8(Uint16x8.splat(), [0, 0, 0, 0, 0, 0, 0, 0]);
    assertThrowsInstanceOf(() => SIMD.Uint16x8.splat(bad), TestError);

    TestSplatX4('Uint32x4', [0, undefined, 3.5, 42, INT32_MAX, INT32_MIN, UINT32_MAX, UINT32_MAX + 1, good], (x) => x >>> 0);
    assertEqX4(SIMD.Uint32x4.splat(), [0, 0, 0, 0]);
    assertThrowsInstanceOf(() => SIMD.Uint32x4.splat(bad), TestError);

    TestSplatX4('Float32x4', [0, undefined, 3.5, 42, -13.37, Infinity, NaN, -0, good], (x) => Math.fround(x));
    assertEqX4(SIMD.Float32x4.splat(), [NaN, NaN, NaN, NaN]);
    assertThrowsInstanceOf(() => SIMD.Float32x4.splat(bad), TestError);

    TestSplatX2('Float64x2', [0, undefined, 3.5, 42, -13.37, Infinity, NaN, -0, good], (x) => +x);
    assertEqX2(SIMD.Float64x2.splat(), [NaN, NaN]);
    assertThrowsInstanceOf(() => SIMD.Float64x2.splat(bad), TestError);

    TestSplatX16('Bool8x16', [true, false], (x) => !!x);
    assertEqX16(Bool8x16.splat(),    [false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false]);
    assertEqX16(Bool8x16.splat(bad), [true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true]);

    TestSplatX8('Bool16x8', [true, false], (x) => !!x);
    assertEqX8(Bool16x8.splat(),    [false, false, false, false, false, false, false, false]);
    assertEqX8(Bool16x8.splat(bad), [true, true, true, true, true, true, true, true]);

    TestSplatX4('Bool32x4', [true, false], (x) => !!x);
    assertEqX4(SIMD.Bool32x4.splat(),    [false, false, false, false]);
    assertEqX4(SIMD.Bool32x4.splat(bad), [true, true, true, true]);

    TestSplatX2('Bool64x2', [true, false], (x) => !!x);
    assertEqX2(SIMD.Bool64x2.splat(),    [false, false]);
    assertEqX2(SIMD.Bool64x2.splat(bad), [true, true]);

    if (typeof reportCompare === "function")
        reportCompare(true, true);
}

test();
