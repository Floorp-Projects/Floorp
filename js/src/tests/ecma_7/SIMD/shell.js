function makeFloat(sign, exp, mantissa) {
    assertEq(sign, sign & 0x1);
    assertEq(exp, exp & 0xFF);
    assertEq(mantissa, mantissa & 0x7FFFFF);

    var i32 = new Int32Array(1);
    var f32 = new Float32Array(i32.buffer);

    i32[0] = (sign << 31) | (exp << 23) | mantissa;
    return f32[0];
}

function makeDouble(sign, exp, mantissa) {
    assertEq(sign, sign & 0x1);
    assertEq(exp, exp & 0x7FF);

    // Can't use bitwise operations on mantissa, as it might be a double
    assertEq(mantissa <= 0xfffffffffffff, true);
    var highBits = (mantissa / Math.pow(2, 32)) | 0;
    var lowBits = mantissa - highBits * Math.pow(2, 32);

    var i32 = new Int32Array(2);
    var f64 = new Float64Array(i32.buffer);

    // Note that this assumes little-endian order, which is the case on tier-1
    // platforms.
    i32[0] = lowBits;
    i32[1] = (sign << 31) | (exp << 20) | highBits;
    return f64[0];
}

function GetType(v) {
    switch (Object.getPrototypeOf(v)) {
        case SIMD.Int8x16.prototype:   return SIMD.Int8x16;
        case SIMD.Int16x8.prototype:   return SIMD.Int16x8;
        case SIMD.Int32x4.prototype:   return SIMD.Int32x4;
        case SIMD.Uint8x16.prototype:  return SIMD.Uint8x16;
        case SIMD.Uint16x8.prototype:  return SIMD.Uint16x8;
        case SIMD.Uint32x4.prototype:  return SIMD.Uint32x4;
        case SIMD.Float32x4.prototype: return SIMD.Float32x4;
        case SIMD.Float64x2.prototype: return SIMD.Float64x2;
        case SIMD.Bool8x16.prototype:  return SIMD.Bool8x16;
        case SIMD.Bool16x8.prototype:  return SIMD.Bool16x8;
        case SIMD.Bool32x4.prototype:  return SIMD.Bool32x4;
        case SIMD.Bool64x2.prototype:  return SIMD.Bool64x2;
    }
}

function assertEqFloat64x2(v, arr) {
    try {
        assertEq(SIMD.Float64x2.extractLane(v, 0), arr[0]);
        assertEq(SIMD.Float64x2.extractLane(v, 1), arr[1]);
    } catch (e) {
        print("stack trace:", e.stack);
        throw e;
    }
}

function assertEqBool64x2(v, arr) {
    try {
        assertEq(SIMD.Bool64x2.extractLane(v, 0), arr[0]);
        assertEq(SIMD.Bool64x2.extractLane(v, 1), arr[1]);
    } catch (e) {
        print("stack trace:", e.stack);
        throw e;
    }
}

function assertEqX2(v, arr) {
    var Type = GetType(v);
    if (Type === SIMD.Float64x2) assertEqFloat64x2(v, arr);
    else if (Type === SIMD.Bool64x2) assertEqBool64x2(v, arr);
    else throw new TypeError("Unknown SIMD kind.");
}

function assertEqInt32x4(v, arr) {
    try {
        for (var i = 0; i < 4; i++)
            assertEq(SIMD.Int32x4.extractLane(v, i), arr[i]);
    } catch (e) {
        print("stack trace:", e.stack);
        throw e;
    }
}

function assertEqUint32x4(v, arr) {
    try {
        for (var i = 0; i < 4; i++)
            assertEq(SIMD.Uint32x4.extractLane(v, i), arr[i]);
    } catch (e) {
        print("stack trace:", e.stack);
        throw e;
    }
}

function assertEqFloat32x4(v, arr) {
    try {
        for (var i = 0; i < 4; i++)
            assertEq(SIMD.Float32x4.extractLane(v, i), arr[i]);
    } catch (e) {
        print("stack trace:", e.stack);
        throw e;
    }
}

function assertEqBool32x4(v, arr) {
    try {
        for (var i = 0; i < 4; i++)
            assertEq(SIMD.Bool32x4.extractLane(v, i), arr[i]);
    } catch (e) {
        print("stack trace:", e.stack);
        throw e;
    }
}

function assertEqX4(v, arr) {
    var Type = GetType(v);
    if (Type === SIMD.Int32x4) assertEqInt32x4(v, arr);
    else if (Type === SIMD.Uint32x4) assertEqUint32x4(v, arr);
    else if (Type === SIMD.Float32x4) assertEqFloat32x4(v, arr);
    else if (Type === SIMD.Bool32x4) assertEqBool32x4(v, arr);
    else throw new TypeError("Unknown SIMD kind.");
}

