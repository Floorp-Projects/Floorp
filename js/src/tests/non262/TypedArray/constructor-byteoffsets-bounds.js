// 22.2.4.5 TypedArray ( buffer [ , byteOffset [ , length ] ] )

// Test bound checks around for |byteOffset| and |length| arguments.

const buffer = new ArrayBuffer(0);

for (let TA of typedArrayConstructors) {
    // Test bound checks around INT32_MAX for |byteOffset| argument.
    assertThrows(() => new TA(ab, 2**31 - TA.BYTES_PER_ELEMENT), RangeError);
    assertThrows(() => new TA(ab, 2**31 - 1), RangeError);
    assertThrows(() => new TA(ab, 2**31), RangeError);
    assertThrows(() => new TA(ab, 2**31 + 1), RangeError);
    assertThrows(() => new TA(ab, 2**31 + TA.BYTES_PER_ELEMENT), RangeError);

    // Test bound checks around UINT32_MAX for |byteOffset| argument.
    assertThrows(() => new TA(ab, 2**32 - TA.BYTES_PER_ELEMENT), RangeError);
    assertThrows(() => new TA(ab, 2**32 - 1), RangeError);
    assertThrows(() => new TA(ab, 2**32), RangeError);
    assertThrows(() => new TA(ab, 2**32 + 1), RangeError);
    assertThrows(() => new TA(ab, 2**32 + TA.BYTES_PER_ELEMENT), RangeError);

    // Test bound checks around INT32_MAX for |length| argument.
    assertThrows(() => new TA(ab, 0, 2**31 - 1), RangeError);
    assertThrows(() => new TA(ab, 0, 2**31), RangeError);
    assertThrows(() => new TA(ab, 0, 2**31 + 1), RangeError);

    // Test bound checks around UINT32_MAX for |length| argument.
    assertThrows(() => new TA(ab, 0, 2**32 - 1), RangeError);
    assertThrows(() => new TA(ab, 0, 2**32), RangeError);
    assertThrows(() => new TA(ab, 0, 2**32 + 1), RangeError);
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
