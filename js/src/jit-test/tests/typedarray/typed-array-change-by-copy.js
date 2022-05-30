// |jit-test| --enable-change-array-by-copy; skip-if: !getBuildConfiguration()['change-array-by-copy']

load(libdir + 'array-compare.js');
load(libdir + "asserts.js");

let typedArray123 = new Uint8Array([1, 2, 3]);
let typedArray12345 = new Uint8Array([1, 2, 3, 4, 5]);
let typedArray = new Uint8Array([1, 2, 3]);
let typedArray2 = new Uint8Array([3, 2, 1]);

let a_withAt = typedArray.withAt(1, 42);
assertEq(arraysEqual(typedArray, new Uint8Array([1, 2, 3])), true);
assertEq(arraysEqual(a_withAt, new Uint8Array([1, 42, 3])), true);

let tarray1 = new Uint8Array([42, 2, 3]);
let tarray2 = new Uint8Array([1, 2, 42]);

assertEq(arraysEqual(typedArray.withAt(-0, 42), tarray1), true);

/* null => 0 */
assertEq(arraysEqual(typedArray.withAt(null, 42), tarray1), true);
/* [] => 0 */
assertEq(arraysEqual(typedArray.withAt([], 42), tarray1), true);

assertEq(arraysEqual(typedArray.withAt("2", 42), tarray2), true);

/* Non-numeric indices => 0 */
assertEq(arraysEqual(typedArray.withAt("monkeys", 42), tarray1), true);
assertEq(arraysEqual(typedArray.withAt(undefined, 42), tarray1), true);
assertEq(arraysEqual(typedArray.withAt(function() {}, 42), tarray1), true);

assertThrowsInstanceOf(() => typedArray.withAt(3, 42), RangeError);
assertThrowsInstanceOf(() => typedArray.withAt(5, 42), RangeError);
assertThrowsInstanceOf(() => typedArray.withAt(-10, 42), RangeError);
assertThrowsInstanceOf(() => typedArray.withAt(Infinity, 42), RangeError);

let reversedIntArray = typedArray.withReversed();
assertEq(arraysEqual(typedArray, typedArray123), true);
assertEq(arraysEqual(reversedIntArray, typedArray2), true);


let sortedIntArray = typedArray2.withSorted();
assertEq(arraysEqual(typedArray2, new Uint8Array([3, 2, 1])), true);
assertEq(arraysEqual(sortedIntArray, typedArray), true);

let a_withSpliced1 = typedArray.withSpliced();
assertEq(arraysEqual(typedArray, typedArray123), true);
assertEq(arraysEqual(a_withSpliced1, typedArray123), true);

let a_withSpliced2 = typedArray.withSpliced(2);
assertEq(arraysEqual(typedArray, typedArray123), true);
assertEq(arraysEqual(a_withSpliced2, new Uint8Array([1, 2])), true);

let typedArray3 = new Uint8Array([1, 2, 3, 4, 5]);
let a_withSpliced3 = typedArray3.withSpliced(1, 2)
assertEq(arraysEqual(typedArray3, typedArray12345), true);
assertEq(arraysEqual(a_withSpliced3, new Uint8Array([1, 4, 5])), true);

let a_withSpliced4 = typedArray3.withSpliced(1, 2, 42, 12)
assertEq(arraysEqual(typedArray3, typedArray12345), true);
assertEq(arraysEqual(a_withSpliced4, new Uint8Array([1, 42, 12, 4, 5])), true);
