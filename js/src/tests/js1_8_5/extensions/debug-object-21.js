// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

// dumb basics of uncaughtExceptionHook

var desc = Object.getOwnPropertyDescriptor(Debug.prototype, "uncaughtExceptionHook");
assertEq(typeof desc.get, 'function');
assertEq(typeof desc.set, 'function');

assertThrows(function () { Debug.prototype.uncaughtExceptionHook = null; }, TypeError);

var g = newGlobal('new-compartment');
var dbg = new Debug(g);
assertEq(desc.get.call(dbg), null);
assertThrows(function () { dbg.uncaughtExceptionHook = []; }, TypeError);
assertThrows(function () { dbg.uncaughtExceptionHook = 3; }, TypeError);
dbg.uncaughtExceptionHook = Math.sin;
assertEq(dbg.uncaughtExceptionHook, Math.sin);
dbg.uncaughtExceptionHook = null;
assertEq(dbg.uncaughtExceptionHook, null);

reportCompare(0, 0, 'ok');
