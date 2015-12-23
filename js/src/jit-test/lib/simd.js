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
        case SIMD.Int32x4.prototype: return SIMD.Int32x4;
        case SIMD.Float32x4.prototype: return SIMD.Float32x4;
        case SIMD.Bool32x4.prototype: return SIMD.Bool32x4;
    }
    throw "unexpected SIMD type";
}

function assertEqVec(v, w) {
    var typeV = GetType(v);
    var ext = typeV.extractLane;
    assertEq(GetType(w), typeV);
    if (typeV === SIMD.Int32x4 || typeV === SIMD.Float32x4 || typeV === SIMD.Bool32x4) {
        [0, 1, 2, 3].forEach((i) => assertEq(ext(v, i), ext(w, i)));
        return;
    }
}

function assertEqX4(vec, arr, ...opts) {

    var assertFunc;
    if (opts.length == 1 && typeof opts[0] !== 'undefined') {
        assertFunc = opts[0];
    } else {
        assertFunc = assertEq;
    }

    var Type = GetType(vec);
    if (Type === SIMD.Int32x4) {
        assertFunc(SIMD.Int32x4.extractLane(vec, 0), arr[0]);
        assertFunc(SIMD.Int32x4.extractLane(vec, 1), arr[1]);
        assertFunc(SIMD.Int32x4.extractLane(vec, 2), arr[2]);
        assertFunc(SIMD.Int32x4.extractLane(vec, 3), arr[3]);
        return;
    }

    if (Type === SIMD.Float32x4) {
        assertFunc(SIMD.Float32x4.extractLane(vec, 0), arr[0]);
        assertFunc(SIMD.Float32x4.extractLane(vec, 1), arr[1]);
        assertFunc(SIMD.Float32x4.extractLane(vec, 2), arr[2]);
        assertFunc(SIMD.Float32x4.extractLane(vec, 3), arr[3]);
        return;
    }

    if (Type === SIMD.Bool32x4) {
        assertFunc(SIMD.Bool32x4.extractLane(vec, 0), arr[0]);
        assertFunc(SIMD.Bool32x4.extractLane(vec, 1), arr[1]);
        assertFunc(SIMD.Bool32x4.extractLane(vec, 2), arr[2]);
        assertFunc(SIMD.Bool32x4.extractLane(vec, 3), arr[3]);
        return;
    }

    throw "unexpected SIMD type";
}

function simdToArray(vec) {
    var Type = GetType(vec);

    if (Type === SIMD.Int32x4) {
        return [
            SIMD.Int32x4.extractLane(vec, 0),
            SIMD.Int32x4.extractLane(vec, 1),
            SIMD.Int32x4.extractLane(vec, 2),
            SIMD.Int32x4.extractLane(vec, 3),
        ];
    }

    if (Type === SIMD.Float32x4) {
        return [
            SIMD.Float32x4.extractLane(vec, 0),
            SIMD.Float32x4.extractLane(vec, 1),
            SIMD.Float32x4.extractLane(vec, 2),
            SIMD.Float32x4.extractLane(vec, 3),
        ];
    }

    if (Type === SIMD.Bool32x4) {
        return [
            SIMD.Bool32x4.extractLane(vec, 0),
            SIMD.Bool32x4.extractLane(vec, 1),
            SIMD.Bool32x4.extractLane(vec, 2),
            SIMD.Bool32x4.extractLane(vec, 3),
        ];
    }

    throw "unexpected SIMD type";
}
