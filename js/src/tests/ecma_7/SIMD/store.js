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
    SIMD[kind].storeX(ta, i, v);
    assertChanged(ta, i, [v.x]);

    reset(ta);
    SIMD[kind].store(ta, i, v);
    assertChanged(ta, i, simdToArray(v));

    if (simdLength(v) > 2) {
        reset(ta);
        SIMD[kind].storeXY(ta, i, v);
        assertChanged(ta, i, [v.x, v.y]);

        reset(ta);
        SIMD[kind].storeXYZ(ta, i, v);
        assertChanged(ta, i, [v.x, v.y, v.z]);
    }
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

    var F32 = new SharedFloat32Array(TA.buffer);
    var F64 = new SharedFloat64Array(TA.buffer);

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
        SIMD.int32x4.store(ta, 0, int32x4);
        for (var i = 0; i < 4; i++) assertEq(I32[i], [1, 2, 3, 4][i]);

        SIMD.float32x4.store(ta, 0, float32x4);
        for (var i = 0; i < 4; i++) assertEq(F32[i], [1, 2, 3, 4][i]);

        SIMD.float64x2.store(ta, 0, float64x2);
        for (var i = 0; i < 2; i++) assertEq(F64[i], [1, 2][i]);

        assertThrowsInstanceOf(() => SIMD.int32x4.store(ta, 1024, int32x4), RangeError);
        assertThrowsInstanceOf(() => SIMD.float32x4.store(ta, 1024, float32x4), RangeError);
        assertThrowsInstanceOf(() => SIMD.float64x2.store(ta, 1024, float64x2), RangeError);
    }
}

testStoreInt32x4();
testStoreFloat32x4();
testStoreFloat64x2();
testSharedArrayBufferCompat();

if (typeof reportCompare === "function")
    reportCompare(true, true);
