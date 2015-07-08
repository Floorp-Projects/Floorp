// |reftest| skip-if(!this.hasOwnProperty("SIMD"))

/*
 * Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/
 */

var float32x4 = SIMD.float32x4;
var float64x2 = SIMD.float64x2;
var int32x4 = SIMD.int32x4;

function select(mask, ifTrue, ifFalse) {
    var m = simdToArray(mask);
    var tv = simdToArray(ifTrue);
    var fv = simdToArray(ifFalse);
    return m.map(function(v, i) {
        return (v < 0 ? tv : fv)[i];
    });
}

/**
 * Tests type.select on all input pairs, for all possible masks. As the mask
 * has 4 lanes and 2 possible values (true or false), there are 16 possible
 * masks.
 */
function testSelect(type, inputs) {
    var x, y;
    for (var i = 0; i < 16; i++) {
        var mask = int32x4.bool(!!(i & 1), !!((i >> 1) & 1), !!((i >> 2) & 1), !!((i >> 3) & 1));
        for ([x, y] of inputs)
            assertEqVec(type.select(mask, x, y), select(mask, x, y));
    }
}

function int32x4FromTypeBits(type, vec) {
    switch (type) {
      case float32x4:
          return int32x4.fromFloat32x4Bits(vec);
      case float64x2:
          return int32x4.fromFloat64x2Bits(vec);
      case int32x4:
          return vec;
      default:
          throw new TypeError("Unknown SIMD type.");
    }
}

function bitselect(type, mask, ifTrue, ifFalse) {
    var tv = int32x4FromTypeBits(type, ifTrue);
    var fv = int32x4FromTypeBits(type, ifFalse);
    var tr = int32x4.and(mask, tv);
    var fr = int32x4.and(int32x4.not(mask), fv);
    var orApplied = int32x4.or(tr, fr);
    var converted = type == int32x4 ? orApplied : type.fromInt32x4Bits(orApplied);
    return simdToArray(converted);
}

function findCorrespondingScalarTypedArray(type) {
    switch (type) {
        case int32x4: return Int32Array;
        case float32x4: return Float32Array;
        case float64x2: return Float64Array;
        default: throw new Error("undefined scalar typed array");
    }
}

/**
 * This tests type.bitselect on all boolean masks, as in select. For these,
 *          bitselect(mask, x, y) === select(mask, x, y)
 */
function testBitSelectSimple(type, inputs) {
    var x, y;
    var ScalarTypedArray = findCorrespondingScalarTypedArray(type);
    for (var i = 0; i < 16; i++) {
        var mask = int32x4.bool(!!(i & 1), !!((i >> 1) & 1), !!((i >> 2) & 1), !!((i >> 3) & 1));
        for ([x, y] of inputs)
            assertEqVec(type.bitselect(mask, x, y), bitselect(type, mask, x, y));
    }
}

/**
 * This tests type.bitselect on a few hand-defined masks. For these,
 *          bitselect(mask, x, y) !== select(mask, x, y)
 */
function testBitSelectComplex(type, inputs) {
    var x, y;
    var masks = [
        int32x4(1337, 0x1337, 0x42, 42),
        int32x4(0x00FF1CE, 0xBAADF00D, 0xDEADBEEF, 0xCAFED00D),
        int32x4(0xD15EA5E, 0xDEADC0DE, 0xFACEB00C, 0x4B1D4B1D)
    ];
    var ScalarTypedArray = findCorrespondingScalarTypedArray(type);
    for (var mask of masks) {
        for ([x, y] of inputs)
            assertEqVec(type.bitselect(mask, x, y), bitselect(type, mask, x, y));
    }
}

function test() {
    var inputs = [
        [int32x4(0,4,9,16), int32x4(1,2,3,4)],
        [int32x4(-1, 2, INT32_MAX, INT32_MIN), int32x4(INT32_MAX, -4, INT32_MIN, 42)]
    ];

    testSelect(int32x4, inputs);
    testBitSelectSimple(int32x4, inputs);
    testBitSelectComplex(int32x4, inputs);

    inputs = [
        [float32x4(0.125,4.25,9.75,16.125), float32x4(1.5,2.75,3.25,4.5)],
        [float32x4(-1.5,-0,NaN,-Infinity), float32x4(1,-2,13.37,3.13)],
        [float32x4(1.5,2.75,NaN,Infinity), float32x4(-NaN,-Infinity,9.75,16.125)]
    ];

    testSelect(float32x4, inputs);
    testBitSelectSimple(float32x4, inputs);
    testBitSelectComplex(float32x4, inputs);

    inputs = [
        [float64x2(0.125,4.25), float64x2(9.75,16.125)],
        [float64x2(1.5,2.75), float64x2(3.25,4.5)],
        [float64x2(-1.5,-0), float64x2(NaN,-Infinity)],
        [float64x2(1,-2), float64x2(13.37,3.13)],
        [float64x2(1.5,2.75), float64x2(NaN,Infinity)],
        [float64x2(-NaN,-Infinity), float64x2(9.75,16.125)]
    ];

    testSelect(float64x2, inputs);
    testBitSelectSimple(float64x2, inputs);
    testBitSelectComplex(float64x2, inputs);

    if (typeof reportCompare === "function")
        reportCompare(true, true);
}

test();
