// Ensure that sorts finish even if a comparator adds items
// Note: the array is not expected to be properly sorted.
let outsideArray = new Int32Array([1, 99, 2]);
function addingComparator(x, y) {
    if (x == 99 || y == 99) {
        outsideArray[0] = 101;
        outsideArray[outsideArray.length - 1] = 102;
    }
    return x - y;
}
outsideArray.sort(addingComparator);
assertEq(outsideArray.every(x => [1, 2, 99, 101, 102].includes(x)), true);

// Ensure that sorts finish even if a comparator calls sort again
// Note: the array is not expected to be properly sorted.
outsideArray = new Int32Array([1, 99, 2]);
function recursiveComparator(x, y) {
    outsideArray.sort();
    return x - y;
}
outsideArray.sort(recursiveComparator);
assertEq(outsideArray.every(x => outsideArray.includes(x)), true)

// Ensure that NaN's returned from custom comparators behave as / are converted
// to +0s.
let nanComparatorData  = [2112, 42, 1111, 34];
let nanComparatorArray = new Int32Array(nanComparatorData);
nanComparatorArray.sort((x, y) => NaN);
assertEq(nanComparatorData.every(x => nanComparatorArray.includes(x)), true);

if (typeof reportCompare === "function")
    reportCompare(true, true);
