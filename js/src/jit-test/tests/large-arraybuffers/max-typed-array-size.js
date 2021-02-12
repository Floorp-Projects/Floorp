load(libdir + "asserts.js");

const maxByteLength = 8 * 1024 * 1024 * 1024;

// Test only the smallest and largest element type, because allocating a lot of
// large buffers can be pretty slow.
for (let ctor of [Int8Array, BigInt64Array]) {
    const maxLength = maxByteLength / ctor.BYTES_PER_ELEMENT;
    let ta1 = new ctor(maxLength);
    assertEq(ta1.length, maxLength);
    ta1[maxLength - 1]++;

    let ta2 = new ctor(ta1.buffer, 0, maxLength);
    assertEq(ta2.length, maxLength);
    assertEq(ta1[maxLength - 1], ta2[maxLength - 1]);

    assertThrowsInstanceOf(() => new ctor(maxLength + 1), RangeError);
    assertThrowsInstanceOf(() => new ctor(ta1.buffer, 0, maxLength + 1), RangeError);
}
