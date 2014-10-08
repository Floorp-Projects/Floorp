/*
 * Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/
 */

var BUGNUMBER = 1079090;
var summary = "Coerce the argument passed to Object.getPrototypeOf using ToObject";
print(BUGNUMBER + ": " + summary);

assertThrowsInstanceOf(() => Object.getPrototypeOf(), TypeError);
assertThrowsInstanceOf(() => Object.getPrototypeOf(undefined), TypeError);
assertThrowsInstanceOf(() => Object.getPrototypeOf(null), TypeError);

assertEq(Object.getPrototypeOf(1), Number.prototype);
assertEq(Object.getPrototypeOf(true), Boolean.prototype);
assertEq(Object.getPrototypeOf("foo"), String.prototype);
if (typeof Symbol === "function") {
    assertEq(Object.getPrototypeOf(Symbol("foo")), Symbol.prototype);
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
