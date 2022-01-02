// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

//-----------------------------------------------------------------------------
var BUGNUMBER = 1003764;
var summary = "ES6 (draft Draft May 22, 2014) ES6 20.1.2.5 Number.isSafeInteger(number)";

print(BUGNUMBER + ": " + summary);

assertEq(Number.isSafeInteger.length, 1);

assertEq(Number.isSafeInteger({}), false);
assertEq(Number.isSafeInteger(NaN), false);
assertEq(Number.isSafeInteger(+Infinity), false);
assertEq(Number.isSafeInteger(-Infinity), false);

assertEq(Number.isSafeInteger(-1), true);
assertEq(Number.isSafeInteger(+0), true);
assertEq(Number.isSafeInteger(-0), true);
assertEq(Number.isSafeInteger(1), true);

assertEq(Number.isSafeInteger(3.2), false);

assertEq(Number.isSafeInteger(Math.pow(2, 53) - 2), true);
assertEq(Number.isSafeInteger(Math.pow(2, 53) - 1), true);
assertEq(Number.isSafeInteger(Math.pow(2, 53)), false);
assertEq(Number.isSafeInteger(Math.pow(2, 53) + 1), false);
assertEq(Number.isSafeInteger(Math.pow(2, 53) + 2), false);

assertEq(Number.isSafeInteger(-Math.pow(2, 53) - 2), false);
assertEq(Number.isSafeInteger(-Math.pow(2, 53) - 1), false);
assertEq(Number.isSafeInteger(-Math.pow(2, 53)), false);
assertEq(Number.isSafeInteger(-Math.pow(2, 53) + 1), true);
assertEq(Number.isSafeInteger(-Math.pow(2, 53) + 2), true);


if (typeof reportCompare === "function")
  reportCompare(true, true);

print("All tests passed!");
