// dumb basics of uncaughtExceptionHook

load(libdir + 'asserts.js');

var desc = Object.getOwnPropertyDescriptor(Debugger.prototype, "uncaughtExceptionHook");
assertEq(typeof desc.get, 'function');
assertEq(typeof desc.set, 'function');

assertThrowsInstanceOf(function () { Debugger.prototype.uncaughtExceptionHook = null; }, TypeError);

var g = newGlobal('new-compartment');
var dbg = new Debugger(g);
assertEq(desc.get.call(dbg), null);
assertThrowsInstanceOf(function () { dbg.uncaughtExceptionHook = []; }, TypeError);
assertThrowsInstanceOf(function () { dbg.uncaughtExceptionHook = 3; }, TypeError);
dbg.uncaughtExceptionHook = Math.sin;
assertEq(dbg.uncaughtExceptionHook, Math.sin);
dbg.uncaughtExceptionHook = null;
assertEq(dbg.uncaughtExceptionHook, null);