function assertEqInt16x8(v, arr) {
    try {
        for (var i = 0; i < 8; i++)
            assertEq(SIMD.Int16x8.extractLane(v, i), arr[i]);
    } catch (e) {
        print("stack trace:", e.stack);
        throw e;
    }
}

function assertEqUint16x8(v, arr) {
    try {
        for (var i = 0; i < 8; i++)
            assertEq(SIMD.Uint16x8.extractLane(v, i), arr[i]);
    } catch (e) {
        print("stack trace:", e.stack);
        throw e;
    }
}

function assertEqBool16x8(v, arr) {
    try {
        for (var i = 0; i < 8; i++){
            assertEq(SIMD.Bool16x8.extractLane(v, i), arr[i]);
        }
    } catch (e) {
        print("stack trace:", e.stack);
        throw e;
    }
}

function assertEqX8(v, arr) {
    var Type = GetType(v);
    if (Type === SIMD.Int16x8) assertEqInt16x8(v, arr);
    else if (Type === SIMD.Uint16x8) assertEqUint16x8(v, arr);
    else if (Type === SIMD.Bool16x8) assertEqBool16x8(v, arr);
    else throw new TypeError("Unknown x8 vector.");
}

function assertEqInt8x16(v, arr) {
    try {
        for (var i = 0; i < 16; i++)
            assertEq(SIMD.Int8x16.extractLane(v, i), arr[i]);
    } catch (e) {
        print("stack trace:", e.stack);
        throw e;
    }
}

function assertEqUint8x16(v, arr) {
    try {
        for (var i = 0; i < 16; i++)
            assertEq(SIMD.Uint8x16.extractLane(v, i), arr[i]);
    } catch (e) {
        print("stack trace:", e.stack);
        throw e;
    }
}

function assertEqBool8x16(v, arr) {
    try {
        for (var i = 0; i < 16; i++)
            assertEq(SIMD.Bool8x16.extractLane(v, i), arr[i]);
    } catch (e) {
        print("stack trace:", e.stack);
        throw e;
    }
}

function assertEqX16(v, arr) {
    var Type = GetType(v);
    if (Type === SIMD.Int8x16) assertEqInt8x16(v, arr);
    else if (Type === SIMD.Uint8x16) assertEqUint8x16(v, arr);
    else if (Type === SIMD.Bool8x16) assertEqBool8x16(v, arr);
    else throw new TypeError("Unknown x16 vector.");
}

function simdLength(v) {
    var pt = Object.getPrototypeOf(v);
    if (pt == SIMD.Int8x16.prototype || pt == SIMD.Uint8x16.prototype ||
            pt === SIMD.Bool8x16.prototype)
        return 16;
    if (pt == SIMD.Int16x8.prototype || pt == SIMD.Uint16x8.prototype ||
            pt === SIMD.Bool16x8.prototype)
        return 8;
    if (pt === SIMD.Int32x4.prototype || pt === SIMD.Uint32x4.prototype ||
            pt === SIMD.Float32x4.prototype || pt === SIMD.Bool32x4.prototype)
        return 4;
    if (pt === SIMD.Float64x2.prototype || pt == SIMD.Bool64x2.prototype)
        return 2;
    throw new TypeError("Unknown SIMD kind.");
}

function simdLengthType(t) {
    if (t == SIMD.Int8x16 || t == SIMD.Uint8x16 || t == SIMD.Bool8x16)
        return 16;
    else if (t == SIMD.Int16x8 || t == SIMD.Uint16x8 || t == SIMD.Bool16x8)
        return 8;
    else if (t == SIMD.Int32x4 || t == SIMD.Uint32x4 || t == SIMD.Float32x4 || t == SIMD.Bool32x4)
        return 4;
    else if (t == SIMD.Float64x2 || t == SIMD.Bool64x2)
        return 2;
    else
        throw new TypeError("Unknown SIMD kind.");
}

function getAssertFuncFromLength(l) {
    if (l == 2)
        return assertEqX2;
    else if (l == 4)
        return assertEqX4;
    else if (l == 8)
        return assertEqX8;
    else if (l == 16)
        return assertEqX16;
    else
        throw new TypeError("Unknown SIMD kind.");
}

function assertEqVec(v, arr) {
    var Type = GetType(v);
    if (Type === SIMD.Int8x16) assertEqInt8x16(v, arr);
    else if (Type === SIMD.Int16x8) assertEqInt16x8(v, arr);
    else if (Type === SIMD.Int32x4) assertEqInt32x4(v, arr);
    else if (Type === SIMD.Uint8x16) assertEqUint8x16(v, arr);
    else if (Type === SIMD.Uint16x8) assertEqUint16x8(v, arr);
    else if (Type === SIMD.Uint32x4) assertEqUint32x4(v, arr);
    else if (Type === SIMD.Float32x4) assertEqFloat32x4(v, arr);
    else if (Type === SIMD.Float64x2) assertEqFloat64x2(v, arr);
    else if (Type === SIMD.Bool8x16) assertEqBool8x16(v, arr);
    else if (Type === SIMD.Bool16x8) assertEqBool16x8(v, arr);
    else if (Type === SIMD.Bool32x4) assertEqBool32x4(v, arr);
    else if (Type === SIMD.Bool64x2) assertEqBool64x2(v, arr);
    else throw new TypeError("Unknown SIMD Kind");
}

