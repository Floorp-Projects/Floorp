load(libdir + "asserts.js");

var LENGTH = 1024, SYMBOL_INDEX = 999;

if (typeof Symbol === "function") {
    var big = [];
    for (var i = 0; i < LENGTH; i++)
        big[i] = (i === SYMBOL_INDEX ? Symbol.for("comet") : i);

    var progress;
    function copy(arr, big) {
        for (var i = 0; i < LENGTH; i++) {
            arr[i] = big[i];
            progress = i;
        }
    }

    for (var T of [Uint8Array, Uint8ClampedArray, Int16Array, Float32Array]) {
        // Typed array constructors convert symbols using ToNumber, which throws.
        assertThrowsInstanceOf(() => new T(big), TypeError);

        // Element assignment does the same.
        var arr = new T(big.length);
        for (var k = 0; k < 3; k++) {
            progress = -1;
            assertThrowsInstanceOf(() => copy(arr, big), TypeError);
            assertEq(progress, SYMBOL_INDEX - 1);
            assertEq(arr[SYMBOL_INDEX], 0);
        }
    }
}
