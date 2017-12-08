// Test ToObject in %TypedArray%.prototype.set(array|typedArray, offset).

let ta = new Int32Array(4);

for (let nullOrUndefined of [null, undefined]) {
    // ToObject(array) throws a TypeError when |array| is null or undefined.
    assertThrowsInstanceOf(() => ta.set(nullOrUndefined), TypeError);

    // ToInteger(offset) is called before ToObject(array).
    class ExpectedError extends Error {}
    assertThrowsInstanceOf(() => ta.set(nullOrUndefined, {
        valueOf() {
            throw new ExpectedError();
        }
    }), ExpectedError);
}

// Ensure ta is still initialized with zeros.
assertEqArray(ta, [0, 0, 0, 0]);

// %TypedArray%.prototype.set can be called with a string primitive values.
ta.set("");
assertEqArray(ta, [0, 0, 0, 0]);

ta.set("123");
assertEqArray(ta, [1, 2, 3, 0]);

// Throws a RangeError if the length is too large.
assertThrowsInstanceOf(() => ta.set("456789"), RangeError);
assertEqArray(ta, [1, 2, 3, 0]);

// When called with other primitive values the typed array contents don't
// change since ToObject(<primitive>).length is zero, i.e. the source object is
// treated the same as an empty array.
for (let value of [true, false, 0, NaN, 123, Infinity, Symbol()]) {
    ta.set(value);
    assertEqArray(ta, [1, 2, 3, 0]);
}

// Repeat test from above when the primitive wrapper prototype has been changed
// to include "length" and an indexed property.
Number.prototype.length = 4;
Number.prototype[3] = -1;
try {
    ta.set(456);
    assertEqArray(ta, [0, 0, 0, -1]);
} finally {
    delete Number.prototype.length;
    delete Number.prototype[3];
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
