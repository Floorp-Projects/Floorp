/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var BUGNUMBER = 1060873;
var summary = "Object.isExtensible() should return false when given primitive values as input";

print(BUGNUMBER + ": " + summary);
assertEq(Object.isExtensible(), false);
assertEq(Object.isExtensible(undefined), false);
assertEq(Object.isExtensible(null), false);
assertEq(Object.isExtensible(1), false);
assertEq(Object.isExtensible("foo"), false);
assertEq(Object.isExtensible(true), false);
if (typeof Symbol === "function") {
    assertEq(Object.isExtensible(Symbol()), false);
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
