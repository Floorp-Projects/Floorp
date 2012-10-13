// Debugger.prototype.findAllGlobals finds ALL the globals!

var g1 = newGlobal();           // Created before the Debugger; debuggee.
var g2 = newGlobal();           // Created before the Debugger; not debuggee.

var dbg = new Debugger;

var g3 = newGlobal();           // Created after the Debugger; debuggee.
var g4 = newGlobal();           // Created after the Debugger; not debuggee.

var g1w = dbg.addDebuggee(g1);
var g3w = dbg.addDebuggee(g3);

var a = dbg.findAllGlobals();

// Get Debugger.Objects viewing the globals from their own compartments;
// this is the sort that findAllGlobals and addDebuggee return.
var g2w = g1w.makeDebuggeeValue(g2).unwrap();
var g4w = g1w.makeDebuggeeValue(g4).unwrap();
var thisw = g1w.makeDebuggeeValue(this).unwrap();

// Check that they're all there.
assertEq(a.indexOf(g1w) != -1, true);
assertEq(a.indexOf(g2w) != -1, true);
assertEq(a.indexOf(g3w) != -1, true);
assertEq(a.indexOf(g4w) != -1, true);
assertEq(a.indexOf(thisw) != -1, true);
