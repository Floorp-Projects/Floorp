function binaryX4(op, v, w) {
    var arr = [];
    var [varr, warr] = [simdToArray(v), simdToArray(w)];
    [varr, warr] = [varr.map(Math.fround), warr.map(Math.fround)];
    for (var i = 0; i < 4; i++)
        arr[i] = op(varr[i], warr[i]);
    return arr.map(Math.fround);
}

function assertEqX4(vec, arr) {
    assertEq(vec.x, arr[0]);
    assertEq(vec.y, arr[1]);
    assertEq(vec.z, arr[2]);
    assertEq(vec.w, arr[3]);
}

function simdToArray(vec) {
    return [vec.x, vec.y, vec.z, vec.w];
}