function simdToArray(v) {
    var Type = GetType(v);

    function indexes(n) {
        var arr = [];
        for (var i = 0; i < n; i++) arr.push(i);
        return arr;
    }

    if (Type === SIMD.Bool8x16) {
        return indexes(16).map((i) => SIMD.Bool8x16.extractLane(v, i));
    }

    if (Type === SIMD.Bool16x8) {
        return indexes(8).map((i) => SIMD.Bool16x8.extractLane(v, i));
    }

    if (Type === SIMD.Bool32x4) {
        return indexes(4).map((i) => SIMD.Bool32x4.extractLane(v, i));
    }

    if (Type === SIMD.Bool64x2) {
        return indexes(2).map((i) => SIMD.Bool64x2.extractLane(v, i));
    }

    if (Type === SIMD.Int8x16) {
        return indexes(16).map((i) => SIMD.Int8x16.extractLane(v, i));
    }

    if (Type === SIMD.Int16x8) {
        return indexes(8).map((i) => SIMD.Int16x8.extractLane(v, i));
    }

    if (Type === SIMD.Int32x4) {
        return indexes(4).map((i) => SIMD.Int32x4.extractLane(v, i));
    }

    if (Type === SIMD.Uint8x16) {
        return indexes(16).map((i) => SIMD.Uint8x16.extractLane(v, i));
    }

    if (Type === SIMD.Uint16x8) {
        return indexes(8).map((i) => SIMD.Uint16x8.extractLane(v, i));
    }

    if (Type === SIMD.Uint32x4) {
        return indexes(4).map((i) => SIMD.Uint32x4.extractLane(v, i));
    }

    if (Type === SIMD.Float32x4) {
        return indexes(4).map((i) => SIMD.Float32x4.extractLane(v, i));
    }

    if (Type === SIMD.Float64x2) {
        return indexes(2).map((i) => SIMD.Float64x2.extractLane(v, i));
    }

    throw new TypeError("Unknown SIMD Kind");
}

const INT8_MAX = Math.pow(2, 7) -1;
const INT8_MIN = -Math.pow(2, 7);
assertEq((INT8_MAX + 1) << 24 >> 24, INT8_MIN);
const INT16_MAX = Math.pow(2, 15) - 1;
const INT16_MIN = -Math.pow(2, 15);
assertEq((INT16_MAX + 1) << 16 >> 16, INT16_MIN);
const INT32_MAX = Math.pow(2, 31) - 1;
const INT32_MIN = -Math.pow(2, 31);
assertEq(INT32_MAX + 1 | 0, INT32_MIN);

const UINT8_MAX = Math.pow(2, 8) - 1;
const UINT16_MAX = Math.pow(2, 16) - 1;
const UINT32_MAX = Math.pow(2, 32) - 1;

function testUnaryFunc(v, simdFunc, func) {
    var varr = simdToArray(v);

    var observed = simdToArray(simdFunc(v));
    var expected = varr.map(function(v, i) { return func(varr[i]); });

    for (var i = 0; i < observed.length; i++)
        assertEq(observed[i], expected[i]);
}

function testBinaryFunc(v, w, simdFunc, func) {
    var varr = simdToArray(v);
    var warr = simdToArray(w);

    var observed = simdToArray(simdFunc(v, w));
    var expected = varr.map(function(v, i) { return func(varr[i], warr[i]); });

    for (var i = 0; i < observed.length; i++)
        assertEq(observed[i], expected[i]);
}

function testBinaryCompare(v, w, simdFunc, func, outType) {
    var varr = simdToArray(v);
    var warr = simdToArray(w);

    var inLanes = simdLength(v);
    var observed = simdToArray(simdFunc(v, w));
    var outTypeLen = simdLengthType(outType);
    assertEq(observed.length, outTypeLen);
    for (var i = 0; i < outTypeLen; i++) {
        var j = ((i * inLanes) / outTypeLen) | 0;
        assertEq(observed[i], func(varr[j], warr[j]));
    }
}

function testBinaryScalarFunc(v, scalar, simdFunc, func) {
    var varr = simdToArray(v);

    var observed = simdToArray(simdFunc(v, scalar));
    var expected = varr.map(function(v, i) { return func(varr[i], scalar); });

    for (var i = 0; i < observed.length; i++)
        assertEq(observed[i], expected[i]);
}
