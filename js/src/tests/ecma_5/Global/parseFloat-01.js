// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

//-----------------------------------------------------------------------------
var BUGNUMBER = 613492;
var summary = "ES5 15.1.2.3 parseFloat(string)";

print(BUGNUMBER + ": " + summary);

assertEq(parseFloat("Infinity"), Infinity);
assertEq(parseFloat("+Infinity"), Infinity);
assertEq(parseFloat("-Infinity"), -Infinity);

assertEq(parseFloat("inf"), NaN);
assertEq(parseFloat("Inf"), NaN);
assertEq(parseFloat("infinity"), NaN);

assertEq(parseFloat("nan"), NaN);
assertEq(parseFloat("NaN"), NaN);

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("All tests passed!");
