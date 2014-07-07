var tests = [
    {T: Uint8Array, result: 0},
    {T: Uint8ClampedArray, result: 0},
    {T: Int16Array, result: 0},
    {T: Float32Array, result: NaN}
];

var LENGTH = 1024, SYMBOL_INDEX = 999;

var big = [];
for (var i = 0; i < LENGTH; i++)
    big[i] = (i === SYMBOL_INDEX ? Symbol.for("comet") : i);

function copy(arr, big) {
    for (var i = 0; i < LENGTH; i++)
        arr[i] = big[i];
}

for (var {T, result} of tests) {
    // Typed array constructors convert symbols to NaN or 0.
    arr = new T(big);
    assertEq(arr[SYMBOL_INDEX], result);

    // Element assignment does the same.
    for (var k = 0; k < 3; k++) {
        copy(arr, big);
        assertEq(arr[SYMBOL_INDEX], result);
    }
}
