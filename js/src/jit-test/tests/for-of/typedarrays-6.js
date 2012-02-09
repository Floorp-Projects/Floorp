// for-of can iterate over float typed arrays containing infinities or NaNs.

var values = [Infinity, -Infinity, -0, NaN];
for (var C of [Float32Array, Float64Array]) {
    var i = 0;
    for (var v of C(values))
        assertEq(v, values[i++]);
    assertEq(i, values.length);
}
