/*
 * Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/
 */

var BUGNUMBER = 1071464;
var summary = "Object.isFrozen() should return true when given primitive values as input";

print(BUGNUMBER + ": " + summary);
assertEq(Object.isFrozen(), true);
assertEq(Object.isFrozen(undefined), true);
assertEq(Object.isFrozen(null), true);
assertEq(Object.isFrozen(1), true);
assertEq(Object.isFrozen("foo"), true);
assertEq(Object.isFrozen(true), true);
if (typeof Symbol === "function") {
    assertEq(Object.isFrozen(Symbol()), true);
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
