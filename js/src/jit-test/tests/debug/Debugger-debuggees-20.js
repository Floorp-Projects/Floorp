// addAllGlobalsAsDebuggees adds all the globals as debuggees.

var g1 = newGlobal({newCompartment: true});           // Created before the Debugger; debuggee.
var g2 = newGlobal({newCompartment: true});           // Created before the Debugger; not debuggee.

var dbg = new Debugger;

var g3 = newGlobal({newCompartment: true});           // Created after the Debugger; debuggee.
var g4 = newGlobal({newCompartment: true});           // Created after the Debugger; not debuggee.

var g1w = dbg.addDebuggee(g1);
var g3w = dbg.addDebuggee(g3);
assertEq(dbg.addAllGlobalsAsDebuggees(), undefined);

// Get Debugger.Objects viewing the WindowProxies from their own compartments.
assertEq(g1w.makeDebuggeeValue(g1), g3w.makeDebuggeeValue(g1).unwrap());
assertEq(g3w.makeDebuggeeValue(g3), g1w.makeDebuggeeValue(g3).unwrap());

var g2w = g1w.makeDebuggeeValue(g2).unwrap();
var g4w = g1w.makeDebuggeeValue(g4).unwrap();

var thisw = g1w.makeDebuggeeValue(this).unwrap();

// Check that they're all there.
assertEq(dbg.hasDebuggee(g1w), true);
assertEq(dbg.hasDebuggee(g2w), true);
assertEq(dbg.hasDebuggee(g3w), true);
assertEq(dbg.hasDebuggee(g4w), true);
// The debugger's global is not a debuggee.
assertEq(dbg.hasDebuggee(thisw), false);
