/*
 * Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/
 */

var BUGNUMBER = 1073446;
var summary = "Object.preventExtensions() should return its argument with no conversion when the argument is a primitive value";

print(BUGNUMBER + ": " + summary);
assertEq(Object.preventExtensions(), undefined);
assertEq(Object.preventExtensions(undefined), undefined);
assertEq(Object.preventExtensions(null), null);
assertEq(Object.preventExtensions(1), 1);
assertEq(Object.preventExtensions("foo"), "foo");
assertEq(Object.preventExtensions(true), true);
if (typeof Symbol === "function") {
    assertEq(Object.preventExtensions(Symbol.for("foo")), Symbol.for("foo"));
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
