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
    let arrOne    = genRandomArray(size, seed);
    let arrTwo    = Array.from(arrOne);
    let arrThree  = Array.from(arrOne);

    // Test numeric comparators against typed array sort.
    assertDeepEq(Array.from((Int32Array.from(arrOne)).sort()),
                 arrTwo.sort((x, y) => (x - y)),
                 `The arr is not properly sorted! seed: ${SEED}`);

    // Use multiplication to kill comparator optimization and trigger
    // self-hosted sorting.
    assertDeepEq(Array.from((Int32Array.from(arrOne)).sort()),
                 arrThree.sort((x, y) => (1*x - 1*y)),
                 `The arr is not properly sorted! seed: ${SEED}`);
}

SortTest(2048, SEED);
SortTest(16, SEED);
SortTest(0, SEED);

if (typeof reportCompare === "function")
    reportCompare(true, true);
