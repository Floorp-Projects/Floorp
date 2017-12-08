// Test with all floating point typed arrays.
const floatConstructors = anyTypedArrayConstructors.filter(isFloatConstructor);

// Also test with cross-compartment wrapped typed arrays.
if (typeof newGlobal === "function") {
    const otherGlobal = newGlobal();
    floatConstructors.push(otherGlobal.Float32Array);
    floatConstructors.push(otherGlobal.Float64Array);
}

function* prod(xs, ys) {
    for (let x of xs) {
        for (let y of ys) {
            yield [x, y];
        }
    }
}

const isLittleEndian = new Uint8Array(new Uint16Array([1]).buffer)[0] !== 0;

function seti32(i32, i, v) {
    i32[i] = v;
}

function seti64(i32, i, [hi, lo]) {
    i32[i * 2 + isLittleEndian] = hi;
    i32[i * 2 + !isLittleEndian] = lo;
}

const setInt = {
    Float32: seti32,
    Float64: seti64,
};

const NaNs = {
    Float32: [
        0x7F800001|0, // smallest SNaN
        0x7FBFFFFF|0, // largest SNaN
        0x7FC00000|0, // smallest QNaN
        0x7FFFFFFF|0, // largest QNaN
        0xFF800001|0, // smallest SNaN, sign-bit set
        0xFFBFFFFF|0, // largest SNaN, sign-bit set
        0xFFC00000|0, // smallest QNaN, sign-bit set
        0xFFFFFFFF|0, // largest QNaN, sign-bit set
    ],
    Float64: [
        [0x7FF00000|0, 0x00000001|0], // smallest SNaN
        [0x7FF7FFFF|0, 0xFFFFFFFF|0], // largest SNaN
        [0x7FF80000|0, 0x00000000|0], // smallest QNaN
        [0x7FFFFFFF|0, 0xFFFFFFFF|0], // largest QNaN
        [0xFFF00000|0, 0x00000001|0], // smallest SNaN, sign-bit set
        [0xFFF7FFFF|0, 0xFFFFFFFF|0], // largest SNaN, sign-bit set
        [0xFFF80000|0, 0x00000000|0], // smallest QNaN, sign-bit set
        [0xFFFFFFFF|0, 0xFFFFFFFF|0], // largest QNaN, sign-bit set
    ],
};

// %TypedArray%.prototype.sort
const TypedArraySort = Int32Array.prototype.sort;

// Test with small and large typed arrays.
const typedArrayLengths = [16, 4096];

for (const [TA, taLength] of prod(floatConstructors, typedArrayLengths)) {
    let type = TA.name.slice(0, -"Array".length);
    let nansLength = NaNs[type].length;
    let fta = new TA(taLength);
    let i32 = new Int32Array(fta.buffer);

    // Add NaNs in various representations at the start of the typed array.
    for (let i = 0; i < nansLength; ++i) {
        setInt[type](i32, i, NaNs[type][i]);
    }

    // Also add two non-NaN values for testing.
    fta[nansLength] = 123;
    fta[nansLength + 1] = -456;

    // Sort the array and validate sort() sorted all elements correctly.
    TypedArraySort.call(fta);

    // |-456| should be sorted to the start.
    assertEq(fta[0], -456);

    // Followed by a bunch of zeros,
    const zeroOffset = 1;
    const zeroCount = taLength - nansLength - 2;
    for (let i = 0; i < zeroCount; ++i) {
        assertEq(fta[zeroOffset + i], 0, `At offset: ${zeroOffset + i}`);
    }

    // and then |123|.
    assertEq(fta[zeroOffset + zeroCount], 123);

    // And finally the NaNs.
    const nanOffset = zeroCount + 2;
    for (let i = 0; i < nansLength; ++i) {
        // We don't assert a specific NaN value is present, because this is
        // not required by the spec and we don't provide any guarantees NaN
        // values are either unchanged or canonicalized in sort().
        assertEq(fta[nanOffset + i], NaN, `At offset: ${nanOffset + i}`);
    }
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
