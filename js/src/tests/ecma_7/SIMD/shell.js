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

function assertEqX2(v, arr) {
    try {
        assertEq(v.x, arr[0]);
        assertEq(v.y, arr[1]);
    } catch (e) {
        print("stack trace:", e.stack);
        throw e;
    }
}

function assertEqX4(v, arr) {
    try {
        assertEq(v.x, arr[0]);
        assertEq(v.y, arr[1]);
        assertEq(v.z, arr[2]);
        assertEq(v.w, arr[3]);
    } catch (e) {
        print("stack trace:", e.stack);
        throw e;
    }
}

function assertEqX8(v, arr) {
    try {
        assertEq(v.s0, arr[0]);
        assertEq(v.s1, arr[1]);
        assertEq(v.s2, arr[2]);
        assertEq(v.s3, arr[3]);
        assertEq(v.s4, arr[4]);
        assertEq(v.s5, arr[5]);
        assertEq(v.s6, arr[6]);
        assertEq(v.s7, arr[7]);
    } catch (e) {
        print("stack trace:", e.stack);
        throw e;
    }
}

function assertEqX16(v, arr) {
    try {
        assertEq(v.s0, arr[0]);
        assertEq(v.s1, arr[1]);
        assertEq(v.s2, arr[2]);
        assertEq(v.s3, arr[3]);
        assertEq(v.s4, arr[4]);
        assertEq(v.s5, arr[5]);
        assertEq(v.s6, arr[6]);
        assertEq(v.s7, arr[7]);
        assertEq(v.s8, arr[8]);
        assertEq(v.s9, arr[9]);
        assertEq(v.s10, arr[10]);
        assertEq(v.s11, arr[11]);
        assertEq(v.s12, arr[12]);
        assertEq(v.s13, arr[13]);
        assertEq(v.s14, arr[14]);
        assertEq(v.s15, arr[15]);
    } catch (e) {
        print("stack trace:", e.stack);
        throw e;
    }
}

function simdLength(v) {
    var pt = Object.getPrototypeOf(v);
    if (pt == SIMD.int8x16.prototype) {
        return 16;
    } else if (pt == SIMD.int16x8.prototype) {
        return 8;
    } else if (pt === SIMD.int32x4.prototype || pt === SIMD.float32x4.prototype) {
        return 4;
    } else if (pt === SIMD.float64x2.prototype) {
        return 2;
    } else {
        throw new TypeError("Unknown SIMD kind.");
    }
}

function simdLengthType(t) {
    if (t == SIMD.int8x16)
        return 16;
    else if (t == SIMD.int16x8)
        return 8;
    else if (t == SIMD.int32x4 || t == SIMD.float32x4)
        return 4;
    else if (t == SIMD.float64x2)
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
    var lanes = simdLength(v);
    var assertFunc = getAssertFuncFromLength(lanes);
    assertFunc(v, arr);
}

function simdToArray(v) {
    var lanes = simdLength(v);
    if (lanes == 4)
        return [v.x, v.y, v.z, v.w];
    else if (lanes == 2)
        return [v.x, v.y];
    else if (lanes == 8)
        return [v.s0, v.s1, v.s2, v.s3, v.s4, v.s5, v.s6, v.s7]
    else if (lanes == 16)
        return [v.s0, v.s1, v.s2, v.s3, v.s4, v.s5, v.s6, v.s7, v.s8, v.s9, v.s10, v.s11, v.s12, v.s13, v.s14, v.s15];
    else
        throw new TypeError("Unknown SIMD kind.");
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
