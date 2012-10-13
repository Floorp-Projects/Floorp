// onNewGlobalObject handlers receive the correct Debugger.Object instances.

var dbg = new Debugger;

var gw = null;
dbg.onNewGlobalObject = function (global) {
  assertEq(arguments.length, 1);
  assertEq(this, dbg);
  gw = global;
};
var g = newGlobal();
assertEq(typeof gw, 'object');
assertEq(dbg.addDebuggee(g), gw);

// The Debugger.Objects passed to onNewGlobalObject are the global as
// viewed from its own compartment.
assertEq(gw.makeDebuggeeValue(g), gw);
