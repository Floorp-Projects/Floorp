// Test that all TypedArray constructor variants throw a RangeError when
// attempting to create a too large array.

// The maximum typed array length is (currently) limited to
// `(INT32_MAX / BYTES_PER_ELEMENT) - 1`.

const INT32_MAX = 2**31 - 1;

// 22.2.4.2 TypedArray ( length )
for (let TA of typedArrayConstructors) {
    assertThrows(() => new TA(INT32_MAX), RangeError);
    assertThrows(() => new TA(INT32_MAX >> Math.log2(TA.BYTES_PER_ELEMENT)), RangeError);
}

// Test disabled because allocating a 2**30 Int8Array easily leads to OOMs.
//
// 22.2.4.3 TypedArray ( typedArray )
// const largeInt8Array = new Int8Array(2**30);
// for (let TA of typedArrayConstructors.filter(c => c.BYTES_PER_ELEMENT > 1)) {
//     assertThrows(() => new TA(largeInt8Array), RangeError);
// }

// 22.2.4.4 TypedArray ( object )
for (let TA of typedArrayConstructors) {
    assertThrows(() => new TA({length: INT32_MAX}), RangeError);
    assertThrows(() => new TA({length: INT32_MAX >> Math.log2(TA.BYTES_PER_ELEMENT)}), RangeError);
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
