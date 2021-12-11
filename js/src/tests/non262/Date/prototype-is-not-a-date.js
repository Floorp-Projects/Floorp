/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var BUGNUMBER = 861219;
var summary = "Date.prototype isn't an instance of Date";

print(BUGNUMBER + ": " + summary);

assertEq(Date.prototype instanceof Date, false);
assertEq(Date.prototype.__proto__, Object.prototype);

if (typeof reportCompare === "function")
  reportCompare(true, true);
