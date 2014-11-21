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

function simdToArray(v) {
    return [v.x, v.y, v.z, v.w];
}

const INT32_MAX = Math.pow(2, 31) - 1;
const INT32_MIN = -Math.pow(2, 31);
assertEq(INT32_MAX + 1 | 0, INT32_MIN);

function testBinaryFunc(v, w, simdFunc, func) {
    var varr = simdToArray(v);
    var warr = simdToArray(w);

    var observed = simdToArray(simdFunc(v, w));
    var expected = varr.map(function(v, i) { return func(varr[i], warr[i]); });

    for (var i = 0; i < observed.length; i++)
        assertEq(observed[i], expected[i]);
}
