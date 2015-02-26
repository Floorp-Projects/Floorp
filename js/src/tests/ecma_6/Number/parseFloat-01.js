// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

//-----------------------------------------------------------------------------
var BUGNUMBER = 886949;
var summary = "ES6 (draft May 2013) 15.7.3.10 Number.parseFloat(string)";

print(BUGNUMBER + ": " + summary);

assertEq(Number.parseFloat("Infinity"), Infinity);
assertEq(Number.parseFloat("+Infinity"), Infinity);
assertEq(Number.parseFloat("-Infinity"), -Infinity);

assertEq(Number.parseFloat("inf"), NaN);
assertEq(Number.parseFloat("Inf"), NaN);
assertEq(Number.parseFloat("infinity"), NaN);

assertEq(Number.parseFloat("nan"), NaN);
assertEq(Number.parseFloat("NaN"), NaN);

/* Number.parseFloat should be the same function object as global parseFloat. */
assertEq(Number.parseFloat, parseFloat);

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("All tests passed!");
