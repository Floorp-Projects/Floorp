load(libdir + "eqArrayHelper.js");

assertEqArray([...wrap([1])], [1]);
assertEqArray([1,, ...wrap([2, 3, 4]), 5, ...wrap([6])], [1,, 2, 3, 4, 5, 6]);
