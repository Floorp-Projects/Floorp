/*
 * Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/
 */

var BUGNUMBER = 1079188;
var summary = "Coerce the argument passed to Object.getOwnPropertyDescriptor using ToObject";
print(BUGNUMBER + ": " + summary);

assertThrowsInstanceOf(() => Object.getOwnPropertyDescriptor(), TypeError);
assertThrowsInstanceOf(() => Object.getOwnPropertyDescriptor(undefined), TypeError);
assertThrowsInstanceOf(() => Object.getOwnPropertyDescriptor(null), TypeError);

Object.getOwnPropertyDescriptor(1);
Object.getOwnPropertyDescriptor(true);
if (typeof Symbol === "function") {
    Object.getOwnPropertyDescriptor(Symbol("foo"));
}

assertDeepEq(Object.getOwnPropertyDescriptor("foo", "length"), {
    configurable: false,
    enumerable: false,
    value: 3,
    writable: false
});

assertDeepEq(Object.getOwnPropertyDescriptor("foo", 0), {
    configurable: false,
    enumerable: true,
    value: "f",
    writable: false
});

if (typeof reportCompare === "function")
    reportCompare(true, true);
