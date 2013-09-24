load(libdir + "eqArrayHelper.js");

function check(f) {
  assertEqArray(f.call(...[null], 1, 2, 3), [1, 2, 3]);
  assertEqArray(f.call(...[null], 1, ...[2, 3], 4, ...[5, 6]), [1, 2, 3, 4, 5, 6]);
  assertEqArray(f.call(...[null, 1], ...[2, 3], 4, ...[5, 6]), [1, 2, 3, 4, 5, 6]);
  assertEqArray(f.call(...[null, 1, ...[2, 3], 4, ...[5, 6]]), [1, 2, 3, 4, 5, 6]);
}

check(function(...x) x);
check((...x) => x);
