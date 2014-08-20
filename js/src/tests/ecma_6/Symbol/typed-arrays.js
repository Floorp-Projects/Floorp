/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/ */

if (typeof Symbol === "function") {
    // Symbol-to-number type conversions involving typed arrays.

    for (var T of [Uint8Array, Uint8ClampedArray, Int16Array, Float32Array]) {
        // Typed array constructors convert symbols using ToNumber(), which throws.
        assertThrowsInstanceOf(() => new T([Symbol("a")]), TypeError);

        // Assignment does the same.
        var arr = new T([1]);
        assertThrowsInstanceOf(() => { arr[0] = Symbol.iterator; }, TypeError);
        assertEq(arr[0], 1);
    }
}

if (typeof reportCompare === "function")
    reportCompare(0, 0);
