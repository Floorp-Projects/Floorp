// |reftest| skip-if(!this.hasOwnProperty("SIMD"))

var float64x2 = SIMD.float64x2;
var float32x4 = SIMD.float32x4;
var int32x4 = SIMD.int32x4;

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

    TestSplatX4('int32x4', [0, undefined, 3.5, 42, -1337, INT32_MAX, INT32_MAX + 1, good], (x) => x | 0);
    assertEqX4(SIMD.int32x4.splat(), [0, 0, 0, 0]);
    assertThrowsInstanceOf(() => SIMD.int32x4.splat(bad), TestError);

    TestSplatX4('float32x4', [0, undefined, 3.5, 42, -13.37, Infinity, NaN, -0, good], (x) => Math.fround(x));
    assertEqX4(SIMD.float32x4.splat(), [NaN, NaN, NaN, NaN]);
    assertThrowsInstanceOf(() => SIMD.float32x4.splat(bad), TestError);

    TestSplatX2('float64x2', [0, undefined, 3.5, 42, -13.37, Infinity, NaN, -0, good], (x) => +x);
    assertEqX4(SIMD.float64x2.splat(), [NaN, NaN]);
    assertThrowsInstanceOf(() => SIMD.float64x2.splat(bad), TestError);

    if (typeof reportCompare === "function")
        reportCompare(true, true);
}

test();
