/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/ */

// Symbol-to-number type conversions involving typed arrays.

var tests = [
    {T: Uint8Array, result: 0},
    {T: Uint8ClampedArray, result: 0},
    {T: Int16Array, result: 0},
    {T: Float32Array, result: NaN}
];

for (var {T, result} of tests) {
    // Typed array constructors convert symbols to NaN or 0.
    var arr = new T([Symbol("a")]);
    assertEq(arr.length, 1);
    assertEq(arr[0], result);

    // Assignment also converts symbols to NaN or 0.
    arr[0] = 0;
    assertEq(arr[0] = Symbol.iterator, Symbol.iterator);
    assertEq(arr[0], result);
}

if (typeof reportCompare === "function")
    reportCompare(0, 0);
