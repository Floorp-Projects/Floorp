if (!this.hasOwnProperty("SIMD"))
    quit();

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

function assertEqVec(v, w) {
    assertEq(v.x, w.x);
    assertEq(v.y, w.y);
    assertEq(v.z, w.z);
    assertEq(v.w, w.w);
}

function assertEqX4(vec, arr, ...opts) {

    var assertFunc;
    if (opts.length == 1) {
        assertFunc = opts[0];
    } else {
        assertFunc = assertEq;
    }

    assertFunc(vec.x, arr[0]);
    assertFunc(vec.y, arr[1]);
    assertFunc(vec.z, arr[2]);
    assertFunc(vec.w, arr[3]);
}

function simdToArray(vec) {
    return [vec.x, vec.y, vec.z, vec.w];
}
