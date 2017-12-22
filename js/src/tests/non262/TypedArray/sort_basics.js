// Note: failed runs should include their "SEED" value in error messages,
// setting "const SEED" to that value will recreate the data from any such run.
const SEED = (Math.random() * 10) + 1;

// Fill up an array buffer with random values and return it in raw form.
// 'size' is the desired length of the view we will place atop the buffer,
// 'width' is the bit-width of the view we plan on placing atop the buffer,
// and 'seed' is an initial value supplied to a pseudo-random number generator.
function genRandomArrayBuffer(size, width, seed) {
    let buf = new ArrayBuffer((width / 8) * size);
    let arr = new Uint8Array(buf);
    let len = 0;
    // We generate a random number, n, where 0 <= n <= 255 for every space
    // available in our buffer.
    for (let n of XorShiftGenerator(seed, buf.byteLength))
        arr[len++] = n;
    return buf;
}

// Because we can generate any possible combination of bits, some floating point
// entries will take on -Infinity, Infinity, and NaN values. This function ensures
// that a is <= b, where, like the default comparator,  -Infinity < Infinity and
// every non-NaN < NaN.
function lte(a, b) {
    if (isNaN(b))
      return true;
    return a <= b;
}

// A a >= b counterpart to the helper function above.
function gte(a, b) {
    return lte(b, a);
}

// A custom comparator.
function cmp(a, b) {
    return lte(a, b) ? gte(a, b) ? 0 : -1 : 1;
}

function SortTest(dataType, dataSource) {
    let typedArray = new dataType(dataSource);
    let originalValues = Array.from(typedArray);

    // Test the default comparator
    typedArray.sort();

    // Test against regular array sort
    assertEqArray(Array.from(typedArray), Array.from(originalValues).sort(cmp),
                  `The array is not properly sorted! seed: ${SEED}`);

    // Another sanity check
    for (let i=0; i < typedArray.length - 1; i++)
        assertEq(lte(typedArray[i], typedArray[i + 1]), true,
                 `The array is not properly sorted! ${typedArray[i]} > ${typedArray[i + 1]}, seed: ${SEED}`)

    // Test custom comparators
    typedArray.sort((x, y) => cmp(y, x));

    // The array should be in reverse order
    for (let i=typedArray.length - 2; i >= 0; i--)
        assertEq(gte(typedArray[i], typedArray[i + 1]), true,
                 `The array is not properly sorted! ${typedArray[i]} < ${typedArray[i + 1]}, seed: ${SEED}`)
}

for (let constructor of anyTypedArrayConstructors) {
    for (let arrayLength of [512, 256, 16, 0]) {
        let source = genRandomArrayBuffer(arrayLength, constructor.BYTES_PER_ELEMENT * 8, SEED);
        SortTest(constructor, source);
    }
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
