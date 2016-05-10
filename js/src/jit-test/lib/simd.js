if (!this.hasOwnProperty("SIMD"))
    quit();

function booleanBinaryX4(op, v, w) {
    var arr = [];
    var [varr, warr] = [simdToArray(v), simdToArray(w)];
    for (var i = 0; i < 4; i++)
        arr[i] = op(varr[i], warr[i]);
    return arr;
}

function binaryX4(op, v, w) {
    var arr = [];
    var [varr, warr] = [simdToArray(v), simdToArray(w)];
    [varr, warr] = [varr.map(Math.fround), warr.map(Math.fround)];
    for (var i = 0; i < 4; i++)
        arr[i] = op(varr[i], warr[i]);
    return arr.map(Math.fround);
}

function unaryX4(op, v, coerceFunc) {
    var arr = [];
    var varr = simdToArray(v).map(coerceFunc);
    for (var i = 0; i < 4; i++)
        arr[i] = op(varr[i]);
    return arr.map(coerceFunc);
}

function assertNear(a, b) {
    assertEq((a != a && b != b) || Math.abs(a - b) < 0.001, true);
}

function GetType(v) {
    var pt = Object.getPrototypeOf(v);
    switch (pt) {
        case SIMD.Int8x16.prototype: return SIMD.Int8x16;
        case SIMD.Int16x8.prototype: return SIMD.Int16x8;
        case SIMD.Int32x4.prototype: return SIMD.Int32x4;
        case SIMD.Uint8x16.prototype: return SIMD.Uint8x16;
        case SIMD.Uint16x8.prototype: return SIMD.Uint16x8;
        case SIMD.Uint32x4.prototype: return SIMD.Uint32x4;
        case SIMD.Float32x4.prototype: return SIMD.Float32x4;
        case SIMD.Bool8x16.prototype: return SIMD.Bool8x16;
        case SIMD.Bool16x8.prototype: return SIMD.Bool16x8;
        case SIMD.Bool32x4.prototype: return SIMD.Bool32x4;
    }
    throw "unexpected SIMD type";
}

function GetLength(t) {
    switch (t) {
      case SIMD.Int8x16: return 16;
      case SIMD.Int16x8: return 8;
      case SIMD.Int32x4: return 4;
      case SIMD.Uint8x16: return 16;
      case SIMD.Uint16x8: return 8;
      case SIMD.Uint32x4: return 4;
      case SIMD.Float32x4: return 4;
      case SIMD.Bool8x16: return 16;
      case SIMD.Bool16x8: return 8;
      case SIMD.Bool32x4: return 4;
    }
    throw "unexpected SIMD type";
}

function assertEqVec(v, w) {
    var typeV = GetType(v);
    var lengthV = GetLength(typeV);
    var ext = typeV.extractLane;
    assertEq(GetType(w), typeV);
    for (var i = 0; i < lengthV; i++)
        assertEq(ext(v, i), ext(w, i));
}

function assertEqVecArr(v, w) {
    var typeV = GetType(v);
    var lengthV = GetLength(typeV);
    var ext = typeV.extractLane;
    assertEq(w.length, lengthV);

    for (var i = 0; i < lengthV; i++)
        assertEq(ext(v, i), w[i]);
}

function assertEqX4(vec, arr, ...opts) {

    var assertFunc;
    if (opts.length == 1 && typeof opts[0] !== 'undefined') {
        assertFunc = opts[0];
    } else {
        assertFunc = assertEq;
    }

    var Type = GetType(vec);

    assertFunc(Type.extractLane(vec, 0), arr[0]);
    assertFunc(Type.extractLane(vec, 1), arr[1]);
    assertFunc(Type.extractLane(vec, 2), arr[2]);
    assertFunc(Type.extractLane(vec, 3), arr[3]);
}

function simdToArray(vec) {
    var Type = GetType(vec);
    var Length = GetLength(Type);
    var a = [];
    for (var i = 0; i < Length; i++)
        a.push(Type.extractLane(vec, i));
    return a;
}
