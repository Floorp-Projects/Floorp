function sortTypedArray(comparator) {
    // Create a typed array with three elements, but also add an own "length"
    // property with the value `2` to restrict the range of elements which
    // will be sorted by Array.prototype.sort().
    var ta = new Int8Array([3, 2, 1]);
    Object.defineProperty(ta, "length", {value: 2});

    // Sort with Array.prototype.sort(), not %TypedArray%.prototype.sort()!
    Array.prototype.sort.call(ta, comparator);

    return ta;
}

// Comparators using the form |return a - b| are special-cased in
// Array.prototype.sort().
function optimizedComparator(a, b) {
    return a - b;
}

// This comparator doesn't compile to the byte code sequence which gets
// special-cased in Array.prototype.sort().
function nonOptimizedComparator(a, b) {
    var d = a - b;
    return d;
}

// Both comparators should produce the same result.
assertEq(sortTypedArray(optimizedComparator).toString(), "2,3,1");
assertEq(sortTypedArray(nonOptimizedComparator).toString(), "2,3,1");


if (typeof reportCompare === 'function')
    reportCompare(0, 0);
