// Note: failed runs should include their "SEED" value in error messages,
// setting "const SEED" to that value will recreate the data from any such run.
const SEED = (Math.random() * 10) + 1;

// Create an array filled with random values, 'size' is the desired length of
// the array and 'seed' is an initial value supplied to a pseudo-random number
// generator.
function genRandomArray(size, seed) {
    return Array.from(XorShiftGenerator(seed, size));
}

function SortTest(size, seed) {
    let arrOne       = genRandomArray(size, seed);
    let arrTwo       = Array.from(arrOne);
    let arrThree     = Array.from(arrOne);
    let typedArrays  = [
        new Uint8Array(arrOne),
        new Int32Array(arrOne),
        new Float32Array(arrOne)
    ];

    let sorted = Array.from((Int32Array.from(arrOne)).sort());

    // Test numeric comparators against typed array sort.
    assertDeepEq(sorted, arrTwo.sort((x, y) => (x - y)),
                 `The array is not properly sorted! seed: ${SEED}`);

    // Use multiplication to kill comparator optimization and trigger
    // self-hosted sorting.
    assertDeepEq(sorted, arrThree.sort((x, y) => (1*x - 1*y)),
                 `The array is not properly sorted! seed: ${SEED}`);

    // Ensure that typed arrays are also sorted property.
    for (typedArr of typedArrays) {
        let sortedTypedArray = Array.prototype.sort.call(typedArr, (x, y) => (1*x - 1*y))
        assertDeepEq(sorted, Array.from(sortedTypedArray),
                     `The array is not properly sorted! seed: ${SEED}`);
    }
}

SortTest(2048, SEED);
SortTest(16, SEED);
SortTest(0, SEED);

if (typeof reportCompare === "function")
    reportCompare(true, true);
