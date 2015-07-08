// |reftest| skip-if(!this.hasOwnProperty("SIMD"))

/*
 * Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/
 */

// As SIMD.*.store is entirely symmetric to SIMD.*.load, this file just
// contains basic tests to store on one single TypedArray kind, while load is
// exhaustively tested. See load.js for more details.

const POISON = 42;

function reset(ta) {
    for (var i = 0; i < ta.length; i++)
        ta[i] = POISON + i;
}

function assertChanged(ta, from, expected) {
    var i = 0;
    for (; i < from; i++)
        assertEq(ta[i], POISON + i);
    for (; i < from + expected.length; i++)
        assertEq(ta[i], expected[i - from]);
    for (; i < ta.length; i++)
        assertEq(ta[i], POISON + i);
}

function testStore(ta, kind, i, v) {
    reset(ta);
    SIMD[kind].store(ta, i, v);
    assertChanged(ta, i, simdToArray(v));

    var length = simdLength(v);
    if (length >= 8) // int8x16 and int16x8 only support store, and not store1/store2/etc.
        return;

    reset(ta);
    SIMD[kind].store1(ta, i, v);
    assertChanged(ta, i, [v.x]);
    if (length > 2) {
        reset(ta);
        SIMD[kind].store2(ta, i, v);
        assertChanged(ta, i, [v.x, v.y]);

        reset(ta);
        SIMD[kind].store3(ta, i, v);
        assertChanged(ta, i, [v.x, v.y, v.z]);
    }
}

