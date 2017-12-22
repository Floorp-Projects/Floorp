// %TypedArray%.prototype.sort throws if the comparator is neither undefined nor
// a callable object.

// Use a zero length typed array, so we can provide any kind of callable object
// without worrying that the function is actually a valid comparator function.
const typedArray = new Int32Array(0);

// Throws if the comparator is neither undefined nor callable.
for (let invalidComparator of [null, 0, true, Symbol(), {}, []]) {
    assertThrowsInstanceOf(() => typedArray.sort(invalidComparator), TypeError);
}

// Doesn't throw if the comparator is undefined or a callable object.
for (let validComparator of [undefined, () => {}, Math.max, class {}, new Proxy(function(){}, {})]) {
    typedArray.sort(validComparator);
}

// Also doesn't throw if no comparator was provided at all.
typedArray.sort();

if (typeof reportCompare === "function")
    reportCompare(0, 0);
