// |jit-test| --enable-change-array-by-copy; skip-if: !getBuildConfiguration()['change-array-by-copy']

load(libdir + 'array-compare.js');
load(libdir + "asserts.js");

typedArray123 = new Uint8Array([1, 2, 3]);
typedArray12345 = new Uint8Array([1, 2, 3, 4, 5]);
typedArray = new Uint8Array([1, 2, 3]);
typedArray2 = new Uint8Array([3, 2, 1]);

a_withAt = typedArray.withAt(1, 42);
assertEq(arraysEqual(typedArray, new Uint8Array([1, 2, 3])), true);
assertEq(arraysEqual(a_withAt, new Uint8Array([1, 42, 3])), true);

assertThrowsInstanceOf(() => typedArray.withAt(5, 42), RangeError);
assertThrowsInstanceOf(() => typedArray.withAt(-10, 42), RangeError);
assertThrowsInstanceOf(() => typedArray.withAt(-0, 42), RangeError);

reversedIntArray = typedArray.withReversed();
assertEq(arraysEqual(typedArray, typedArray123), true);
assertEq(arraysEqual(reversedIntArray, typedArray2), true);


sortedIntArray = typedArray2.withSorted();
assertEq(arraysEqual(typedArray2, new Uint8Array([3, 2, 1])), true);
assertEq(arraysEqual(sortedIntArray, typedArray), true);

a_withSpliced1 = typedArray.withSpliced();
assertEq(arraysEqual(typedArray, typedArray123), true);
assertEq(arraysEqual(a_withSpliced1, typedArray123), true);

a_withSpliced2 = typedArray.withSpliced(2);
assertEq(arraysEqual(typedArray, typedArray123), true);
assertEq(arraysEqual(a_withSpliced2, new Uint8Array([1, 2])), true);

typedArray2 = new Uint8Array([1, 2, 3, 4, 5]);
a_withSpliced3 = typedArray2.withSpliced(1, 2)
assertEq(arraysEqual(typedArray2, typedArray12345), true);
assertEq(arraysEqual(a_withSpliced3, new Uint8Array([1, 4, 5])), true);

a_withSpliced4 = typedArray2.withSpliced(1, 2, 42, 12)
assertEq(arraysEqual(typedArray2, typedArray12345), true);
    assertEq(arraysEqual(a_withSpliced4, new Uint8Array([1, 42, 12, 4, 5])), true);
