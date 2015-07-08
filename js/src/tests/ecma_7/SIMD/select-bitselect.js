// |reftest| skip-if(!this.hasOwnProperty("SIMD"))

/*
 * Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/
 */

var float32x4 = SIMD.float32x4;
var float64x2 = SIMD.float64x2;
var int8x16 = SIMD.int8x16;
var int16x8 = SIMD.int16x8;
var int32x4 = SIMD.int32x4;

function getMask(i, maskLength) {
    var args = [];
    for (var j = 0; j < maskLength; j++) args.push(!!((i >> j) & 1));
    if (maskLength == 4)
        return int32x4.bool(...args);
    else if (maskLength == 8)
        return int16x8.bool(...args);
    else if (maskLength == 16)
        return int8x16.bool(...args);
    else
        throw new Error("Invalid mask length.");
}

function selectMaskType(type) {
    if (type == int32x4 || type == float32x4 || type == float64x2)
        return int32x4;
    return type;
}

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
 * has 4 lanes (for int32x4) and 2 possible values (true or false), there are 16 possible
 * masks. For int8x16, the mask has 16 lanes and 2 possible values, so there are 256
 * possible masks. For int16x8, the mask has 8 lanes and 2 possible values, so there
 * are 64 possible masks.
 */
function testSelect(type, inputs) {
    var x, y;
    var maskLength = simdLengthType(type);
    maskLength = maskLength != 2 ? maskLength : 4;
    for (var i = 0; i < Math.pow(maskLength, 2); i++) {
        var mask = getMask(i, maskLength);
        for ([x, y] of inputs)
            assertEqVec(type.select(mask, x, y), select(mask, x, y));
    }
}

function intFromTypeBits(type, vec) {
    switch (type) {
      case float32x4:
          return int32x4.fromFloat32x4Bits(vec);
      case float64x2:
          return int32x4.fromFloat64x2Bits(vec);
      case int8x16:
          return vec;
      case int16x8:
          return vec;
      case int32x4:
          return vec;
      default:
          throw new TypeError("Unknown SIMD type.");
    }
}

function bitselect(type, mask, ifTrue, ifFalse) {
    var maskType = selectMaskType(type);
    var tv = intFromTypeBits(type, ifTrue);
    var fv = intFromTypeBits(type, ifFalse);
    var tr = maskType.and(mask, tv);
    var fr = maskType.and(maskType.not(mask), fv);
    var orApplied = maskType.or(tr, fr);
    var converted = type == maskType ? orApplied : type.fromInt32x4Bits(orApplied);
    return simdToArray(converted);
}

function findCorrespondingScalarTypedArray(type) {
    switch (type) {
        case int8x16: return Int8Array;
        case int16x8: return Int16Array;
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
    var maskLength = simdLengthType(type);
    maskLength = maskLength != 2 ? maskLength : 4;
    var ScalarTypedArray = findCorrespondingScalarTypedArray(type);
    for (var i = 0; i < Math.pow(maskLength, 2); i++) {
        var mask = getMask(i, maskLength);
        for ([x, y] of inputs)
            assertEqVec(type.bitselect(mask, x, y), bitselect(type, mask, x, y));
    }
}

/**
 * This tests type.bitselect on a few hand-defined masks. For these,
 *          bitselect(mask, x, y) !== select(mask, x, y)
 */
function testBitSelectComplex(type, inputs) {
    var masks8 = [
        int8x16(0x42, 42, INT8_MAX, INT8_MIN, INT8_MAX + 1, INT8_MIN - 1, 13, 37, -42, 125, -125, -1, 1, 0xA, 0xB, 0xC)
    ]
    var masks16 = [
        int16x8(0x42, 42, INT16_MAX, INT16_MIN, INT16_MAX + 1, INT16_MIN - 1, 13, 37),
        int16x8(-42, 125, -125, -1, 1, 0xA, 0xB, 0xC)
    ]
    var masks32 = [
        int32x4(1337, 0x1337, 0x42, 42),
        int32x4(0x00FF1CE, 0xBAADF00D, 0xDEADBEEF, 0xCAFED00D),
        int32x4(0xD15EA5E, 0xDEADC0DE, 0xFACEB00C, 0x4B1D4B1D)
    ];
    var masks = [];
    var maskType = selectMaskType(type);
    if (maskType == SIMD.int8x16)
        masks = masks8;
    else if (maskType == SIMD.int16x8)
        masks = masks16;
    else if (maskType == SIMD.int32x4)
        masks = masks32;
    else
        throw new Error("Unknown mask type.");

    var x, y;
    var ScalarTypedArray = findCorrespondingScalarTypedArray(type);
    for (var mask of masks) {
        for ([x, y] of inputs)
            assertEqVec(type.bitselect(mask, x, y), bitselect(type, mask, x, y));
    }
}

function test() {
    var inputs = [
        [int8x16(0,4,9,16,25,36,49,64,81,121,-4,-9,-16,-25,-36,-49), int8x16(1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16)],
        [int8x16(-1, 2, INT8_MAX, INT8_MIN, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16),
         int8x16(INT8_MAX, -4, INT8_MIN, 42, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16)]
    ];

    testSelect(int8x16, inputs);
    testBitSelectSimple(int8x16, inputs);
    testBitSelectComplex(int8x16, inputs);

    inputs = [
        [int16x8(0,4,9,16,25,36,49,64), int16x8(1,2,3,4,5,6,7,8)],
        [int16x8(-1, 2, INT16_MAX, INT16_MIN, 5, 6, 7, 8),
         int16x8(INT16_MAX, -4, INT16_MIN, 42, 5, 6, 7, 8)]
    ];

    testSelect(int16x8, inputs);
    testBitSelectSimple(int16x8, inputs);
    testBitSelectComplex(int16x8, inputs);

    inputs = [
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
