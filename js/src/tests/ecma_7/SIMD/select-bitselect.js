// |reftest| skip-if(!this.hasOwnProperty("SIMD"))

/*
 * Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/
 */

var float32x4 = SIMD.float32x4;
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
            assertEqX4(type.select(mask, x, y), select(mask, x, y));
    }
}

function bitselect(ScalarTypedArray, mask, ifTrue, ifFalse) {
    var m = simdToArray(mask);

    var tv = new ScalarTypedArray(simdToArray(ifTrue));
    var fv = new ScalarTypedArray(simdToArray(ifFalse));

    tv = new Int32Array(tv.buffer);
    fv = new Int32Array(fv.buffer);
    var res = new Int32Array(4);

    for (var i = 0; i < 4; i++) {
        var t = 0;
        for (var bit = 0; bit < 32; bit++) {
            var readVal = (m[i] >> bit) & 1 ? tv[i] : fv[i];
            var readBit = (readVal >> bit) & 1;
            t |= readBit << bit;
        }
        res[i] = t;
    }

    res = new ScalarTypedArray(res.buffer);
    return Array.prototype.map.call(res, x => x);
}

function findCorrespondingScalarTypedArray(type) {
    switch (type) {
        case int32x4: return Int32Array;
        case float32x4: return Float32Array;
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
            assertEqX4(type.bitselect(mask, x, y), bitselect(ScalarTypedArray, mask, x, y));
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
            assertEqX4(type.bitselect(mask, x, y), bitselect(ScalarTypedArray, mask, x, y));
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

    if (typeof reportCompare === "function")
        reportCompare(true, true);
}

test();
