function swapElements(arr, i, j) {
    var swap = arr[i];
    arr[i] = arr[j];
    arr[j] = swap;
}

// Yield every permutation of the elements in some iterable.
function *permutations(items) {
    if (items.length == 0) {
        yield [];
    } else {
        for (let i = 0; i < items.length; i++) {
            swapElements(items, 0, i);
            for (let e of permutations(items.slice(1, items.length)))
                yield [items[0]].concat(e);
        }
    }
}

// Pre-sorted test data, it's important that these arrays remain in ascending order.
let i32  = [-2147483648, -320000, -244000, 2147483647]
let u32  = [0, 987632, 4294967295]
let i16  = [-32768, -999, 1942, 32767]
let u16  = [0, 65535, 65535]
let i8   = [-128, 127]
let u8   = [255]

// Test the behavior in the default comparator as described in 22.2.3.26.
// The spec boils down to, -0s come before +0s, and NaNs always come last.
// Float Arrays are used because all other types convert -0 and NaN to +0.
let f32  = [-2147483647, -2147483646.99, -0, 0, 2147483646.99, NaN]
let f64  = [-2147483646.99, -0, 0, 4147483646.99, NaN]
let nans = [1/undefined, NaN, Number.NaN]

// Sort every possible permutation of an arrays
function sortAllPermutations(dataType, testData) {
    let reference = new dataType(testData);
    for (let permutation of permutations(testData))
        assertDeepEq((new dataType(permutation)).sort(), reference);
}

sortAllPermutations(Int32Array,   i32);
sortAllPermutations(Uint32Array,  u32);
sortAllPermutations(Int16Array,   i16);
sortAllPermutations(Uint16Array,  u16);
sortAllPermutations(Int8Array,    i8);
sortAllPermutations(Uint8Array,   u8);
sortAllPermutations(Float32Array, f32);
sortAllPermutations(Float64Array, f64);
sortAllPermutations(Float32Array, nans);
sortAllPermutations(Float64Array, nans);

if (typeof reportCompare === "function")
    reportCompare(true, true);
