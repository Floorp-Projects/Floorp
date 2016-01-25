// Sort every possible permutation of some arrays.
function sortAllPermutations(data, comparefn) {
    for (let permutation of Permutations(Array.from(data))) {
        let sorted = (Array.from(permutation)).sort(comparefn);
        for (let i in sorted) {
            assertEq(sorted[i], data[i],
            [`[${permutation}].sort(${comparefn})`,
            `returned ${sorted}, expected ${data}`].join(' '));
        }
    }
}

let lex  = [2112, "bob", "is", "my", "name"];
let nans = [1/undefined, NaN, Number.NaN]

let num1  = [-11, 0, 0, 100, 101];
let num2  = [-11, 100, 201234.23, undefined, undefined];

sortAllPermutations(lex);
sortAllPermutations(nans);

sortAllPermutations(nans, (x, y) => x - y);
// Multiplication kills comparator optimization.
sortAllPermutations(nans, (x, y) => (1*x - 1*y));

sortAllPermutations(num1, (x, y) => x - y);
sortAllPermutations(num1, (x, y) => (1*x - 1*y));

sortAllPermutations(num2, (x, y) => x - y);
sortAllPermutations(num2, (x, y) => (1*x - 1*y));

if (typeof reportCompare === "function")
    reportCompare(true, true);
