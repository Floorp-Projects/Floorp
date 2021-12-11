// Debugger.prototype.makeGlobalObjectReference only accepts actual global objects.

load(libdir + 'asserts.js');

var dbg = new Debugger;

assertThrowsInstanceOf(() => dbg.makeGlobalObjectReference(true), TypeError);
assertThrowsInstanceOf(() => dbg.makeGlobalObjectReference("foo"), TypeError);
assertThrowsInstanceOf(() => dbg.makeGlobalObjectReference(12), TypeError);
assertThrowsInstanceOf(() => dbg.makeGlobalObjectReference(undefined), TypeError);
assertThrowsInstanceOf(() => dbg.makeGlobalObjectReference(null), TypeError);
assertThrowsInstanceOf(() => dbg.makeGlobalObjectReference({ xlerb: "sbot" }), TypeError);
assertEq(dbg.makeGlobalObjectReference(this) instanceof Debugger.Object, true);
