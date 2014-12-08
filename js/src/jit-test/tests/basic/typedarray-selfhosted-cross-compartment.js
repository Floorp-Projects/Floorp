/*
 * Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/
 */

var global = newGlobal();
var array = new global.Int8Array(10);

assertEq(array.find(v => v == 1), undefined)
assertEq(array.findIndex(v => v == 0), 0)

if (typeof reportCompare === "function")
    reportCompare(true, true);
