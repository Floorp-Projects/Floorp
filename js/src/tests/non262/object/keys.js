/*
 * Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/
 */

var BUGNUMBER = 1038545;
var summary = "Coerce the argument passed to Object.keys using ToObject";
print(BUGNUMBER + ": " + summary);

assertThrowsInstanceOf(() => Object.keys(), TypeError);
assertThrowsInstanceOf(() => Object.keys(undefined), TypeError);
assertThrowsInstanceOf(() => Object.keys(null), TypeError);

assertDeepEq(Object.keys(1), []);
assertDeepEq(Object.keys(true), []);
if (typeof Symbol === "function") {
    assertDeepEq(Object.keys(Symbol("foo")), []);
}

assertDeepEq(Object.keys("foo"), ["0", "1", "2"]);

if (typeof reportCompare === "function")
    reportCompare(true, true);
