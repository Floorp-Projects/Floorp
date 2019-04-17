// onNewGlobalObject handlers receive the correct Debugger.Object instances.

var dbg = new Debugger;

var gw = null;
dbg.onNewGlobalObject = function (global) {
  assertEq(arguments.length, 1);
  assertEq(this, dbg);
  gw = global;
};
var g = newGlobal({newCompartment: true});
assertEq(typeof gw, 'object');
assertEq(dbg.addDebuggee(g), gw);

// The Debugger.Objects passed to onNewGlobalObject are the global itself
// without any cross-compartment wrappers.
// NOTE: They also ignore any WindowProxy that may be associated with global.
assertEq(gw.unwrap(), gw);