function testStoreInt8x16() {
    var I8 = new Int8Array(32);

    var v = SIMD.int8x16(0, 1, INT8_MAX, INT8_MIN, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
    testStore(I8, 'int8x16', 0, v);
    testStore(I8, 'int8x16', 1, v);
    testStore(I8, 'int8x16', 2, v);
    testStore(I8, 'int8x16', 16, v);

    assertThrowsInstanceOf(() => SIMD.int8x16.store(I8), TypeError);
    assertThrowsInstanceOf(() => SIMD.int8x16.store(I8, 0), TypeError);
    assertThrowsInstanceOf(() => SIMD.int16x8.store(I8, 0, v), TypeError);
}

function testStoreInt16x8() {
    var I16 = new Int16Array(32);

    var v = SIMD.int16x8(0, 1, INT16_MAX, INT16_MIN, 4, 5, 6, 7);
    testStore(I16, 'int16x8', 0, v);
    testStore(I16, 'int16x8', 1, v);
    testStore(I16, 'int16x8', 2, v);
    testStore(I16, 'int16x8', 24, v);

    assertThrowsInstanceOf(() => SIMD.int16x8.store(I16), TypeError);
    assertThrowsInstanceOf(() => SIMD.int16x8.store(I16, 0), TypeError);
    assertThrowsInstanceOf(() => SIMD.int8x16.store(I16, 0, v), TypeError);
}

function testStoreInt32x4() {
    var I32 = new Int32Array(16);

    var v = SIMD.int32x4(0, 1, Math.pow(2,31) - 1, -Math.pow(2, 31));
    testStore(I32, 'int32x4', 0, v);
    testStore(I32, 'int32x4', 1, v);
    testStore(I32, 'int32x4', 2, v);
    testStore(I32, 'int32x4', 12, v);

    assertThrowsInstanceOf(() => SIMD.int32x4.store(I32), TypeError);
    assertThrowsInstanceOf(() => SIMD.int32x4.store(I32, 0), TypeError);
    assertThrowsInstanceOf(() => SIMD.float32x4.store(I32, 0, v), TypeError);
}

function testStoreFloat32x4() {
    var F32 = new Float32Array(16);

    var v = SIMD.float32x4(1,2,3,4);
    testStore(F32, 'float32x4', 0, v);
    testStore(F32, 'float32x4', 1, v);
    testStore(F32, 'float32x4', 2, v);
    testStore(F32, 'float32x4', 12, v);

    var v = SIMD.float32x4(NaN, -0, -Infinity, 5e-324);
    testStore(F32, 'float32x4', 0, v);
    testStore(F32, 'float32x4', 1, v);
    testStore(F32, 'float32x4', 2, v);
    testStore(F32, 'float32x4', 12, v);

    assertThrowsInstanceOf(() => SIMD.float32x4.store(F32), TypeError);
    assertThrowsInstanceOf(() => SIMD.float32x4.store(F32, 0), TypeError);
    assertThrowsInstanceOf(() => SIMD.int32x4.store(F32, 0, v), TypeError);
}

function testStoreFloat64x2() {
    var F64 = new Float64Array(16);

    var v = SIMD.float64x2(1, 2);
    testStore(F64, 'float64x2', 0, v);
    testStore(F64, 'float64x2', 1, v);
    testStore(F64, 'float64x2', 14, v);

    var v = SIMD.float64x2(NaN, -0);
    testStore(F64, 'float64x2', 0, v);
    testStore(F64, 'float64x2', 1, v);
    testStore(F64, 'float64x2', 14, v);

    var v = SIMD.float64x2(-Infinity, +Infinity);
    testStore(F64, 'float64x2', 0, v);
    testStore(F64, 'float64x2', 1, v);
    testStore(F64, 'float64x2', 14, v);

    assertThrowsInstanceOf(() => SIMD.float64x2.store(F64), TypeError);
    assertThrowsInstanceOf(() => SIMD.float64x2.store(F64, 0), TypeError);
    assertThrowsInstanceOf(() => SIMD.float32x4.store(F64, 0, v), TypeError);
}

function testSharedArrayBufferCompat() {
    if (!this.SharedArrayBuffer || !this.SharedFloat32Array || !this.Atomics)
        return;

    var I32 = new SharedInt32Array(16);
    var TA = I32;

    var I8 = new SharedInt8Array(TA.buffer);
    var I16 = new SharedInt16Array(TA.buffer);
    var F32 = new SharedFloat32Array(TA.buffer);
    var F64 = new SharedFloat64Array(TA.buffer);

    var int8x16 = SIMD.int8x16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
    var int16x8 = SIMD.int16x8(1, 2, 3, 4, 5, 6, 7, 8);
    var int32x4 = SIMD.int32x4(1, 2, 3, 4);
    var float32x4 = SIMD.float32x4(1, 2, 3, 4);
    var float64x2 = SIMD.float64x2(1, 2);

    for (var ta of [
                    new SharedUint8Array(TA.buffer),
                    new SharedInt8Array(TA.buffer),
                    new SharedUint16Array(TA.buffer),
                    new SharedInt16Array(TA.buffer),
                    new SharedUint32Array(TA.buffer),
                    new SharedInt32Array(TA.buffer),
                    new SharedFloat32Array(TA.buffer),
                    new SharedFloat64Array(TA.buffer)
                   ])
    {
        SIMD.int8x16.store(ta, 0, int8x16);
        for (var i = 0; i < 16; i++) assertEq(I8[i], [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16][i]);

        SIMD.int16x8.store(ta, 0, int16x8);
        for (var i = 0; i < 8; i++) assertEq(I16[i], [1, 2, 3, 4, 5, 6, 7, 8][i]);

        SIMD.int32x4.store(ta, 0, int32x4);
        for (var i = 0; i < 4; i++) assertEq(I32[i], [1, 2, 3, 4][i]);

        SIMD.float32x4.store(ta, 0, float32x4);
        for (var i = 0; i < 4; i++) assertEq(F32[i], [1, 2, 3, 4][i]);

        SIMD.float64x2.store(ta, 0, float64x2);
        for (var i = 0; i < 2; i++) assertEq(F64[i], [1, 2][i]);

        assertThrowsInstanceOf(() => SIMD.int8x16.store(ta, 1024, int8x16), RangeError);
        assertThrowsInstanceOf(() => SIMD.int16x8.store(ta, 1024, int16x8), RangeError);
        assertThrowsInstanceOf(() => SIMD.int32x4.store(ta, 1024, int32x4), RangeError);
        assertThrowsInstanceOf(() => SIMD.float32x4.store(ta, 1024, float32x4), RangeError);
        assertThrowsInstanceOf(() => SIMD.float64x2.store(ta, 1024, float64x2), RangeError);
    }
}

testStoreInt8x16();
testStoreInt16x8();
testStoreInt32x4();
testStoreFloat32x4();
testStoreFloat64x2();
testSharedArrayBufferCompat();

if (typeof reportCompare === "function")
    reportCompare(true, true);
