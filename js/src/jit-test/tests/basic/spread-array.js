load(libdir + "eqArrayHelper.js");

assertEqArray([...[1, 2, 3]], [1, 2, 3]);
assertEqArray([1, ...[2, 3, 4], 5], [1, 2, 3, 4, 5]);
assertEqArray([1, ...[], 2], [1, 2]);
assertEqArray([1, ...[2, 3], 4, ...[5, 6]], [1, 2, 3, 4, 5, 6]);
assertEqArray([1, ...[], 2], [1, 2]);
assertEqArray([1,, ...[2]], [1,, 2]);
assertEqArray([1,, ...[2],, 3,, 4,], [1,, 2,, 3,, 4,]);
assertEqArray([...[1, 2, 3],,,,], [1, 2, 3,,,,]);
assertEqArray([,,...[1, 2, 3],,,,], [,,1,2,3,,,,]);

// According to the draft spec, null and undefined are to be treated as empty
// arrays. However, they are not iterable. If the spec is not changed to be in
// terms of iterables, these tests should be fixed.
//assertEqArray([1, ...null, 2], [1, 2]);
//assertEqArray([1, ...undefined, 2], [1, 2]);
