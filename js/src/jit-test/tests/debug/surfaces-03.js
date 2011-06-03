// dumb basics of uncaughtExceptionHook

load(libdir + 'asserts.js');

var desc = Object.getOwnPropertyDescriptor(Debug.prototype, "uncaughtExceptionHook");
assertEq(typeof desc.get, 'function');
assertEq(typeof desc.set, 'function');

assertThrowsInstanceOf(function () { Debug.prototype.uncaughtExceptionHook = null; }, TypeError);

var g = newGlobal('new-compartment');
var dbg = new Debug(g);
assertEq(desc.get.call(dbg), null);
assertThrowsInstanceOf(function () { dbg.uncaughtExceptionHook = []; }, TypeError);
assertThrowsInstanceOf(function () { dbg.uncaughtExceptionHook = 3; }, TypeError);
dbg.uncaughtExceptionHook = Math.sin;
assertEq(dbg.uncaughtExceptionHook, Math.sin);
dbg.uncaughtExceptionHook = null;
assertEq(dbg.uncaughtExceptionHook, null);
