// |reftest| skip-if(!xulRuntime.shell)

// Test that all TypedArray constructor variants throw a RangeError when
// attempting to create a too large array.

// The maximum typed array length is limited to `(INT32_MAX/BYTES_PER_ELEMENT)`
// on all 32-bit systems; on 64-bit systems the limit is 8GB presently.

const INT32_MAX = 2**31 - 1;
const EIGHTGB = 8 * 1024 * 1024 * 1024;

function tooLarge(elementSize) {
    if (largeArrayBufferEnabled()) {
        return (EIGHTGB / elementSize) + 1;
    }
    return (INT32_MAX + 1) / elementSize;
}

// 22.2.4.2 TypedArray ( length )
for (let TA of typedArrayConstructors) {
    assertThrowsInstanceOf(() => new TA(tooLarge(1)), RangeError);
    assertThrowsInstanceOf(() => new TA(tooLarge(TA.BYTES_PER_ELEMENT)), RangeError);
}

// Test disabled because allocating a 2**30 Int8Array easily leads to OOMs.
//
// 22.2.4.3 TypedArray ( typedArray )
// const largeInt8Array = new Int8Array(2**30);
// for (let TA of typedArrayConstructors.filter(c => c.BYTES_PER_ELEMENT > 1)) {
//     assertThrowsInstanceOf(() => new TA(largeInt8Array), RangeError);
// }

// 22.2.4.4 TypedArray ( object )
for (let TA of typedArrayConstructors) {
    assertThrowsInstanceOf(() => new TA({length: tooLarge(1)}), RangeError);
    assertThrowsInstanceOf(() => new TA({length: tooLarge(TA.BYTES_PER_ELEMENT)}), RangeError);
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
