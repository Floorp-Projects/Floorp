// |reftest| skip-if(!this.hasOwnProperty("SIMD"))

/*
 * Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/
 */

var Float32x4 = SIMD.Float32x4;
var Float64x2 = SIMD.Float64x2;
var Int8x16 = SIMD.Int8x16;
var Int16x8 = SIMD.Int16x8;
var Int32x4 = SIMD.Int32x4;

function getMask(i, maskLength) {
    var args = [];
    for (var j = 0; j < maskLength; j++) args.push(!!((i >> j) & 1));
    if (maskLength == 4)
        return Int32x4.bool(...args);
    else if (maskLength == 8)
        return Int16x8.bool(...args);
    else if (maskLength == 16)
        return Int8x16.bool(...args);
    else
        throw new Error("Invalid mask length.");
}

function selectMaskType(type) {
    if (type == Int32x4 || type == Float32x4 || type == Float64x2)
        return Int32x4;
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
 * has 4 lanes (for Int32x4) and 2 possible values (true or false), there are 16 possible
 * masks. For Int8x16, the mask has 16 lanes and 2 possible values, so there are 256
 * possible masks. For Int16x8, the mask has 8 lanes and 2 possible values, so there
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
      case Float32x4:
          return Int32x4.fromFloat32x4Bits(vec);
      case Float64x2:
          return Int32x4.fromFloat64x2Bits(vec);
      case Int8x16:
          return vec;
      case Int16x8:
          return vec;
      case Int32x4:
          return vec;
      default:
          throw new TypeError("Unknown SIMD type.");
    }
}

function selectBits(type, mask, ifTrue, ifFalse) {
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
        case Int8x16: return Int8Array;
        case Int16x8: return Int16Array;
        case Int32x4: return Int32Array;
        case Float32x4: return Float32Array;
        case Float64x2: return Float64Array;
        default: throw new Error("undefined scalar typed array");
    }
}

/**
 * This tests type.selectBits on all boolean masks, as in select. For these,
 *          selectBits(mask, x, y) === select(mask, x, y)
 */
function testSelectBitsSimple(type, inputs) {
    var x, y;
    var maskLength = simdLengthType(type);
    maskLength = maskLength != 2 ? maskLength : 4;
    var ScalarTypedArray = findCorrespondingScalarTypedArray(type);
    for (var i = 0; i < Math.pow(maskLength, 2); i++) {
        var mask = getMask(i, maskLength);
        for ([x, y] of inputs)
            assertEqVec(type.selectBits(mask, x, y), selectBits(type, mask, x, y));
    }
}

/**
 * This tests type.selectBits on a few hand-defined masks. For these,
 *          selectBits(mask, x, y) !== select(mask, x, y)
 */
function testSelectBitsComplex(type, inputs) {
    var masks8 = [
        Int8x16(0x42, 42, INT8_MAX, INT8_MIN, INT8_MAX + 1, INT8_MIN - 1, 13, 37, -42, 125, -125, -1, 1, 0xA, 0xB, 0xC)
    ]
    var masks16 = [
        Int16x8(0x42, 42, INT16_MAX, INT16_MIN, INT16_MAX + 1, INT16_MIN - 1, 13, 37),
        Int16x8(-42, 125, -125, -1, 1, 0xA, 0xB, 0xC)
    ]
    var masks32 = [
        Int32x4(1337, 0x1337, 0x42, 42),
        Int32x4(0x00FF1CE, 0xBAADF00D, 0xDEADBEEF, 0xCAFED00D),
        Int32x4(0xD15EA5E, 0xDEADC0DE, 0xFACEB00C, 0x4B1D4B1D)
    ];
    var masks = [];
    var maskType = selectMaskType(type);
    if (maskType == SIMD.Int8x16)
        masks = masks8;
    else if (maskType == SIMD.Int16x8)
        masks = masks16;
    else if (maskType == SIMD.Int32x4)
        masks = masks32;
    else
        throw new Error("Unknown mask type.");

    var x, y;
    var ScalarTypedArray = findCorrespondingScalarTypedArray(type);
    for (var mask of masks) {
        for ([x, y] of inputs)
            assertEqVec(type.selectBits(mask, x, y), selectBits(type, mask, x, y));
    }
}

function test() {
    var inputs = [
        [Int8x16(0,4,9,16,25,36,49,64,81,121,-4,-9,-16,-25,-36,-49), Int8x16(1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16)],
        [Int8x16(-1, 2, INT8_MAX, INT8_MIN, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16),
         Int8x16(INT8_MAX, -4, INT8_MIN, 42, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16)]
    ];

    testSelect(Int8x16, inputs);
    testSelectBitsSimple(Int8x16, inputs);
    testSelectBitsComplex(Int8x16, inputs);

    inputs = [
        [Int16x8(0,4,9,16,25,36,49,64), Int16x8(1,2,3,4,5,6,7,8)],
        [Int16x8(-1, 2, INT16_MAX, INT16_MIN, 5, 6, 7, 8),
         Int16x8(INT16_MAX, -4, INT16_MIN, 42, 5, 6, 7, 8)]
    ];

    testSelect(Int16x8, inputs);
    testSelectBitsSimple(Int16x8, inputs);
    testSelectBitsComplex(Int16x8, inputs);

    inputs = [
        [Int32x4(0,4,9,16), Int32x4(1,2,3,4)],
        [Int32x4(-1, 2, INT32_MAX, INT32_MIN), Int32x4(INT32_MAX, -4, INT32_MIN, 42)]
    ];

    testSelect(Int32x4, inputs);
    testSelectBitsSimple(Int32x4, inputs);
    testSelectBitsComplex(Int32x4, inputs);

    inputs = [
        [Float32x4(0.125,4.25,9.75,16.125), Float32x4(1.5,2.75,3.25,4.5)],
        [Float32x4(-1.5,-0,NaN,-Infinity), Float32x4(1,-2,13.37,3.13)],
        [Float32x4(1.5,2.75,NaN,Infinity), Float32x4(-NaN,-Infinity,9.75,16.125)]
    ];

    testSelect(Float32x4, inputs);
    testSelectBitsSimple(Float32x4, inputs);
    testSelectBitsComplex(Float32x4, inputs);

    inputs = [
        [Float64x2(0.125,4.25), Float64x2(9.75,16.125)],
        [Float64x2(1.5,2.75), Float64x2(3.25,4.5)],
        [Float64x2(-1.5,-0), Float64x2(NaN,-Infinity)],
        [Float64x2(1,-2), Float64x2(13.37,3.13)],
        [Float64x2(1.5,2.75), Float64x2(NaN,Infinity)],
        [Float64x2(-NaN,-Infinity), Float64x2(9.75,16.125)]
    ];

    testSelect(Float64x2, inputs);
    testSelectBitsSimple(Float64x2, inputs);
    testSelectBitsComplex(Float64x2, inputs);

    if (typeof reportCompare === "function")
        reportCompare(true, true);
}

test();
