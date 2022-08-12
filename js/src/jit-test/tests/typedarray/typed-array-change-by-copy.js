// |jit-test| --enable-change-array-by-copy; skip-if: !getBuildConfiguration()['change-array-by-copy']

load(libdir + 'array-compare.js');
load(libdir + "asserts.js");

let typedArray123 = new Uint8Array([1, 2, 3]);
let typedArray12345 = new Uint8Array([1, 2, 3, 4, 5]);
let typedArray = new Uint8Array([1, 2, 3]);
let typedArray2 = new Uint8Array([3, 2, 1]);

let a_with = typedArray.with(1, 42);
assertEq(arraysEqual(typedArray, new Uint8Array([1, 2, 3])), true);
assertEq(arraysEqual(a_with, new Uint8Array([1, 42, 3])), true);

let tarray1 = new Uint8Array([42, 2, 3]);
let tarray2 = new Uint8Array([1, 2, 42]);

assertEq(arraysEqual(typedArray.with(-0, 42), tarray1), true);

/* null => 0 */
assertEq(arraysEqual(typedArray.with(null, 42), tarray1), true);
/* [] => 0 */
assertEq(arraysEqual(typedArray.with([], 42), tarray1), true);

assertEq(arraysEqual(typedArray.with("2", 42), tarray2), true);

/* Non-numeric indices => 0 */
assertEq(arraysEqual(typedArray.with("monkeys", 42), tarray1), true);
assertEq(arraysEqual(typedArray.with(undefined, 42), tarray1), true);
assertEq(arraysEqual(typedArray.with(function() {}, 42), tarray1), true);

assertThrowsInstanceOf(() => typedArray.with(3, 42), RangeError);
assertThrowsInstanceOf(() => typedArray.with(5, 42), RangeError);
assertThrowsInstanceOf(() => typedArray.with(-10, 42), RangeError);
assertThrowsInstanceOf(() => typedArray.with(Infinity, 42), RangeError);

let reversedIntArray = typedArray.toReversed();
assertEq(arraysEqual(typedArray, typedArray123), true);
assertEq(arraysEqual(reversedIntArray, typedArray2), true);

let sortedIntArray = typedArray2.toSorted();
assertEq(arraysEqual(typedArray2, new Uint8Array([3, 2, 1])), true);
assertEq(arraysEqual(sortedIntArray, typedArray), true);

let a_toSpliced1 = typedArray.toSpliced();
assertEq(arraysEqual(typedArray, typedArray123), true);
assertEq(arraysEqual(a_toSpliced1, typedArray123), true);

let a_toSpliced2 = typedArray.toSpliced(2);
assertEq(arraysEqual(typedArray, typedArray123), true);
assertEq(arraysEqual(a_toSpliced2, new Uint8Array([1, 2])), true);

let typedArray3 = new Uint8Array([1, 2, 3, 4, 5]);
let a_toSpliced3 = typedArray3.toSpliced(1, 2)
assertEq(arraysEqual(typedArray3, typedArray12345), true);
assertEq(arraysEqual(a_toSpliced3, new Uint8Array([1, 4, 5])), true);

let a_toSpliced4 = typedArray3.toSpliced(1, 2, 42, 12)
assertEq(arraysEqual(typedArray3, typedArray12345), true);
assertEq(arraysEqual(a_toSpliced4, new Uint8Array([1, 42, 12, 4, 5])), true);
