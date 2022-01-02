/*
 * Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/
 */

assertEq(Object.hasOwn({}, "any"), false);
assertThrowsInstanceOf(() => Object.hasOwn(null, "any"), TypeError);

var x = { test: 'test value'}
var y = {}
var z = Object.create(x);

assertEq(Object.hasOwn(x, "test"), true);
assertEq(Object.hasOwn(y, "test"), false);
assertEq(Object.hasOwn(z, "test"), false);

if (typeof reportCompare === "function")
    reportCompare(true, true);
