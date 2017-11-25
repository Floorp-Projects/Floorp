// Ensure that when sorting TypedArrays we don't
// ignore byte offsets (bug 1290579).

var sortFunctions = [Int32Array.prototype.sort];

// Also test with cross-compartment wrapped typed arrays.
if (typeof newGlobal === "function") {
    var otherGlobal = newGlobal();
    sortFunctions.push(newGlobal().Int32Array.prototype.sort);
}

// The bug manifests itself only with Float arrays,
// but checking everything here just for sanity.

for (var ctor of anyTypedArrayConstructors) {
    var ab = new ArrayBuffer(1025 * ctor.BYTES_PER_ELEMENT);
    var ta = new ctor(ab, ctor.BYTES_PER_ELEMENT, 1024);

    // |testArray[0]| shouldn't be modified when sort() is called below.
    var testArray = new ctor(ab, 0, 1);
    testArray[0] = 1;

    for (var sortFn of sortFunctions) {
        sortFn.call(ta);
        assertEq(testArray[0], 1);
    }
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
