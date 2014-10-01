/*
 * Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/
 */

var BUGNUMBER = 1075294;
var summary = "Object.seal() should return its argument with no conversion when the argument is a primitive value";

print(BUGNUMBER + ": " + summary);
assertEq(Object.seal(), undefined);
assertEq(Object.seal(undefined), undefined);
assertEq(Object.seal(null), null);
assertEq(Object.seal(1), 1);
assertEq(Object.seal("foo"), "foo");
assertEq(Object.seal(true), true);
if (typeof Symbol === "function") {
    assertEq(Object.seal(Symbol.for("foo")), Symbol.for("foo"));
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
